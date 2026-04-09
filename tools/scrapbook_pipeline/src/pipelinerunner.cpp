#include "pipelinerunner.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QSet>
#include <QTextStream>
#include <QElapsedTimer>

#include <atomic>
#include <exception>
#include <functional>
#include <algorithm>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

struct Options {
  QString inputDir;
  QString workDir;
  QString toolPath;
  QString config = QStringLiteral("Debug");
  QString assetStoreDir;
  QString logicalStoreDir;
  int maxWidth = 1024;
  int maxHeight = 1024;
  int padding = 2;
  QString splitMode = QStringLiteral("components");
  int componentAlphaThreshold = 1;
  int minComponentPixels = 16;
  int splitComponentsMaxCount = 8;
  double splitComponentsMaxSizeRatio = 2.0;
  double similarityReviewMinScore = 0.90;
  double similarityAutoMinScore = 0.92;
  int similarityAutoMaxLuminanceDistance = 8;
  int similarityAutoMaxAlphaDistance = 8;
  double similarityAutoMaxAspectRatioDelta = 0.10;
  double similarityAutoMinDimensionRatio = 0.90;
  int similarityReportMaxPairs = 500;
  int jobs = 0;
};

struct Component {
  QRect bbox;
  int pixelCount = 0;
};

struct OccurrenceRecord {
  QString sourceImage;
  QString itemName;
  QString exactId;
  QString trimmedImage;
};

struct LogicalGroup {
  QString logicalId;
  QString representativeExactId;
  QString representativeName;
  QString representativeSourceImage;
  QString representativeTrimmedImage;
  QStringList memberExactIds;
  int occurrenceCount = 0;
  QStringList fixtures;
  QStringList sourceImages;
  bool mergedBySimilarity = false;
  bool mergedByReview = false;
};

struct SourceResult {
  int sourceIndex = -1;
  QJsonObject summaryItem;
  QList<QJsonObject> packItems;
  QList<QPair<QString, QString>> duplicateEntries;
  int extractionCount = 0;
  int deduplicatedOutputs = 0;
  double durationSeconds = 0.0;
};

struct PackInputAnalysis {
  int requiredWidth = 0;
  int requiredHeight = 0;
};

QString executableName(const QString& baseName) {
#ifdef Q_OS_WIN
  return baseName + QStringLiteral(".exe");
#else
  return baseName;
#endif
}

QString nowStamp() {
  return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

void logLine(const QString& level, const QString& message) {
  static std::mutex logMutex;
  std::lock_guard<std::mutex> lock(logMutex);
  QTextStream stream(stdout);
  stream << nowStamp() << " [" << level << "] " << message << Qt::endl;
}

QString formatDuration(double seconds) {
  if (seconds < 60.0) {
    return QStringLiteral("%1s").arg(QString::number(seconds, 'f', 2));
  }
  const int hours = int(seconds / 3600.0);
  seconds -= hours * 3600.0;
  const int minutes = int(seconds / 60.0);
  seconds -= minutes * 60.0;
  if (hours > 0) {
    return QStringLiteral("%1h%2m%3s")
        .arg(hours)
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(QString::number(seconds, 'f', 1));
  }
  return QStringLiteral("%1m%2s").arg(minutes).arg(QString::number(seconds, 'f', 1));
}

int resolveJobCount(int requestedJobs, int sourceCount) {
  if (requestedJobs < 0) {
    throw std::runtime_error("--jobs must be at least 1.");
  }
  if (sourceCount <= 0) {
    return 1;
  }
  const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
  const int effectiveJobs = requestedJobs > 0 ? requestedJobs : int(hardwareThreads);
  return std::max(1, std::min(effectiveJobs, sourceCount));
}

PackInputAnalysis analyzePackInputs(const QList<QJsonObject>& packItems, int padding) {
  PackInputAnalysis analysis;
  for (const QJsonObject& item : packItems) {
    const QString imagePath = item.value(QStringLiteral("image")).toString();
    if (imagePath.isEmpty()) {
      continue;
    }
    QImageReader reader(imagePath);
    const QSize size = reader.size();
    if (!size.isValid()) {
      QImage image = reader.read();
      if (image.isNull()) {
        throw std::runtime_error(
            QStringLiteral("Could not inspect pack image %1: %2").arg(imagePath, reader.errorString()).toStdString());
      }
      analysis.requiredWidth = std::max(analysis.requiredWidth, image.width() + (padding * 2));
      analysis.requiredHeight = std::max(analysis.requiredHeight, image.height() + (padding * 2));
      continue;
    }

    analysis.requiredWidth = std::max(analysis.requiredWidth, size.width() + (padding * 2));
    analysis.requiredHeight = std::max(analysis.requiredHeight, size.height() + (padding * 2));
  }
  return analysis;
}

QString normalizedPath(QString path) {
  path = QDir::fromNativeSeparators(path.trimmed());
  return path.isEmpty() ? QString{} : QFileInfo(path).absoluteFilePath();
}

QString sanitizeName(const QString& value) {
  QString sanitized;
  sanitized.reserve(value.size());
  for (const QChar character : value) {
    sanitized.append(character.isLetterOrNumber() || character == QLatin1Char('-') || character == QLatin1Char('_')
                         ? character
                         : QLatin1Char('_'));
  }
  return sanitized.isEmpty() ? QStringLiteral("item") : sanitized;
}

QString canonicalPairKey(const QString& left, const QString& right) {
  return left <= right ? left + QLatin1Char('\n') + right : right + QLatin1Char('\n') + left;
}

QPair<QString, QString> decodePairKey(const QString& value) {
  const int separator = value.indexOf(QLatin1Char('\n'));
  return separator < 0 ? qMakePair(value, QString{}) : qMakePair(value.left(separator), value.mid(separator + 1));
}

QStringList sortedUnique(QStringList values) {
  values.removeDuplicates();
  std::sort(values.begin(), values.end());
  return values;
}

QJsonObject readJsonObject(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    throw std::runtime_error(QStringLiteral("Could not open %1").arg(path).toStdString());
  }
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    throw std::runtime_error(QStringLiteral("Expected JSON object in %1").arg(path).toStdString());
  }
  return document.object();
}

void writeJson(const QString& path, const QJsonValue& value) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    throw std::runtime_error(QStringLiteral("Could not write %1").arg(path).toStdString());
  }
  if (value.isArray()) {
    file.write(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented));
  } else {
    file.write(QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented));
  }
}

QString detectToolPath(const Options& options) {
  if (!options.toolPath.isEmpty()) {
    return options.toolPath;
  }

  const QString binaryName = executableName(QStringLiteral("libatlas_tool"));
  const QStringList candidates{
      QDir(QCoreApplication::applicationDirPath()).filePath(binaryName),
      QDir(QDir::currentPath()).filePath(QStringLiteral("build/bin/%1/%2").arg(options.config, binaryName)),
      QDir(QDir::currentPath()).filePath(QStringLiteral("build/bin/%1").arg(binaryName)),
  };
  for (const QString& candidate : candidates) {
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  throw std::runtime_error("Could not find libatlas_tool.");
}

void runProcess(const QString& program, const QStringList& arguments) {
  QProcess process;
  process.start(program, arguments);
  if (!process.waitForStarted()) {
    throw std::runtime_error(QStringLiteral("Could not start %1").arg(program).toStdString());
  }
  process.waitForFinished(-1);
  const QString stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
  const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
  if (!stdoutText.isEmpty()) {
    logLine(QStringLiteral("INFO"), stdoutText);
  }
  if (!stderrText.isEmpty()) {
    logLine(QStringLiteral("INFO"), stderrText);
  }
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    throw std::runtime_error(QStringLiteral("Command failed: %1").arg(program).toStdString());
  }
}

quint8 byteAt(const QByteArray& bytes, int offset) {
  if (offset < 0 || offset >= bytes.size()) {
    throw std::runtime_error("Unexpected end of image data.");
  }
  return static_cast<quint8>(bytes.at(offset));
}

quint16 readU16(const QByteArray& bytes, int offset) {
  return quint16(byteAt(bytes, offset)) | (quint16(byteAt(bytes, offset + 1)) << 8);
}

quint32 readU32(const QByteArray& bytes, int offset) {
  return quint32(byteAt(bytes, offset)) | (quint32(byteAt(bytes, offset + 1)) << 8) |
         (quint32(byteAt(bytes, offset + 2)) << 16) | (quint32(byteAt(bytes, offset + 3)) << 24);
}

quint64 readU48(const QByteArray& bytes, int offset) {
  quint64 value = 0;
  for (int index = 0; index < 6; ++index) {
    value |= quint64(byteAt(bytes, offset + index)) << (index * 8);
  }
  return value;
}

QByteArray readFileBytes(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    throw std::runtime_error(QStringLiteral("Could not open %1").arg(path).toStdString());
  }
  return file.readAll();
}

quint8 scaleChannel(quint32 pixel, quint32 mask, quint8 defaultValue = 255) {
  if (mask == 0) {
    return defaultValue;
  }

  quint32 shiftedMask = mask;
  int shift = 0;
  while ((shiftedMask & 1u) == 0u) {
    shiftedMask >>= 1;
    ++shift;
  }

  quint32 maxValue = shiftedMask;
  quint32 value = (pixel & mask) >> shift;
  return maxValue == 0 ? defaultValue : quint8((value * 255u + (maxValue / 2u)) / maxValue);
}

struct Rgba8 {
  quint8 r = 0;
  quint8 g = 0;
  quint8 b = 0;
  quint8 a = 255;
};

Rgba8 decodeRgb565(quint16 value) {
  return {
      quint8((((value >> 11) & 0x1f) * 255u + 15u) / 31u),
      quint8((((value >> 5) & 0x3f) * 255u + 31u) / 63u),
      quint8(((value & 0x1f) * 255u + 15u) / 31u),
      255,
  };
}

Rgba8 mixColors(const Rgba8& left, int leftWeight, const Rgba8& right, int rightWeight, int divisor) {
  return {
      quint8(((int(left.r) * leftWeight) + (int(right.r) * rightWeight)) / divisor),
      quint8(((int(left.g) * leftWeight) + (int(right.g) * rightWeight)) / divisor),
      quint8(((int(left.b) * leftWeight) + (int(right.b) * rightWeight)) / divisor),
      quint8(((int(left.a) * leftWeight) + (int(right.a) * rightWeight)) / divisor),
  };
}

void setPixelRgba(QImage& image, int x, int y, const Rgba8& color) {
  if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
    return;
  }
  uchar* pixel = image.scanLine(y) + (x * 4);
  pixel[0] = color.r;
  pixel[1] = color.g;
  pixel[2] = color.b;
  pixel[3] = color.a;
}

QImage loadDdsFallback(const QString& path) {
  const QByteArray bytes = readFileBytes(path);
  if (bytes.size() < 128 || bytes.left(4) != "DDS ") {
    throw std::runtime_error("Invalid DDS header.");
  }
  if (readU32(bytes, 4) != 124 || readU32(bytes, 76) != 32) {
    throw std::runtime_error("Unsupported DDS header size.");
  }

  const int height = int(readU32(bytes, 12));
  const int width = int(readU32(bytes, 16));
  const quint32 pixelFormatFlags = readU32(bytes, 80);
  const QByteArray fourCC = bytes.mid(84, 4);
  const quint32 rgbBitCount = readU32(bytes, 88);
  const quint32 redMask = readU32(bytes, 92);
  const quint32 greenMask = readU32(bytes, 96);
  const quint32 blueMask = readU32(bytes, 100);
  const quint32 alphaMask = readU32(bytes, 104);

  if (width <= 0 || height <= 0) {
    throw std::runtime_error("DDS image has invalid dimensions.");
  }

  QImage image(width, height, QImage::Format_RGBA8888);
  if (image.isNull()) {
    throw std::runtime_error("Could not allocate image buffer.");
  }

  const int dataOffset = 128;
  if ((pixelFormatFlags & 0x4u) != 0u) {
    const int blockSize = fourCC == "DXT1" ? 8 : (fourCC == "DXT5" ? 16 : 0);
    if (blockSize == 0) {
      throw std::runtime_error(QStringLiteral("Unsupported DDS compression format: %1")
                                   .arg(QString::fromLatin1(fourCC))
                                   .toStdString());
    }

    const int blockCountX = (width + 3) / 4;
    const int blockCountY = (height + 3) / 4;
    const qsizetype requiredBytes = qsizetype(blockCountX) * qsizetype(blockCountY) * qsizetype(blockSize);
    if ((bytes.size() - dataOffset) < requiredBytes) {
      throw std::runtime_error("DDS file is truncated.");
    }

    int offset = dataOffset;
    for (int blockY = 0; blockY < blockCountY; ++blockY) {
      for (int blockX = 0; blockX < blockCountX; ++blockX) {
        quint8 alphaPalette[8]{};
        quint64 alphaIndices = 0;
        if (fourCC == "DXT5") {
          const quint8 alpha0 = byteAt(bytes, offset);
          const quint8 alpha1 = byteAt(bytes, offset + 1);
          alphaPalette[0] = alpha0;
          alphaPalette[1] = alpha1;
          if (alpha0 > alpha1) {
            for (int index = 1; index <= 6; ++index) {
              alphaPalette[index + 1] = quint8((((7 - index) * int(alpha0)) + (index * int(alpha1))) / 7);
            }
          } else {
            for (int index = 1; index <= 4; ++index) {
              alphaPalette[index + 1] = quint8((((5 - index) * int(alpha0)) + (index * int(alpha1))) / 5);
            }
            alphaPalette[6] = 0;
            alphaPalette[7] = 255;
          }
          alphaIndices = readU48(bytes, offset + 2);
          offset += 8;
        }

        const quint16 color0Value = readU16(bytes, offset);
        const quint16 color1Value = readU16(bytes, offset + 2);
        const quint32 colorIndices = readU32(bytes, offset + 4);
        offset += 8;

        Rgba8 palette[4]{};
        palette[0] = decodeRgb565(color0Value);
        palette[1] = decodeRgb565(color1Value);
        if (fourCC == "DXT1" && color0Value <= color1Value) {
          palette[2] = mixColors(palette[0], 1, palette[1], 1, 2);
          palette[3] = {0, 0, 0, 0};
        } else {
          palette[2] = mixColors(palette[0], 2, palette[1], 1, 3);
          palette[3] = mixColors(palette[0], 1, palette[1], 2, 3);
        }

        for (int pixelIndex = 0; pixelIndex < 16; ++pixelIndex) {
          Rgba8 color = palette[(colorIndices >> (pixelIndex * 2)) & 0x3u];
          if (fourCC == "DXT5") {
            color.a = alphaPalette[(alphaIndices >> (pixelIndex * 3)) & 0x7u];
          }
          const int x = (blockX * 4) + (pixelIndex % 4);
          const int y = (blockY * 4) + (pixelIndex / 4);
          setPixelRgba(image, x, y, color);
        }
      }
    }

    return image;
  }

  if ((pixelFormatFlags & 0x40u) == 0u || (rgbBitCount != 24u && rgbBitCount != 32u)) {
    throw std::runtime_error("Unsupported DDS pixel format.");
  }

  const int bytesPerPixel = int(rgbBitCount / 8u);
  const qsizetype requiredBytes = qsizetype(width) * qsizetype(height) * qsizetype(bytesPerPixel);
  if ((bytes.size() - dataOffset) < requiredBytes) {
    throw std::runtime_error("DDS file is truncated.");
  }

  const uchar* source = reinterpret_cast<const uchar*>(bytes.constData() + dataOffset);
  for (int y = 0; y < height; ++y) {
    uchar* destination = image.scanLine(y);
    for (int x = 0; x < width; ++x) {
      quint32 pixel = 0;
      const qsizetype pixelOffset = (qsizetype(y) * width + x) * bytesPerPixel;
      for (int index = 0; index < bytesPerPixel; ++index) {
        pixel |= quint32(source[pixelOffset + index]) << (index * 8);
      }
      destination[(x * 4) + 0] = scaleChannel(pixel, redMask, 0);
      destination[(x * 4) + 1] = scaleChannel(pixel, greenMask, 0);
      destination[(x * 4) + 2] = scaleChannel(pixel, blueMask, 0);
      destination[(x * 4) + 3] = scaleChannel(pixel, alphaMask, 255);
    }
  }

  return image;
}

QImage loadTgaFallback(const QString& path) {
  const QByteArray bytes = readFileBytes(path);
  if (bytes.size() < 18) {
    throw std::runtime_error("Invalid TGA header.");
  }

  const int idLength = int(byteAt(bytes, 0));
  const int colorMapType = int(byteAt(bytes, 1));
  const int imageType = int(byteAt(bytes, 2));
  const int width = int(readU16(bytes, 12));
  const int height = int(readU16(bytes, 14));
  const int bitsPerPixel = int(byteAt(bytes, 16));
  const quint8 descriptor = byteAt(bytes, 17);
  if (colorMapType != 0) {
    throw std::runtime_error("Color-mapped TGA images are not supported.");
  }
  if (imageType != 2 && imageType != 10) {
    throw std::runtime_error("Unsupported TGA image type.");
  }
  if (bitsPerPixel != 24 && bitsPerPixel != 32) {
    throw std::runtime_error("Unsupported TGA pixel depth.");
  }
  if (width <= 0 || height <= 0) {
    throw std::runtime_error("TGA image has invalid dimensions.");
  }

  QImage image(width, height, QImage::Format_RGBA8888);
  if (image.isNull()) {
    throw std::runtime_error("Could not allocate image buffer.");
  }

  const int bytesPerPixel = bitsPerPixel / 8;
  int offset = 18 + idLength;
  const bool topOrigin = (descriptor & 0x20u) != 0u;
  const bool rightOrigin = (descriptor & 0x10u) != 0u;

  auto writePixel = [&](int linearIndex, const uchar* sourcePixel) {
    const int sourceX = linearIndex % width;
    const int sourceY = linearIndex / width;
    const int targetX = rightOrigin ? (width - 1 - sourceX) : sourceX;
    const int targetY = topOrigin ? sourceY : (height - 1 - sourceY);
    uchar* destination = image.scanLine(targetY) + (targetX * 4);
    destination[0] = sourcePixel[2];
    destination[1] = sourcePixel[1];
    destination[2] = sourcePixel[0];
    destination[3] = bytesPerPixel == 4 ? sourcePixel[3] : 255;
  };

  const int pixelCount = width * height;
  if (imageType == 2) {
    const qsizetype requiredBytes = qsizetype(pixelCount) * qsizetype(bytesPerPixel);
    if ((bytes.size() - offset) < requiredBytes) {
      throw std::runtime_error("TGA file is truncated.");
    }
    const uchar* source = reinterpret_cast<const uchar*>(bytes.constData() + offset);
    for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
      writePixel(pixelIndex, source + (pixelIndex * bytesPerPixel));
    }
    return image;
  }

  int pixelIndex = 0;
  while (pixelIndex < pixelCount) {
    const quint8 packetHeader = byteAt(bytes, offset++);
    const int runLength = int(packetHeader & 0x7fu) + 1;
    if ((packetHeader & 0x80u) != 0u) {
      if ((bytes.size() - offset) < bytesPerPixel) {
        throw std::runtime_error("TGA file is truncated.");
      }
      const uchar* sourcePixel = reinterpret_cast<const uchar*>(bytes.constData() + offset);
      offset += bytesPerPixel;
      for (int count = 0; count < runLength && pixelIndex < pixelCount; ++count) {
        writePixel(pixelIndex++, sourcePixel);
      }
      continue;
    }

    const qsizetype requiredBytes = qsizetype(runLength) * qsizetype(bytesPerPixel);
    if ((bytes.size() - offset) < requiredBytes) {
      throw std::runtime_error("TGA file is truncated.");
    }
    const uchar* source = reinterpret_cast<const uchar*>(bytes.constData() + offset);
    offset += int(requiredBytes);
    for (int count = 0; count < runLength && pixelIndex < pixelCount; ++count) {
      writePixel(pixelIndex++, source + (count * bytesPerPixel));
    }
  }

  return image;
}

QImage loadRgba(const QString& path) {
  QImageReader reader(path);
  QImage image = reader.read();
  if (image.isNull()) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    try {
      if (suffix == QStringLiteral("dds")) {
        return loadDdsFallback(path);
      }
      if (suffix == QStringLiteral("tga")) {
        return loadTgaFallback(path);
      }
    } catch (const std::exception& exception) {
      throw std::runtime_error(
          QStringLiteral("Could not decode %1: %2").arg(path, QString::fromLocal8Bit(exception.what())).toStdString());
    }
    throw std::runtime_error(QStringLiteral("Could not decode %1: %2").arg(path, reader.errorString()).toStdString());
  }
  return image.convertToFormat(QImage::Format_RGBA8888);
}

QRect alphaBounds(const QImage& image) {
  int minX = image.width();
  int minY = image.height();
  int maxX = -1;
  int maxY = -1;
  for (int y = 0; y < image.height(); ++y) {
    const uchar* scanLine = image.constScanLine(y);
    for (int x = 0; x < image.width(); ++x) {
      if (scanLine[(x * 4) + 3] == 0) {
        continue;
      }
      minX = std::min(minX, x);
      minY = std::min(minY, y);
      maxX = std::max(maxX, x);
      maxY = std::max(maxY, y);
    }
  }
  return maxX < 0 ? QRect(0, 0, image.width(), image.height())
                  : QRect(minX, minY, (maxX - minX) + 1, (maxY - minY) + 1);
}

QVector<Component> detectComponents(const QImage& image, int alphaThreshold, int minComponentPixels) {
  QVector<Component> components;
  QVector<uchar> alpha(image.width() * image.height());
  QVector<bool> visited(alpha.size(), false);
  for (int y = 0; y < image.height(); ++y) {
    const uchar* scanLine = image.constScanLine(y);
    for (int x = 0; x < image.width(); ++x) {
      alpha[(y * image.width()) + x] = scanLine[(x * 4) + 3];
    }
  }

  QVector<QPoint> queue;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const int index = (y * image.width()) + x;
      if (visited[index] || alpha[index] <= alphaThreshold) {
        continue;
      }
      queue.clear();
      queue.push_back(QPoint(x, y));
      visited[index] = true;
      int head = 0;
      int minX = x;
      int maxX = x;
      int minY = y;
      int maxY = y;
      int pixelCount = 0;
      while (head < queue.size()) {
        const QPoint current = queue.at(head++);
        ++pixelCount;
        minX = std::min(minX, current.x());
        maxX = std::max(maxX, current.x());
        minY = std::min(minY, current.y());
        maxY = std::max(maxY, current.y());
        for (int ny = std::max(0, current.y() - 1); ny <= std::min(image.height() - 1, current.y() + 1); ++ny) {
          for (int nx = std::max(0, current.x() - 1); nx <= std::min(image.width() - 1, current.x() + 1); ++nx) {
            const int neighborIndex = (ny * image.width()) + nx;
            if (visited[neighborIndex] || alpha[neighborIndex] <= alphaThreshold) {
              continue;
            }
            visited[neighborIndex] = true;
            queue.push_back(QPoint(nx, ny));
          }
        }
      }
      if (pixelCount >= minComponentPixels) {
        components.push_back(Component{QRect(minX, minY, (maxX - minX) + 1, (maxY - minY) + 1), pixelCount});
      }
    }
  }

  std::sort(components.begin(), components.end(), [](const Component& left, const Component& right) {
    return left.bbox.top() == right.bbox.top() ? left.bbox.left() < right.bbox.left()
                                               : left.bbox.top() < right.bbox.top();
  });
  return components;
}

bool shouldSplitComponents(const QVector<Component>& components, int maxCount, double maxSizeRatio) {
  if (components.size() <= 1 || components.size() > maxCount) {
    return false;
  }
  int minWidth = components.front().bbox.width();
  int maxWidth = minWidth;
  int minHeight = components.front().bbox.height();
  int maxHeight = minHeight;
  for (const Component& component : components) {
    minWidth = std::min(minWidth, component.bbox.width());
    maxWidth = std::max(maxWidth, component.bbox.width());
    minHeight = std::min(minHeight, component.bbox.height());
    maxHeight = std::max(maxHeight, component.bbox.height());
  }
  return (double(maxWidth) / double(minWidth) <= maxSizeRatio) &&
         (double(maxHeight) / double(minHeight) <= maxSizeRatio);
}

QVector<Component> chooseComponents(const Options& options, const QVector<Component>& components) {
  if (options.splitMode == QStringLiteral("bbox")) {
    return {};
  }
  if (options.splitMode == QStringLiteral("components")) {
    return components.size() > 1 ? components : QVector<Component>{};
  }
  if (options.splitMode == QStringLiteral("auto")) {
    return shouldSplitComponents(components, options.splitComponentsMaxCount, options.splitComponentsMaxSizeRatio)
               ? components
               : QVector<Component>{};
  }
  throw std::runtime_error(QStringLiteral("Unsupported split mode: %1").arg(options.splitMode).toStdString());
}

QJsonObject makeUvRect(const QRect& bounds, int width, int height) {
  return {
      {QStringLiteral("x_min"), double(bounds.left()) / double(width)},
      {QStringLiteral("x_max"), double(bounds.right() + 1) / double(width)},
      {QStringLiteral("y_min"), double(bounds.top()) / double(height)},
      {QStringLiteral("y_max"), double(bounds.bottom() + 1) / double(height)},
  };
}

QJsonArray buildRequestItems(const QString& atlasName, const QSize& size, const QRect& alphaBox, const QVector<Component>& components) {
  QJsonArray items;
  if (!components.isEmpty()) {
    for (int index = 0; index < components.size(); ++index) {
      items.append(QJsonObject{
          {QStringLiteral("name"), QStringLiteral("%1_%2").arg(atlasName).arg(index, 2, 10, QLatin1Char('0'))},
          {QStringLiteral("uv"), makeUvRect(components.at(index).bbox, size.width(), size.height())},
      });
    }
    return items;
  }

  items.append(QJsonObject{
      {QStringLiteral("name"), atlasName},
      {QStringLiteral("uv"), makeUvRect(alphaBox, size.width(), size.height())},
  });
  return items;
}

bool dedupeIdenticalOutputs(QJsonObject& item) {
  const QString croppedImage = item.value(QStringLiteral("cropped_image")).toString();
  const QString trimmedImage = item.value(QStringLiteral("trimmed_image")).toString();
  QJsonObject metadata = item.value(QStringLiteral("metadata")).toObject();
  if (croppedImage.isEmpty() || trimmedImage.isEmpty()) {
    return false;
  }

  const bool sameDimensions =
      metadata.value(QStringLiteral("cropped_width")).toInt() == metadata.value(QStringLiteral("trimmed_width")).toInt() &&
      metadata.value(QStringLiteral("cropped_height")).toInt() == metadata.value(QStringLiteral("trimmed_height")).toInt();
  const QJsonObject trimmedRect = metadata.value(QStringLiteral("trimmed_rect_in_crop")).toObject();
  if (!sameDimensions || trimmedRect.value(QStringLiteral("x")).toInt() != 0 || trimmedRect.value(QStringLiteral("y")).toInt() != 0 ||
      trimmedRect.value(QStringLiteral("width")).toInt() != metadata.value(QStringLiteral("cropped_width")).toInt() ||
      trimmedRect.value(QStringLiteral("height")).toInt() != metadata.value(QStringLiteral("cropped_height")).toInt()) {
    return false;
  }

  QFile croppedFile(croppedImage);
  QFile trimmedFile(trimmedImage);
  if (!croppedFile.open(QIODevice::ReadOnly) || !trimmedFile.open(QIODevice::ReadOnly)) {
    return false;
  }
  if (croppedFile.readAll() != trimmedFile.readAll()) {
    return false;
  }
  trimmedFile.remove();
  item.insert(QStringLiteral("trimmed_image"), croppedImage);
  return true;
}

SourceResult processSource(const Options& options, const QString& toolPath, const QFileInfo& source, int sourceIndex, int sourceCount) {
  QElapsedTimer totalTimer;
  totalTimer.start();
  const QString atlasName = source.completeBaseName();
  const QString logPrefix =
      QStringLiteral("%1/%2 %3").arg(sourceIndex + 1).arg(sourceCount).arg(source.fileName());

  QElapsedTimer analysisTimer;
  analysisTimer.start();
  const QImage image = loadRgba(source.absoluteFilePath());
  const QRect alphaBox = alphaBounds(image);
  const QVector<Component> detectedComponents =
      detectComponents(image, options.componentAlphaThreshold, options.minComponentPixels);
  const QVector<Component> components = chooseComponents(options, detectedComponents);
  const double analysisSeconds = double(analysisTimer.elapsed()) / 1000.0;

  const QString convertedDir = QDir(options.workDir).filePath(QStringLiteral("converted_png"));
  const QString requestDir = QDir(options.workDir).filePath(QStringLiteral("requests"));
  const QString extractRoot = QDir(options.workDir).filePath(QStringLiteral("extract"));
  const QString metadataDir = QDir(options.workDir).filePath(QStringLiteral("metadata/extract"));
  const QString extractOutputDir = QDir(extractRoot).filePath(atlasName);
  QDir().mkpath(convertedDir);
  QDir().mkpath(requestDir);
  QDir().mkpath(extractRoot);
  QDir().mkpath(metadataDir);
  QDir().mkpath(extractOutputDir);

  const QString convertedPath = QDir(convertedDir).filePath(atlasName + QStringLiteral(".png"));
  const QString requestPath = QDir(requestDir).filePath(atlasName + QStringLiteral(".json"));
  const QString extractMetadataPath = QDir(metadataDir).filePath(atlasName + QStringLiteral(".json"));
  const QJsonArray requestItems = buildRequestItems(atlasName, image.size(), alphaBox, components);
  const QString extractionStrategy = components.isEmpty() ? QStringLiteral("bbox") : QStringLiteral("components");

  image.save(convertedPath);
  writeJson(requestPath, QJsonObject{
      {QStringLiteral("atlas_identifier"), atlasName},
      {QStringLiteral("items"), requestItems},
  });

  logLine(
      QStringLiteral("INFO"),
      QStringLiteral("%1: extracting %2 item(s) from %3x%4 atlas after %5 analysis (detected %6 component(s), strategy=%7)")
          .arg(logPrefix)
          .arg(requestItems.size())
          .arg(image.width())
          .arg(image.height())
          .arg(formatDuration(analysisSeconds))
          .arg(detectedComponents.size())
          .arg(extractionStrategy));

  QStringList extractArguments{
      QStringLiteral("extract"),
      QStringLiteral("--atlas"),
      convertedPath,
      QStringLiteral("--requests"),
      requestPath,
      QStringLiteral("--output-dir"),
      extractOutputDir,
      QStringLiteral("--metadata"),
      extractMetadataPath,
      QStringLiteral("--origin"),
      QStringLiteral("top-left"),
      QStringLiteral("--rounding"),
      QStringLiteral("nearest"),
  };
  if (!options.assetStoreDir.isEmpty()) {
    extractArguments << QStringLiteral("--asset-store") << options.assetStoreDir;
  }
  QElapsedTimer extractTimer;
  extractTimer.start();
  runProcess(toolPath, extractArguments);
  const double extractSeconds = double(extractTimer.elapsed()) / 1000.0;

  QElapsedTimer postprocessTimer;
  postprocessTimer.start();
  QJsonObject extractMetadata = readJsonObject(extractMetadataPath);
  QJsonArray items = extractMetadata.value(QStringLiteral("items")).toArray();
  int deduplicatedOutputs = 0;
  QList<QJsonObject> packItems;
  QList<QPair<QString, QString>> duplicateEntries;
  QJsonArray summaryItems;
  for (int index = 0; index < items.size(); ++index) {
    QJsonObject item = items.at(index).toObject();
    if (dedupeIdenticalOutputs(item)) {
      ++deduplicatedOutputs;
    }
    const QJsonObject metadata = item.value(QStringLiteral("metadata")).toObject();
    const QString name = item.value(QStringLiteral("name")).toString();
    const QString exactId = metadata.value(QStringLiteral("exact_id")).toString();
    duplicateEntries.push_back({exactId, name});
    const QString trimmedImage = item.value(QStringLiteral("trimmed_image")).toString();
    if (!trimmedImage.isEmpty() && metadata.value(QStringLiteral("trimmed_width")).toInt() > 0 &&
        metadata.value(QStringLiteral("trimmed_height")).toInt() > 0) {
      packItems.push_back(QJsonObject{
          {QStringLiteral("entry_id"), sanitizeName(name)},
          {QStringLiteral("image"), trimmedImage},
          {QStringLiteral("source_label"), name},
      });
    }
    summaryItems.append(QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("uv"), metadata.value(QStringLiteral("requested_uv_rect")).toObject()},
        {QStringLiteral("cropped_image"), item.value(QStringLiteral("cropped_image")).toString()},
        {QStringLiteral("trimmed_image"), trimmedImage},
        {QStringLiteral("exact_id"), exactId},
        {QStringLiteral("warnings"), metadata.value(QStringLiteral("warnings")).toArray()},
    });
    items[index] = item;
  }
  extractMetadata.insert(QStringLiteral("items"), items);
  writeJson(extractMetadataPath, extractMetadata);
  const double totalSeconds = double(totalTimer.elapsed()) / 1000.0;

  logLine(
      QStringLiteral("INFO"),
      QStringLiteral("%1: finished in %2 (%3 extracted, %4 identical pairs collapsed, extract=%5)")
          .arg(logPrefix)
          .arg(formatDuration(totalSeconds))
          .arg(items.size())
          .arg(deduplicatedOutputs)
          .arg(formatDuration(extractSeconds)));

  return SourceResult{
      sourceIndex,
      QJsonObject{
          {QStringLiteral("source_image"), source.absoluteFilePath()},
          {QStringLiteral("converted_png"), convertedPath},
          {QStringLiteral("request_json"), requestPath},
          {QStringLiteral("extract_metadata_json"), extractMetadataPath},
          {QStringLiteral("width"), image.width()},
          {QStringLiteral("height"), image.height()},
          {QStringLiteral("detected_component_count"), detectedComponents.size()},
          {QStringLiteral("extraction_strategy"), extractionStrategy},
          {QStringLiteral("timing_seconds"),
           QJsonObject{
               {QStringLiteral("analysis"), analysisSeconds},
               {QStringLiteral("extract"), extractSeconds},
               {QStringLiteral("postprocess"), double(postprocessTimer.elapsed()) / 1000.0},
               {QStringLiteral("total"), totalSeconds},
           }},
          {QStringLiteral("items"), summaryItems},
      },
      packItems,
      duplicateEntries,
      static_cast<int>(items.size()),
      deduplicatedOutputs,
      totalSeconds,
  };
}

QList<OccurrenceRecord> collectOccurrences(const QJsonArray& fixtures) {
  QList<OccurrenceRecord> occurrences;
  for (const QJsonValue& fixtureValue : fixtures) {
    const QJsonObject fixture = fixtureValue.toObject();
    const QString sourceImage = fixture.value(QStringLiteral("source_image")).toString();
    for (const QJsonValue& itemValue : fixture.value(QStringLiteral("items")).toArray()) {
      const QJsonObject item = itemValue.toObject();
      const QString exactId = item.value(QStringLiteral("exact_id")).toString();
      if (exactId.isEmpty()) {
        continue;
      }
      occurrences.push_back(OccurrenceRecord{
          sourceImage,
          item.value(QStringLiteral("name")).toString(),
          exactId,
          item.value(QStringLiteral("trimmed_image")).toString(),
      });
    }
  }
  return occurrences;
}

QList<LogicalGroup> buildLogicalGroups(const QList<OccurrenceRecord>& occurrences, const QJsonObject& similarityReport) {
  QHash<QString, QList<OccurrenceRecord>> occurrencesByExactId;
  for (const OccurrenceRecord& occurrence : occurrences) {
    occurrencesByExactId[occurrence.exactId].push_back(occurrence);
  }

  QSet<QString> assignedExactIds;
  QList<LogicalGroup> logicalGroups;
  for (const QJsonValue& componentValue : similarityReport.value(QStringLiteral("auto_duplicate_components")).toArray()) {
    QStringList exactIds;
    for (const QJsonValue& exactIdValue : componentValue.toObject().value(QStringLiteral("member_exact_ids")).toArray()) {
      const QString exactId = exactIdValue.toString();
      if (occurrencesByExactId.contains(exactId)) {
        exactIds.push_back(exactId);
      }
    }
    exactIds = sortedUnique(exactIds);
    if (exactIds.size() < 2) {
      continue;
    }
    for (const QString& exactId : exactIds) {
      assignedExactIds.insert(exactId);
    }
    QString representativeExactId = exactIds.front();
    int bestCount = -1;
    for (const QString& exactId : exactIds) {
      const int count = occurrencesByExactId.value(exactId).size();
      if (count > bestCount || (count == bestCount && exactId < representativeExactId)) {
        representativeExactId = exactId;
        bestCount = count;
      }
    }
    QList<OccurrenceRecord> memberOccurrences;
    for (const QString& exactId : exactIds) {
      memberOccurrences.append(occurrencesByExactId.value(exactId));
    }
    std::sort(memberOccurrences.begin(), memberOccurrences.end(), [](const OccurrenceRecord& left, const OccurrenceRecord& right) {
      return left.itemName < right.itemName;
    });
    logicalGroups.push_back(LogicalGroup{
        sanitizeName(exactIds.front()),
        representativeExactId,
        memberOccurrences.front().itemName,
        memberOccurrences.front().sourceImage,
        memberOccurrences.front().trimmedImage,
        exactIds,
        static_cast<int>(memberOccurrences.size()),
        sortedUnique([&]() {
          QStringList values;
          for (const OccurrenceRecord& occurrence : memberOccurrences) {
            values.push_back(occurrence.itemName);
          }
          return values;
        }()),
        sortedUnique([&]() {
          QStringList values;
          for (const OccurrenceRecord& occurrence : memberOccurrences) {
            values.push_back(occurrence.sourceImage);
          }
          return values;
        }()),
        true,
        false,
    });
  }

  QStringList exactIds = occurrencesByExactId.keys();
  std::sort(exactIds.begin(), exactIds.end());
  for (const QString& exactId : exactIds) {
    if (assignedExactIds.contains(exactId)) {
      continue;
    }
    QList<OccurrenceRecord> exactOccurrences = occurrencesByExactId.value(exactId);
    std::sort(exactOccurrences.begin(), exactOccurrences.end(), [](const OccurrenceRecord& left, const OccurrenceRecord& right) {
      return left.itemName < right.itemName;
    });
    logicalGroups.push_back(LogicalGroup{
        sanitizeName(exactId),
        exactId,
        exactOccurrences.front().itemName,
        exactOccurrences.front().sourceImage,
        exactOccurrences.front().trimmedImage,
        QStringList{exactId},
        static_cast<int>(exactOccurrences.size()),
        sortedUnique([&]() {
          QStringList values;
          for (const OccurrenceRecord& occurrence : exactOccurrences) {
            values.push_back(occurrence.itemName);
          }
          return values;
        }()),
        sortedUnique([&]() {
          QStringList values;
          for (const OccurrenceRecord& occurrence : exactOccurrences) {
            values.push_back(occurrence.sourceImage);
          }
          return values;
        }()),
        false,
        false,
    });
  }

  std::sort(logicalGroups.begin(), logicalGroups.end(), [](const LogicalGroup& left, const LogicalGroup& right) {
    return left.logicalId < right.logicalId;
  });
  return logicalGroups;
}

QPair<QHash<QString, QString>, QSet<QString>> loadReviewDecisions(const QString& logicalStoreDir) {
  QHash<QString, QString> aliases;
  QSet<QString> distinctPairs;
  const QDir groupsDir(QDir(logicalStoreDir).filePath(QStringLiteral("review_candidates/groups")));
  QDirIterator iterator(groupsDir.absolutePath(), {QStringLiteral("decision.json")}, QDir::Files, QDirIterator::Subdirectories);
  while (iterator.hasNext()) {
    const QString decisionPath = iterator.next();
    const QJsonObject payload = readJsonObject(decisionPath);
    const QJsonObject payloadAliases = payload.value(QStringLiteral("aliases")).toObject();
    for (auto aliasIt = payloadAliases.begin(); aliasIt != payloadAliases.end(); ++aliasIt) {
      aliases.insert(aliasIt.key(), aliasIt.value().toString());
    }
    for (const QJsonValue& pairValue : payload.value(QStringLiteral("distinct_pairs")).toArray()) {
      const QJsonArray pair = pairValue.toArray();
      if (pair.size() == 2) {
        distinctPairs.insert(canonicalPairKey(pair.at(0).toString(), pair.at(1).toString()));
      }
    }
  }
  return {aliases, distinctPairs};
}

QPair<QList<LogicalGroup>, QHash<QString, QString>> applyReviewAliases(
    const QList<LogicalGroup>& logicalGroups,
    const QHash<QString, QString>& reviewAliases) {
  if (reviewAliases.isEmpty()) {
    return {logicalGroups, {}};
  }

  QHash<QString, LogicalGroup> groupsById;
  for (const LogicalGroup& group : logicalGroups) {
    groupsById.insert(group.logicalId, group);
  }

  QHash<QString, QString> validAliases;
  for (auto aliasIt = reviewAliases.begin(); aliasIt != reviewAliases.end(); ++aliasIt) {
    if (groupsById.contains(aliasIt.key()) && groupsById.contains(aliasIt.value())) {
      validAliases.insert(aliasIt.key(), aliasIt.value());
    }
  }

  std::function<QString(const QString&)> resolve = [&](const QString& logicalId) -> QString {
    QSet<QString> seen;
    QString current = logicalId;
    while (validAliases.contains(current)) {
      if (seen.contains(current)) {
        throw std::runtime_error("Cycle detected in review aliases.");
      }
      seen.insert(current);
      current = validAliases.value(current);
    }
    return current;
  };

  QHash<QString, QList<LogicalGroup>> groupedByRoot;
  QHash<QString, QString> resolvedAliases;
  for (const LogicalGroup& group : logicalGroups) {
    const QString root = resolve(group.logicalId);
    groupedByRoot[root].push_back(group);
    if (root != group.logicalId) {
      resolvedAliases.insert(group.logicalId, root);
    }
  }

  QList<LogicalGroup> mergedGroups;
  QStringList roots = groupedByRoot.keys();
  std::sort(roots.begin(), roots.end());
  for (const QString& root : roots) {
    const QList<LogicalGroup> componentGroups = groupedByRoot.value(root);
    const LogicalGroup rootGroup = groupsById.value(root);
    if (componentGroups.size() == 1) {
      mergedGroups.push_back(rootGroup);
      continue;
    }
    QStringList memberExactIds;
    QStringList fixtures;
    QStringList sourceImages;
    int occurrenceCount = 0;
    bool mergedBySimilarity = false;
    for (const LogicalGroup& group : componentGroups) {
      memberExactIds.append(group.memberExactIds);
      fixtures.append(group.fixtures);
      sourceImages.append(group.sourceImages);
      occurrenceCount += group.occurrenceCount;
      mergedBySimilarity = mergedBySimilarity || group.mergedBySimilarity;
    }
    mergedGroups.push_back(LogicalGroup{
        rootGroup.logicalId,
        rootGroup.representativeExactId,
        rootGroup.representativeName,
        rootGroup.representativeSourceImage,
        rootGroup.representativeTrimmedImage,
        sortedUnique(memberExactIds),
        occurrenceCount,
        sortedUnique(fixtures),
        sortedUnique(sourceImages),
        mergedBySimilarity,
        true,
    });
  }

  return {mergedGroups, resolvedAliases};
}

QSet<QString> normalizeDistinctPairs(
    const QList<LogicalGroup>& logicalGroups,
    const QSet<QString>& reviewDistinctPairs,
    const QHash<QString, QString>& appliedReviewAliases) {
  QSet<QString> validLogicalIds;
  for (const LogicalGroup& group : logicalGroups) {
    validLogicalIds.insert(group.logicalId);
  }

  std::function<QString(const QString&)> resolve = [&](const QString& logicalId) -> QString {
    QString current = logicalId;
    QSet<QString> seen;
    while (appliedReviewAliases.contains(current)) {
      if (seen.contains(current)) {
        throw std::runtime_error("Cycle detected while resolving distinct review pairs.");
      }
      seen.insert(current);
      current = appliedReviewAliases.value(current);
    }
    return current;
  };

  QSet<QString> normalizedPairs;
  for (const QString& pairKey : reviewDistinctPairs) {
    const auto pair = decodePairKey(pairKey);
    const QString left = resolve(pair.first);
    const QString right = resolve(pair.second);
    if (left == right || !validLogicalIds.contains(left) || !validLogicalIds.contains(right)) {
      continue;
    }
    normalizedPairs.insert(canonicalPairKey(left, right));
  }
  return normalizedPairs;
}

void linkOrCopy(const QString& source, const QString& destination) {
  QDir().mkpath(QFileInfo(destination).absolutePath());
  if (QFileInfo::exists(destination)) {
    QFile::remove(destination);
  }
#ifdef Q_OS_WIN
  if (!QFile::copy(source, destination)) {
    throw std::runtime_error(QStringLiteral("Could not copy %1 to %2").arg(source, destination).toStdString());
  }
#else
  QFile sourceFile(source);
  if (!sourceFile.link(destination) && !QFile::copy(source, destination)) {
    throw std::runtime_error(QStringLiteral("Could not copy %1 to %2").arg(source, destination).toStdString());
  }
#endif
}

void createContactSheet(const QJsonArray& members, const QString& outputPath) {
  if (members.isEmpty()) {
    return;
  }
  const QSize thumbSize(180, 180);
  const int columns = 4;
  const int labelHeight = 54;
  const int cellWidth = thumbSize.width() + 16;
  const int cellHeight = thumbSize.height() + labelHeight + 16;
  const int rows = (members.size() + columns - 1) / columns;
  QImage sheet(columns * cellWidth, rows * cellHeight, QImage::Format_ARGB32_Premultiplied);
  sheet.fill(QColor(250, 250, 250));
  QPainter painter(&sheet);
  painter.setPen(QColor(40, 40, 40));
  for (int index = 0; index < members.size(); ++index) {
    const QJsonObject member = members.at(index).toObject();
    const QString imagePath = member.value(QStringLiteral("review_image")).toString();
    if (imagePath.isEmpty()) {
      continue;
    }
    const QImage source = loadRgba(imagePath);
    const QImage tile = source.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int column = index % columns;
    const int row = index / columns;
    const QRect frame(column * cellWidth + 4, row * cellHeight + 4, cellWidth - 8, cellHeight - 8);
    painter.drawRect(frame);
    const QPoint imageOrigin(frame.left() + 4 + ((thumbSize.width() - tile.width()) / 2),
                             frame.top() + 4 + ((thumbSize.height() - tile.height()) / 2));
    painter.drawImage(imageOrigin, tile);
    painter.drawText(QRect(frame.left() + 8, frame.top() + thumbSize.height() + 10, frame.width() - 16, labelHeight),
                     Qt::TextWordWrap,
                     member.value(QStringLiteral("representative_name")).toString() + QLatin1Char('\n') +
                         member.value(QStringLiteral("logical_id")).toString().right(12));
  }
  painter.end();
  QDir().mkpath(QFileInfo(outputPath).absolutePath());
  sheet.save(outputPath);
}

QString makeReviewGroupId(const QStringList& logicalIds, const QString& representativeName) {
  return QStringLiteral("group__%1_items__%2__%3")
      .arg(logicalIds.size(), 2, 10, QLatin1Char('0'))
      .arg(sanitizeName(representativeName), logicalIds.front());
}

QString makeReviewImageFileName(int index, const QString& representativeName) {
  return QStringLiteral("%1__%2.png")
      .arg(index + 1, 2, 10, QLatin1Char('0'))
      .arg(sanitizeName(representativeName).left(24));
}

QStringList buildReviewGroupsFromCandidatePairs(
    const QJsonArray& reviewCandidates,
    const QHash<QString, QString>& logicalIdByExactId,
    const QSet<QString>& excludedPairs) {
  QHash<QString, QString> parent;
  std::function<QString(const QString&)> find = [&](const QString& value) -> QString {
    const QString root = parent.value(value, value);
    if (root == value) {
      return root;
    }
    const QString resolved = find(root);
    parent.insert(value, resolved);
    return resolved;
  };
  auto unite = [&](const QString& left, const QString& right) {
    const QString leftRoot = find(left);
    const QString rightRoot = find(right);
    if (leftRoot != rightRoot) {
      parent.insert(rightRoot, leftRoot);
    }
  };

  QSet<QString> reviewLogicalIds;
  for (const QJsonValue& value : reviewCandidates) {
    const QJsonObject candidate = value.toObject();
    const QString leftExactId = candidate.value(QStringLiteral("left")).toObject().value(QStringLiteral("exact_id")).toString();
    const QString rightExactId = candidate.value(QStringLiteral("right")).toObject().value(QStringLiteral("exact_id")).toString();
    const QString leftLogicalId = logicalIdByExactId.value(leftExactId);
    const QString rightLogicalId = logicalIdByExactId.value(rightExactId);
    if (leftLogicalId.isEmpty() || rightLogicalId.isEmpty() || leftLogicalId == rightLogicalId ||
        excludedPairs.contains(canonicalPairKey(leftLogicalId, rightLogicalId))) {
      continue;
    }
    reviewLogicalIds.insert(leftLogicalId);
    reviewLogicalIds.insert(rightLogicalId);
    unite(leftLogicalId, rightLogicalId);
  }

  QMap<QString, QStringList> grouped;
  for (const QString& logicalId : reviewLogicalIds) {
    grouped[find(logicalId)].push_back(logicalId);
  }

  QStringList groups;
  for (QStringList logicalIds : grouped) {
    logicalIds = sortedUnique(logicalIds);
    if (logicalIds.size() > 1) {
      groups.push_back(logicalIds.join(QLatin1Char('\t')));
    }
  }
  std::sort(groups.begin(), groups.end());
  return groups;
}

QPair<QList<QJsonObject>, QJsonArray> materializeLogicalStore(
    const QList<LogicalGroup>& logicalGroups,
    const QString& logicalStoreDir) {
  const QString imagesDir = QDir(logicalStoreDir).filePath(QStringLiteral("images"));
  const QString metadataDir = QDir(logicalStoreDir).filePath(QStringLiteral("metadata"));
  QDir().mkpath(imagesDir);
  QDir().mkpath(metadataDir);

  QSet<QString> expectedFiles;
  for (const LogicalGroup& group : logicalGroups) {
    expectedFiles.insert(group.logicalId + QStringLiteral(".png"));
  }
  const QFileInfoList existingImages = QDir(imagesDir).entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Name);
  for (const QFileInfo& fileInfo : existingImages) {
    if (!expectedFiles.contains(fileInfo.fileName())) {
      QFile::remove(fileInfo.absoluteFilePath());
    }
  }

  QList<QJsonObject> packItems;
  QJsonArray metadataItems;
  for (const LogicalGroup& group : logicalGroups) {
    const QString imagePath = QDir(imagesDir).filePath(group.logicalId + QStringLiteral(".png"));
    const bool existedBeforeRun = QFileInfo::exists(imagePath);
    if (!existedBeforeRun && !group.representativeTrimmedImage.isEmpty()) {
      QFile::copy(group.representativeTrimmedImage, imagePath);
    }
    if (QFileInfo::exists(imagePath)) {
      packItems.push_back(QJsonObject{
          {QStringLiteral("entry_id"), group.logicalId},
          {QStringLiteral("image"), imagePath},
          {QStringLiteral("source_label"), group.representativeName},
      });
    }
    metadataItems.append(QJsonObject{
        {QStringLiteral("logical_id"), group.logicalId},
        {QStringLiteral("representative_exact_id"), group.representativeExactId},
        {QStringLiteral("representative_name"), group.representativeName},
        {QStringLiteral("representative_source_image"), group.representativeSourceImage},
        {QStringLiteral("representative_trimmed_image"), group.representativeTrimmedImage},
        {QStringLiteral("logical_image"), QFileInfo::exists(imagePath) ? imagePath : QString()},
        {QStringLiteral("logical_image_existed_before_run"), existedBeforeRun},
        {QStringLiteral("member_exact_ids"), QJsonArray::fromStringList(group.memberExactIds)},
        {QStringLiteral("occurrence_count"), group.occurrenceCount},
        {QStringLiteral("fixtures"), QJsonArray::fromStringList(group.fixtures)},
        {QStringLiteral("source_images"), QJsonArray::fromStringList(group.sourceImages)},
        {QStringLiteral("merged_by_similarity"), group.mergedBySimilarity},
        {QStringLiteral("merged_by_review"), group.mergedByReview},
    });
  }
  return {packItems, metadataItems};
}

QJsonObject materializeReviewCandidates(
    const QJsonObject& similarityReport,
    const QList<LogicalGroup>& logicalGroups,
    const QString& logicalStoreDir,
    const QSet<QString>& distinctPairs) {
  const QString reviewRoot = QDir(logicalStoreDir).filePath(QStringLiteral("review_candidates"));
  const QString groupsDir = QDir(reviewRoot).filePath(QStringLiteral("groups"));
  QHash<QString, QJsonObject> preservedDecisions;
  QDirIterator previousDecisionIterator(groupsDir, {QStringLiteral("decision.json")}, QDir::Files, QDirIterator::Subdirectories);
  while (previousDecisionIterator.hasNext()) {
    const QJsonObject decision = readJsonObject(previousDecisionIterator.next());
    preservedDecisions.insert(decision.value(QStringLiteral("group_id")).toString(), decision);
  }
  if (QFileInfo::exists(reviewRoot)) {
    QDir(reviewRoot).removeRecursively();
  }
  QDir().mkpath(groupsDir);

  QHash<QString, LogicalGroup> logicalGroupById;
  QHash<QString, QString> logicalIdByExactId;
  for (const LogicalGroup& group : logicalGroups) {
    logicalGroupById.insert(group.logicalId, group);
    for (const QString& exactId : group.memberExactIds) {
      logicalIdByExactId.insert(exactId, group.logicalId);
    }
  }

  QStringList reviewGroupKeys;
  const int omittedPairCount = similarityReport.value(QStringLiteral("review_candidate_omitted_count")).toInt();
  if (omittedPairCount == 0) {
    reviewGroupKeys = buildReviewGroupsFromCandidatePairs(
        similarityReport.value(QStringLiteral("review_candidates")).toArray(),
        logicalIdByExactId,
        distinctPairs);
  } else {
    for (const QJsonValue& componentValue : similarityReport.value(QStringLiteral("review_components")).toArray()) {
      QStringList logicalIds;
      for (const QJsonValue& exactIdValue : componentValue.toObject().value(QStringLiteral("member_exact_ids")).toArray()) {
        const QString logicalId = logicalIdByExactId.value(exactIdValue.toString());
        if (!logicalId.isEmpty()) {
          logicalIds.push_back(logicalId);
        }
      }
      logicalIds = sortedUnique(logicalIds);
      if (logicalIds.size() > 1 && !(logicalIds.size() == 2 && distinctPairs.contains(canonicalPairKey(logicalIds.at(0), logicalIds.at(1))))) {
        reviewGroupKeys.push_back(logicalIds.join(QLatin1Char('\t')));
      }
    }
    std::sort(reviewGroupKeys.begin(), reviewGroupKeys.end());
  }

  int exportedImageCount = 0;
  QJsonArray manifestGroups;
  for (const QString& groupKey : reviewGroupKeys) {
    const QStringList logicalIds = groupKey.split(QLatin1Char('\t'));
    const QString groupName = makeReviewGroupId(logicalIds, logicalGroupById.value(logicalIds.front()).representativeName);
    logLine(QStringLiteral("INFO"), QStringLiteral("Preparing review group %1").arg(groupName));
    const QString groupDir = QDir(groupsDir).filePath(groupName);
    const QString imagesDir = QDir(groupDir).filePath(QStringLiteral("images"));
    QDir().mkpath(imagesDir);

    QStringList memberExactIds;
    QStringList fixtures;
    QStringList sourceImages;
    QJsonArray memberRecords;
    for (int index = 0; index < logicalIds.size(); ++index) {
      const LogicalGroup group = logicalGroupById.value(logicalIds.at(index));
      const QString sourceLogicalImage = QDir(logicalStoreDir).filePath(QStringLiteral("images/%1.png").arg(group.logicalId));
      QString reviewImagePath;
      if (QFileInfo::exists(sourceLogicalImage)) {
        reviewImagePath = QDir(imagesDir).filePath(makeReviewImageFileName(index, group.representativeName));
        linkOrCopy(sourceLogicalImage, reviewImagePath);
        ++exportedImageCount;
      }
      memberRecords.append(QJsonObject{
          {QStringLiteral("logical_id"), group.logicalId},
          {QStringLiteral("representative_name"), group.representativeName},
          {QStringLiteral("representative_exact_id"), group.representativeExactId},
          {QStringLiteral("representative_source_image"), group.representativeSourceImage},
          {QStringLiteral("review_image"), reviewImagePath},
          {QStringLiteral("member_exact_ids"), QJsonArray::fromStringList(group.memberExactIds)},
          {QStringLiteral("occurrence_count"), group.occurrenceCount},
          {QStringLiteral("fixtures"), QJsonArray::fromStringList(group.fixtures)},
          {QStringLiteral("source_images"), QJsonArray::fromStringList(group.sourceImages)},
          {QStringLiteral("merged_by_similarity"), group.mergedBySimilarity},
          {QStringLiteral("merged_by_review"), group.mergedByReview},
      });
      memberExactIds.append(group.memberExactIds);
      fixtures.append(group.fixtures);
      sourceImages.append(group.sourceImages);
    }

    const QString contactSheetPath = QDir(groupDir).filePath(QStringLiteral("contact_sheet.png"));
    logLine(QStringLiteral("INFO"), QStringLiteral("Creating contact sheet for %1").arg(groupName));
    createContactSheet(memberRecords, contactSheetPath);

    QJsonObject decision = preservedDecisions.value(groupName);
    QJsonObject filteredAliases;
    const QJsonObject decisionAliases = decision.value(QStringLiteral("aliases")).toObject();
    for (auto aliasIt = decisionAliases.begin(); aliasIt != decisionAliases.end(); ++aliasIt) {
      if (logicalIds.contains(aliasIt.key()) && logicalIds.contains(aliasIt.value().toString()) && aliasIt.key() != aliasIt.value().toString()) {
        filteredAliases.insert(aliasIt.key(), aliasIt.value().toString());
      }
    }
    QJsonArray filteredDistinctPairs;
    for (const QJsonValue& pairValue : decision.value(QStringLiteral("distinct_pairs")).toArray()) {
      const QJsonArray pair = pairValue.toArray();
      if (pair.size() == 2 && logicalIds.contains(pair.at(0).toString()) && logicalIds.contains(pair.at(1).toString())) {
        filteredDistinctPairs.append(pair);
      }
    }
    const QJsonObject decisionPayload{
        {QStringLiteral("group_id"), groupName},
        {QStringLiteral("status"), decision.value(QStringLiteral("status")).toString(QStringLiteral("pending"))},
        {QStringLiteral("notes"), decision.value(QStringLiteral("notes")).toString()},
        {QStringLiteral("aliases"), filteredAliases},
        {QStringLiteral("distinct_pairs"), filteredDistinctPairs},
        {QStringLiteral("available_logical_ids"), QJsonArray::fromStringList(logicalIds)},
    };
    writeJson(QDir(groupDir).filePath(QStringLiteral("decision.json")), decisionPayload);

    const QJsonObject groupManifest{
        {QStringLiteral("group_id"), groupName},
        {QStringLiteral("logical_id_count"), logicalIds.size()},
        {QStringLiteral("member_exact_id_count"), sortedUnique(memberExactIds).size()},
        {QStringLiteral("member_occurrence_count"), [&]() {
           int total = 0;
           for (const QJsonValue& memberValue : memberRecords) {
             total += memberValue.toObject().value(QStringLiteral("occurrence_count")).toInt();
           }
           return total;
         }()},
        {QStringLiteral("fixtures"), QJsonArray::fromStringList(sortedUnique(fixtures))},
        {QStringLiteral("source_images"), QJsonArray::fromStringList(sortedUnique(sourceImages))},
        {QStringLiteral("contact_sheet"), contactSheetPath},
        {QStringLiteral("members"), memberRecords},
    };
    writeJson(QDir(groupDir).filePath(QStringLiteral("group.json")), groupManifest);
    manifestGroups.append(QJsonObject{
        {QStringLiteral("group_id"), groupName},
        {QStringLiteral("path"), groupDir},
        {QStringLiteral("contact_sheet"), contactSheetPath},
        {QStringLiteral("decision_json"), QDir(groupDir).filePath(QStringLiteral("decision.json"))},
        {QStringLiteral("logical_id_count"), groupManifest.value(QStringLiteral("logical_id_count")).toInt()},
        {QStringLiteral("member_exact_id_count"), groupManifest.value(QStringLiteral("member_exact_id_count")).toInt()},
        {QStringLiteral("member_occurrence_count"), groupManifest.value(QStringLiteral("member_occurrence_count")).toInt()},
        {QStringLiteral("fixtures"), groupManifest.value(QStringLiteral("fixtures")).toArray()},
        {QStringLiteral("source_images"), groupManifest.value(QStringLiteral("source_images")).toArray()},
    });
  }

  const QJsonObject manifest{
      {QStringLiteral("review_candidate_pair_count"), similarityReport.value(QStringLiteral("review_candidate_count")).toInt()},
      {QStringLiteral("review_component_count"), manifestGroups.size()},
      {QStringLiteral("review_candidate_image_count"), exportedImageCount},
      {QStringLiteral("review_groups"), manifestGroups},
  };
  writeJson(QDir(reviewRoot).filePath(QStringLiteral("review_groups.json")), manifest);
  return manifest;
}

QJsonArray buildOccurrenceRemap(
    const QList<OccurrenceRecord>& occurrences,
    const QList<LogicalGroup>& logicalGroups,
    const QJsonObject& packMetadata) {
  QHash<QString, QString> logicalIdByExactId;
  for (const LogicalGroup& group : logicalGroups) {
    for (const QString& exactId : group.memberExactIds) {
      logicalIdByExactId.insert(exactId, group.logicalId);
    }
  }

  QHash<int, QString> atlasIdentifierByIndex;
  const QJsonArray atlases = packMetadata.value(QStringLiteral("atlases")).toArray();
  for (int index = 0; index < atlases.size(); ++index) {
    atlasIdentifierByIndex.insert(index, atlases.at(index).toObject().value(QStringLiteral("atlas_identifier")).toString());
  }

  QHash<QString, QJsonObject> placementByEntryId;
  for (const QJsonValue& placementValue : packMetadata.value(QStringLiteral("placements")).toArray()) {
    const QJsonObject placement = placementValue.toObject();
    placementByEntryId.insert(placement.value(QStringLiteral("entry_id")).toString(), placement);
  }

  QJsonArray remap;
  for (const OccurrenceRecord& occurrence : occurrences) {
    const QString logicalId = logicalIdByExactId.value(occurrence.exactId);
    const QJsonObject placement = placementByEntryId.value(logicalId);
    if (logicalId.isEmpty() || placement.isEmpty()) {
      continue;
    }
    remap.append(QJsonObject{
        {QStringLiteral("source_image"), occurrence.sourceImage},
        {QStringLiteral("occurrence_name"), occurrence.itemName},
        {QStringLiteral("exact_id"), occurrence.exactId},
        {QStringLiteral("logical_id"), logicalId},
        {QStringLiteral("atlas_index"), placement.value(QStringLiteral("atlas_index")).toInt()},
        {QStringLiteral("atlas_identifier"), atlasIdentifierByIndex.value(placement.value(QStringLiteral("atlas_index")).toInt())},
        {QStringLiteral("pixel_rect"), placement.value(QStringLiteral("pixel_rect")).toObject()},
        {QStringLiteral("uv_rect"), placement.value(QStringLiteral("uv_rect")).toObject()},
    });
  }
  return remap;
}

Options parseOptions(const QStringList& arguments) {
  QCommandLineParser parser;
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
  parser.addHelpOption();
  const QList<QCommandLineOption> options{
      {{QStringLiteral("input-dir")}, QStringLiteral("Input atlas directory."), QStringLiteral("path")},
      {{QStringLiteral("work-dir")}, QStringLiteral("Pipeline work directory."), QStringLiteral("path")},
      {{QStringLiteral("tool")}, QStringLiteral("Path to libatlas_tool."), QStringLiteral("path")},
      {{QStringLiteral("config")}, QStringLiteral("Build config for tool autodetection."), QStringLiteral("name"), QStringLiteral("Debug")},
      {{QStringLiteral("asset-store")}, QStringLiteral("Persistent asset store directory."), QStringLiteral("path")},
      {{QStringLiteral("logical-store")}, QStringLiteral("Persistent logical store directory."), QStringLiteral("path")},
      {{QStringLiteral("max-width")}, QStringLiteral("Packed atlas width."), QStringLiteral("value"), QStringLiteral("1024")},
      {{QStringLiteral("max-height")}, QStringLiteral("Packed atlas height."), QStringLiteral("value"), QStringLiteral("1024")},
      {{QStringLiteral("padding")}, QStringLiteral("Packed atlas padding."), QStringLiteral("value"), QStringLiteral("2")},
      {{QStringLiteral("split-mode")}, QStringLiteral("components, auto, or bbox."), QStringLiteral("value"), QStringLiteral("components")},
      {{QStringLiteral("component-alpha-threshold")}, QStringLiteral("Component alpha threshold."), QStringLiteral("value"), QStringLiteral("1")},
      {{QStringLiteral("min-component-pixels")}, QStringLiteral("Minimum pixels per component."), QStringLiteral("value"), QStringLiteral("16")},
      {{QStringLiteral("split-components-max-count")}, QStringLiteral("Auto split max component count."), QStringLiteral("value"), QStringLiteral("8")},
      {{QStringLiteral("split-components-max-size-ratio")}, QStringLiteral("Auto split max size ratio."), QStringLiteral("value"), QStringLiteral("2.0")},
      {{QStringLiteral("similarity-review-min-score")}, QStringLiteral("Review min score."), QStringLiteral("value"), QStringLiteral("0.90")},
      {{QStringLiteral("similarity-auto-min-score")}, QStringLiteral("Auto min score."), QStringLiteral("value"), QStringLiteral("0.92")},
      {{QStringLiteral("similarity-auto-max-luminance-distance")}, QStringLiteral("Auto luminance distance."), QStringLiteral("value"), QStringLiteral("8")},
      {{QStringLiteral("similarity-auto-max-alpha-distance")}, QStringLiteral("Auto alpha distance."), QStringLiteral("value"), QStringLiteral("8")},
      {{QStringLiteral("similarity-auto-max-aspect-ratio-delta")}, QStringLiteral("Auto aspect ratio delta."), QStringLiteral("value"), QStringLiteral("0.10")},
      {{QStringLiteral("similarity-auto-min-dimension-ratio")}, QStringLiteral("Auto min dimension ratio."), QStringLiteral("value"), QStringLiteral("0.90")},
      {{QStringLiteral("similarity-report-max-pairs")}, QStringLiteral("Report max pairs."), QStringLiteral("value"), QStringLiteral("500")},
      {{QStringLiteral("jobs")}, QStringLiteral("Number of source images to process concurrently."), QStringLiteral("value"), QStringLiteral("0")},
  };
  parser.addOptions(options);
  parser.process(arguments);

  Options optionsValue;
  optionsValue.inputDir = normalizedPath(parser.value(QStringLiteral("input-dir")));
  optionsValue.workDir = normalizedPath(parser.value(QStringLiteral("work-dir")));
  optionsValue.toolPath = normalizedPath(parser.value(QStringLiteral("tool")));
  optionsValue.config = parser.value(QStringLiteral("config"));
  optionsValue.assetStoreDir = normalizedPath(parser.value(QStringLiteral("asset-store")));
  optionsValue.logicalStoreDir = normalizedPath(parser.value(QStringLiteral("logical-store")));
  optionsValue.maxWidth = parser.value(QStringLiteral("max-width")).toInt();
  optionsValue.maxHeight = parser.value(QStringLiteral("max-height")).toInt();
  optionsValue.padding = parser.value(QStringLiteral("padding")).toInt();
  optionsValue.splitMode = parser.value(QStringLiteral("split-mode"));
  optionsValue.componentAlphaThreshold = parser.value(QStringLiteral("component-alpha-threshold")).toInt();
  optionsValue.minComponentPixels = parser.value(QStringLiteral("min-component-pixels")).toInt();
  optionsValue.splitComponentsMaxCount = parser.value(QStringLiteral("split-components-max-count")).toInt();
  optionsValue.splitComponentsMaxSizeRatio = parser.value(QStringLiteral("split-components-max-size-ratio")).toDouble();
  optionsValue.similarityReviewMinScore = parser.value(QStringLiteral("similarity-review-min-score")).toDouble();
  optionsValue.similarityAutoMinScore = parser.value(QStringLiteral("similarity-auto-min-score")).toDouble();
  optionsValue.similarityAutoMaxLuminanceDistance = parser.value(QStringLiteral("similarity-auto-max-luminance-distance")).toInt();
  optionsValue.similarityAutoMaxAlphaDistance = parser.value(QStringLiteral("similarity-auto-max-alpha-distance")).toInt();
  optionsValue.similarityAutoMaxAspectRatioDelta = parser.value(QStringLiteral("similarity-auto-max-aspect-ratio-delta")).toDouble();
  optionsValue.similarityAutoMinDimensionRatio = parser.value(QStringLiteral("similarity-auto-min-dimension-ratio")).toDouble();
  optionsValue.similarityReportMaxPairs = parser.value(QStringLiteral("similarity-report-max-pairs")).toInt();
  optionsValue.jobs = parser.value(QStringLiteral("jobs")).toInt();
  if (optionsValue.assetStoreDir.isEmpty()) {
    optionsValue.assetStoreDir = QDir(optionsValue.workDir).filePath(QStringLiteral("fixture_asset_store"));
  }
  if (optionsValue.logicalStoreDir.isEmpty()) {
    optionsValue.logicalStoreDir = QDir(optionsValue.workDir).filePath(QStringLiteral("fixture_logical_store"));
  }
  return optionsValue;
}

}  // namespace

int PipelineRunner::run(const QStringList& arguments) {
  try {
    QElapsedTimer totalTimer;
    totalTimer.start();
    const Options options = parseOptions(arguments);
    if (options.inputDir.isEmpty() || options.workDir.isEmpty()) {
      throw std::runtime_error("Both --input-dir and --work-dir are required.");
    }
    const QString toolPath = detectToolPath(options);
    const QStringList filters{QStringLiteral("*.dds"), QStringLiteral("*.DDS"), QStringLiteral("*.tga"), QStringLiteral("*.TGA"), QStringLiteral("*.png"), QStringLiteral("*.PNG")};
    const QFileInfoList sources = QDir(options.inputDir).entryInfoList(filters, QDir::Files, QDir::Name);
    if (sources.isEmpty()) {
      throw std::runtime_error("No supported input images found.");
    }

    QDir workDir(options.workDir);
    if (workDir.exists()) {
      workDir.removeRecursively();
    }
    QDir().mkpath(options.workDir);
    QDir().mkpath(options.logicalStoreDir);

    const int workerCount = resolveJobCount(options.jobs, sources.size());
    logLine(
        QStringLiteral("INFO"),
        QStringLiteral("Processing %1 source image(s) with %2 worker(s).").arg(sources.size()).arg(workerCount));

    QElapsedTimer sourceStageTimer;
    sourceStageTimer.start();
    const int progressInterval = sources.size() <= 20 ? 1 : 25;
    std::atomic<int> nextSourceIndex{0};
    std::atomic<int> completedCount{0};
    std::atomic<bool> stopRequested{false};
    std::mutex exceptionMutex;
    std::exception_ptr workerException;
    std::vector<std::optional<SourceResult>> sourceResults(size_t(sources.size()));
    std::vector<std::thread> workers;
    workers.reserve(size_t(workerCount));

    auto worker = [&]() {
      while (!stopRequested.load()) {
        const int sourceIndex = nextSourceIndex.fetch_add(1);
        if (sourceIndex >= sources.size()) {
          break;
        }
        try {
          sourceResults[size_t(sourceIndex)] = processSource(options, toolPath, sources.at(sourceIndex), sourceIndex, sources.size());
          const int completed = completedCount.fetch_add(1) + 1;
          if (completed == sources.size() || completed % progressInterval == 0) {
            logLine(QStringLiteral("INFO"), QStringLiteral("Completed %1/%2 source image(s).").arg(completed).arg(sources.size()));
          }
        } catch (...) {
          {
            std::lock_guard<std::mutex> lock(exceptionMutex);
            if (!workerException) {
              workerException = std::current_exception();
            }
          }
          stopRequested.store(true);
          break;
        }
      }
    };

    for (int workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
      workers.emplace_back(worker);
    }
    for (std::thread& thread : workers) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    if (workerException) {
      std::rethrow_exception(workerException);
    }

    QJsonArray summaryFixtures;
    QHash<QString, QStringList> duplicateGroups;
    int totalExtractions = 0;
    int deduplicatedOutputs = 0;
    for (int sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex) {
      const SourceResult& result = sourceResults[size_t(sourceIndex)].value();
      summaryFixtures.append(result.summaryItem);
      totalExtractions += result.extractionCount;
      deduplicatedOutputs += result.deduplicatedOutputs;
      for (const auto& duplicateEntry : result.duplicateEntries) {
        duplicateGroups[duplicateEntry.first].push_back(duplicateEntry.second);
      }
    }
    const double sourceStageSeconds = double(sourceStageTimer.elapsed()) / 1000.0;

    const QString metadataDir = QDir(options.workDir).filePath(QStringLiteral("metadata"));
    QDir().mkpath(metadataDir);
    const QString similaritySourceMapPath = QDir(metadataDir).filePath(QStringLiteral("similarity_source_map.json"));
    QJsonObject sourceMap;
    for (const QFileInfo& source : sources) {
      sourceMap.insert(source.completeBaseName(), source.absoluteFilePath());
    }
    writeJson(similaritySourceMapPath, sourceMap);

    const QString similarityReportPath = QDir(metadataDir).filePath(QStringLiteral("similarity_report.json"));
    logLine(QStringLiteral("INFO"), QStringLiteral("Building similarity report"));
    runProcess(toolPath, {
                             QStringLiteral("similarity-report"),
                             QStringLiteral("--metadata-dir"),
                             QDir(metadataDir).filePath(QStringLiteral("extract")),
                             QStringLiteral("--output"),
                             similarityReportPath,
                             QStringLiteral("--source-map"),
                             similaritySourceMapPath,
                             QStringLiteral("--review-min-score"),
                             QString::number(options.similarityReviewMinScore),
                             QStringLiteral("--auto-min-score"),
                             QString::number(options.similarityAutoMinScore),
                             QStringLiteral("--auto-max-luminance-distance"),
                             QString::number(options.similarityAutoMaxLuminanceDistance),
                             QStringLiteral("--auto-max-alpha-distance"),
                             QString::number(options.similarityAutoMaxAlphaDistance),
                             QStringLiteral("--auto-max-aspect-ratio-delta"),
                             QString::number(options.similarityAutoMaxAspectRatioDelta),
                             QStringLiteral("--auto-min-dimension-ratio"),
                             QString::number(options.similarityAutoMinDimensionRatio),
                             QStringLiteral("--max-pairs"),
                             QString::number(options.similarityReportMaxPairs),
                         });

    const QJsonObject similarityReport = readJsonObject(similarityReportPath);
    QList<OccurrenceRecord> occurrences = collectOccurrences(summaryFixtures);
    QList<LogicalGroup> logicalGroups = buildLogicalGroups(occurrences, similarityReport);
    const auto [reviewAliases, reviewDistinctPairs] = loadReviewDecisions(options.logicalStoreDir);
    const auto [aliasedGroups, appliedAliases] = applyReviewAliases(logicalGroups, reviewAliases);
    logicalGroups = aliasedGroups;
    const QSet<QString> normalizedDistinctPairs = normalizeDistinctPairs(logicalGroups, reviewDistinctPairs, appliedAliases);
    logLine(QStringLiteral("INFO"), QStringLiteral("Materializing logical store for %1 logical texture(s).").arg(logicalGroups.size()));
    const auto [logicalPackItems, logicalGroupMetadata] = materializeLogicalStore(logicalGroups, options.logicalStoreDir);
    writeJson(QDir(options.logicalStoreDir).filePath(QStringLiteral("metadata/logical_groups.json")),
              QJsonObject{{QStringLiteral("logical_groups"), logicalGroupMetadata}});
    logLine(QStringLiteral("INFO"), QStringLiteral("Materializing review candidates"));
    const QJsonObject reviewManifest = materializeReviewCandidates(similarityReport, logicalGroups, options.logicalStoreDir, normalizedDistinctPairs);

    const QString packManifestPath = QDir(metadataDir).filePath(QStringLiteral("pack_manifest.json"));
    QJsonArray packItemsJson;
    for (const QJsonObject& packItem : logicalPackItems) {
      packItemsJson.append(packItem);
    }
    writeJson(packManifestPath, QJsonObject{{QStringLiteral("items"), packItemsJson}});

    const QString packDir = QDir(options.workDir).filePath(QStringLiteral("packed"));
    const QString packMetadataPath = QDir(metadataDir).filePath(QStringLiteral("packed.json"));
    int effectiveMaxWidth = options.maxWidth;
    int effectiveMaxHeight = options.maxHeight;
    if (!logicalPackItems.isEmpty()) {
      const PackInputAnalysis packInputAnalysis = analyzePackInputs(logicalPackItems, options.padding);
      effectiveMaxWidth = std::max(options.maxWidth, packInputAnalysis.requiredWidth);
      effectiveMaxHeight = std::max(options.maxHeight, packInputAnalysis.requiredHeight);
      if (effectiveMaxWidth != options.maxWidth || effectiveMaxHeight != options.maxHeight) {
        logLine(
            QStringLiteral("WARN"),
            QStringLiteral("Increased pack atlas size from %1x%2 to %3x%4 to fit oversized logical image(s) once padding is applied.")
                .arg(options.maxWidth)
                .arg(options.maxHeight)
                .arg(effectiveMaxWidth)
                .arg(effectiveMaxHeight));
      }
      logLine(
          QStringLiteral("INFO"),
          QStringLiteral("Packing %1 logical image(s) with max atlas size %2x%3 and padding %4.")
              .arg(logicalPackItems.size())
              .arg(effectiveMaxWidth)
              .arg(effectiveMaxHeight)
              .arg(options.padding));
      runProcess(toolPath, {
                               QStringLiteral("pack"),
                               QStringLiteral("--manifest"),
                               packManifestPath,
                               QStringLiteral("--output-dir"),
                               packDir,
                               QStringLiteral("--metadata"),
                               packMetadataPath,
                               QStringLiteral("--atlas-prefix"),
                               QStringLiteral("fixtures"),
                               QStringLiteral("--max-width"),
                               QString::number(effectiveMaxWidth),
                               QStringLiteral("--max-height"),
                               QString::number(effectiveMaxHeight),
                               QStringLiteral("--padding"),
                               QString::number(options.padding),
                               QStringLiteral("--origin"),
                               QStringLiteral("top-left"),
                           });
    } else {
      writeJson(packMetadataPath, QJsonObject{{QStringLiteral("atlases"), QJsonArray{}}, {QStringLiteral("placements"), QJsonArray{}}});
    }

    const QJsonObject packMetadata = readJsonObject(packMetadataPath);
    writeJson(QDir(metadataDir).filePath(QStringLiteral("occurrence_remap.json")),
              QJsonObject{{QStringLiteral("occurrences"), buildOccurrenceRemap(occurrences, logicalGroups, packMetadata)}});

    QJsonArray duplicateSummary;
    QStringList duplicateKeys = duplicateGroups.keys();
    std::sort(duplicateKeys.begin(), duplicateKeys.end());
    for (const QString& exactId : duplicateKeys) {
      if (duplicateGroups.value(exactId).size() > 1) {
        duplicateSummary.append(QJsonObject{
            {QStringLiteral("exact_id"), exactId},
            {QStringLiteral("fixtures"), QJsonArray::fromStringList(sortedUnique(duplicateGroups.value(exactId)))},
        });
      }
    }

    writeJson(QDir(metadataDir).filePath(QStringLiteral("summary.json")),
              QJsonObject{
                  {QStringLiteral("tool"), toolPath},
                  {QStringLiteral("input_dir"), options.inputDir},
                  {QStringLiteral("work_dir"), options.workDir},
                  {QStringLiteral("logical_store_dir"), options.logicalStoreDir},
                  {QStringLiteral("worker_count"), workerCount},
                  {QStringLiteral("source_count"), sources.size()},
                  {QStringLiteral("extraction_count"), totalExtractions},
                  {QStringLiteral("logical_texture_count"), logicalGroups.size()},
                  {QStringLiteral("deduplicated_identical_output_count"), deduplicatedOutputs},
                  {QStringLiteral("requested_pack_max_width"), options.maxWidth},
                  {QStringLiteral("requested_pack_max_height"), options.maxHeight},
                  {QStringLiteral("effective_pack_max_width"), effectiveMaxWidth},
                  {QStringLiteral("effective_pack_max_height"), effectiveMaxHeight},
                  {QStringLiteral("timing_seconds"),
                   QJsonObject{
                       {QStringLiteral("source_processing"), sourceStageSeconds},
                       {QStringLiteral("total"), double(totalTimer.elapsed()) / 1000.0},
                   }},
                  {QStringLiteral("duplicate_exact_ids"), duplicateSummary},
                  {QStringLiteral("review_candidate_manifest_json"), QDir(options.logicalStoreDir).filePath(QStringLiteral("review_candidates/review_groups.json"))},
                  {QStringLiteral("similarity_report_json"), similarityReportPath},
                  {QStringLiteral("fixtures"), summaryFixtures},
                  {QStringLiteral("review_manifest"), reviewManifest},
              });

    logLine(QStringLiteral("INFO"), QStringLiteral("Processed %1 source image(s) into %2 extracted item(s) in %3.")
                                      .arg(sources.size())
                                      .arg(totalExtractions)
                                      .arg(formatDuration(double(totalTimer.elapsed()) / 1000.0)));
    return 0;
  } catch (const std::exception& exception) {
    QTextStream(stderr) << "error: " << exception.what() << Qt::endl;
    return 1;
  }
}
