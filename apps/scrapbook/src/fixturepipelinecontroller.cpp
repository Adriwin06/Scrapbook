#include "fixturepipelinecontroller.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <functional>

namespace {

QString executableName(const QString& baseName) {
#ifdef Q_OS_WIN
  return baseName + QStringLiteral(".exe");
#else
  return baseName;
#endif
}

QString canonicalPairKey(const QString& left, const QString& right) {
  return left <= right ? left + QLatin1Char('\n') + right : right + QLatin1Char('\n') + left;
}

QPair<QString, QString> decodePairKey(const QString& value) {
  const int separatorIndex = value.indexOf(QLatin1Char('\n'));
  if (separatorIndex < 0) {
    return {value, QString{}};
  }
  return {value.left(separatorIndex), value.mid(separatorIndex + 1)};
}

QString normalizePath(QString path) {
  path = QDir::fromNativeSeparators(path.trimmed());
  return path.isEmpty() ? QString{} : QDir(path).absolutePath();
}

QStringList stringListFromJson(const QJsonValue& value) {
  QStringList result;
  for (const QJsonValue& entry : value.toArray()) {
    if (entry.isString()) {
      result.push_back(entry.toString());
    }
  }
  return result;
}

}  // namespace

FixturePipelineController::FixturePipelineController(QObject* parent)
    : QObject(parent),
      m_reviewGroupModel(this),
      m_reviewMemberModel(this),
      m_settings(QStringLiteral("Adriwin06"), QStringLiteral("Scrapbook")) {
  m_repoRoot = findRepoRoot();
  m_sourceDirectory =
      normalizePath(m_settings.value(QStringLiteral("sourceDirectory"), defaultSourceDirectory()).toString());
  m_workspaceRoot =
      normalizePath(m_settings.value(QStringLiteral("workspaceRoot"), defaultWorkspaceRoot()).toString());
  m_toolPath = normalizePath(m_settings.value(QStringLiteral("toolPath")).toString());
  m_config = m_settings.value(QStringLiteral("config"), QStringLiteral("Debug")).toString();
  m_splitMode = m_settings.value(QStringLiteral("splitMode"), QStringLiteral("components")).toString();
  const bool splitModeMigratedToComponents =
      m_settings.value(QStringLiteral("splitModeMigratedToComponents"), false).toBool();
  if (!splitModeMigratedToComponents && m_splitMode == QStringLiteral("auto")) {
    m_splitMode = QStringLiteral("components");
    m_settings.setValue(QStringLiteral("splitMode"), m_splitMode);
  }
  m_settings.setValue(QStringLiteral("splitModeMigratedToComponents"), true);
  m_similarityReviewMinScore = m_settings.value(QStringLiteral("similarityReviewMinScore"), 0.90).toDouble();
  m_similarityAutoMinScore = m_settings.value(QStringLiteral("similarityAutoMinScore"), 0.92).toDouble();
  m_similarityMaxPairs = m_settings.value(QStringLiteral("similarityMaxPairs"), 20000).toInt();
  m_status = QStringLiteral("Idle");
  m_currentGroupTitle = QStringLiteral("No unresolved review groups.");
  m_pipelineProcess.setProcessChannelMode(QProcess::MergedChannels);

  connect(&m_pipelineProcess, &QProcess::readyRead, this, [this]() {
    appendLog(QString::fromLocal8Bit(m_pipelineProcess.readAll()));
  });
  connect(&m_pipelineProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    appendLog(QStringLiteral("Pipeline process error (%1): %2").arg(int(error)).arg(m_pipelineProcess.errorString()));
  });
  connect(
      &m_pipelineProcess,
      QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      this,
      [this](int exitCode, QProcess::ExitStatus exitStatus) {
        emit pipelineRunningChanged();
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
          setStatus(QStringLiteral("Pipeline finished"));
          refreshReviewGroups();
        } else {
          setStatus(QStringLiteral("Pipeline failed (%1)").arg(exitCode));
        }
      });

  refreshReviewGroups();
}

QString FixturePipelineController::sourceDirectory() const {
  return m_sourceDirectory;
}

QString FixturePipelineController::workspaceRoot() const {
  return m_workspaceRoot;
}

QString FixturePipelineController::toolPath() const {
  return m_toolPath;
}

QString FixturePipelineController::config() const {
  return m_config;
}

QString FixturePipelineController::splitMode() const {
  return m_splitMode;
}

double FixturePipelineController::similarityReviewMinScore() const {
  return m_similarityReviewMinScore;
}

double FixturePipelineController::similarityAutoMinScore() const {
  return m_similarityAutoMinScore;
}

int FixturePipelineController::similarityMaxPairs() const {
  return m_similarityMaxPairs;
}

QString FixturePipelineController::assetStoreDirectory() const {
  return QDir(m_workspaceRoot).filePath(QStringLiteral("fixture_asset_store"));
}

QString FixturePipelineController::logicalStoreDirectory() const {
  return QDir(m_workspaceRoot).filePath(QStringLiteral("fixture_logical_store"));
}

QString FixturePipelineController::pipelineWorkDirectory() const {
  return QDir(m_workspaceRoot).filePath(QStringLiteral("fixture_pipeline"));
}

QString FixturePipelineController::status() const {
  return m_status;
}

QString FixturePipelineController::logText() const {
  return m_logText;
}

bool FixturePipelineController::pipelineRunning() const {
  return m_pipelineProcess.state() != QProcess::NotRunning;
}

ReviewGroupModel* FixturePipelineController::reviewGroupModel() {
  return &m_reviewGroupModel;
}

ReviewMemberModel* FixturePipelineController::reviewMemberModel() {
  return &m_reviewMemberModel;
}

int FixturePipelineController::currentGroupIndex() const {
  return m_currentGroupIndex;
}

QString FixturePipelineController::currentGroupTitle() const {
  return m_currentGroupTitle;
}

QUrl FixturePipelineController::currentContactSheetUrl() const {
  return m_currentContactSheetUrl;
}

QString FixturePipelineController::decisionNotes() const {
  return m_decisionNotes;
}

bool FixturePipelineController::hasCurrentGroup() const {
  return !m_currentDecisionPath.isEmpty();
}

void FixturePipelineController::setSourceDirectory(const QString& sourceDirectory) {
  const QString normalized = normalizePath(sourceDirectory);
  if (m_sourceDirectory == normalized) {
    return;
  }
  m_sourceDirectory = normalized;
  saveSetting(QStringLiteral("sourceDirectory"), normalized);
  emit sourceDirectoryChanged();
}

void FixturePipelineController::setWorkspaceRoot(const QString& workspaceRoot) {
  const QString normalized = normalizePath(workspaceRoot);
  if (m_workspaceRoot == normalized) {
    return;
  }
  m_workspaceRoot = normalized;
  saveSetting(QStringLiteral("workspaceRoot"), normalized);
  emit workspaceRootChanged();
  emit workspacePathsChanged();
}

void FixturePipelineController::setToolPath(const QString& toolPath) {
  const QString normalized = normalizePath(toolPath);
  if (m_toolPath == normalized) {
    return;
  }
  m_toolPath = normalized;
  saveSetting(QStringLiteral("toolPath"), normalized);
  emit toolPathChanged();
}

void FixturePipelineController::setConfig(const QString& config) {
  if (m_config == config) {
    return;
  }
  m_config = config;
  saveSetting(QStringLiteral("config"), m_config);
  emit configChanged();
}

void FixturePipelineController::setSplitMode(const QString& splitMode) {
  if (m_splitMode == splitMode) {
    return;
  }
  m_splitMode = splitMode;
  saveSetting(QStringLiteral("splitMode"), m_splitMode);
  emit splitModeChanged();
}

void FixturePipelineController::setSimilarityReviewMinScore(double similarityReviewMinScore) {
  if (similarityReviewMinScore < 0.0 || similarityReviewMinScore > 1.0 || m_similarityReviewMinScore == similarityReviewMinScore) {
    return;
  }
  m_similarityReviewMinScore = similarityReviewMinScore;
  saveSetting(QStringLiteral("similarityReviewMinScore"), similarityReviewMinScore);
  emit similarityReviewMinScoreChanged();
}

void FixturePipelineController::setSimilarityAutoMinScore(double similarityAutoMinScore) {
  if (similarityAutoMinScore < 0.0 || similarityAutoMinScore > 1.0 || m_similarityAutoMinScore == similarityAutoMinScore) {
    return;
  }
  m_similarityAutoMinScore = similarityAutoMinScore;
  saveSetting(QStringLiteral("similarityAutoMinScore"), similarityAutoMinScore);
  emit similarityAutoMinScoreChanged();
}

void FixturePipelineController::setSimilarityMaxPairs(int similarityMaxPairs) {
  if (similarityMaxPairs < 1 || m_similarityMaxPairs == similarityMaxPairs) {
    return;
  }
  m_similarityMaxPairs = similarityMaxPairs;
  saveSetting(QStringLiteral("similarityMaxPairs"), similarityMaxPairs);
  emit similarityMaxPairsChanged();
}

void FixturePipelineController::setDecisionNotes(const QString& decisionNotes) {
  if (m_decisionNotes == decisionNotes) {
    return;
  }
  m_decisionNotes = decisionNotes;
  emit decisionNotesChanged();
}

void FixturePipelineController::refreshReviewGroups() {
  const QString manifestPath =
      QDir(logicalStoreDirectory()).filePath(QStringLiteral("review_candidates/review_groups.json"));
  const QString previousGroupId =
      (m_currentGroupIndex >= 0 && m_currentGroupIndex < m_reviewGroupModel.count())
          ? m_reviewGroupModel.groupAt(m_currentGroupIndex).groupId
          : QString{};

  QList<ReviewGroupItem> groups;
  if (QFileInfo::exists(manifestPath)) {
    const QJsonObject manifest = readJsonObject(manifestPath);
    for (const QJsonValue& value : manifest.value(QStringLiteral("review_groups")).toArray()) {
      const QJsonObject object = value.toObject();
      ReviewGroupItem group;
      group.groupId = object.value(QStringLiteral("group_id")).toString();
      group.path = object.value(QStringLiteral("path")).toString();
      group.contactSheetPath = object.value(QStringLiteral("contact_sheet")).toString();
      group.decisionJsonPath = object.value(QStringLiteral("decision_json")).toString();
      group.logicalIdCount = object.value(QStringLiteral("logical_id_count")).toInt();
      group.memberOccurrenceCount = object.value(QStringLiteral("member_occurrence_count")).toInt();
      group.fixtures = stringListFromJson(object.value(QStringLiteral("fixtures")));
      group.sourceImages = stringListFromJson(object.value(QStringLiteral("source_images")));
      if (!group.groupId.isEmpty()) {
        groups.push_back(group);
      }
    }
  }

  m_reviewGroupModel.setGroups(groups);
  if (groups.isEmpty()) {
    clearCurrentGroup(QStringLiteral("No unresolved review groups."));
    return;
  }

  int selectedIndex = 0;
  if (!previousGroupId.isEmpty()) {
    for (int row = 0; row < groups.size(); ++row) {
      if (groups.at(row).groupId == previousGroupId) {
        selectedIndex = row;
        break;
      }
    }
  }
  selectReviewGroup(selectedIndex);
}

void FixturePipelineController::selectReviewGroup(int index) {
  if (index < 0 || index >= m_reviewGroupModel.count()) {
    clearCurrentGroup(QStringLiteral("No unresolved review groups."));
    return;
  }

  const ReviewGroupItem group = m_reviewGroupModel.groupAt(index);
  const QJsonObject manifest = readJsonObject(QDir(group.path).filePath(QStringLiteral("group.json")));
  const QJsonObject decision = readJsonObject(group.decisionJsonPath);
  applyCurrentGroup(group, manifest, decision);

  if (m_currentGroupIndex != index) {
    m_currentGroupIndex = index;
    emit currentGroupIndexChanged();
  }
}

void FixturePipelineController::runPipeline() {
  if (pipelineRunning()) {
    appendLog(QStringLiteral("Pipeline is already running."));
    return;
  }

  if (m_similarityAutoMinScore < m_similarityReviewMinScore) {
    appendLog(QStringLiteral("Auto-merge threshold must be greater than or equal to the review threshold."));
    setStatus(QStringLiteral("Invalid similarity thresholds"));
    return;
  }

  const QString executable = detectedPipelineExecutable();
  if (executable.isEmpty()) {
    appendLog(QStringLiteral("Could not find scrapbook_pipeline next to the app or in the build tree."));
    setStatus(QStringLiteral("Pipeline executable not found"));
    return;
  }

  QStringList arguments{
      QStringLiteral("--input-dir"),
      m_sourceDirectory,
      QStringLiteral("--work-dir"),
      pipelineWorkDirectory(),
      QStringLiteral("--asset-store"),
      assetStoreDirectory(),
      QStringLiteral("--logical-store"),
      logicalStoreDirectory(),
      QStringLiteral("--config"),
      m_config,
      QStringLiteral("--split-mode"),
      m_splitMode,
      QStringLiteral("--similarity-review-min-score"),
      QString::number(m_similarityReviewMinScore, 'f', 4),
      QStringLiteral("--similarity-auto-min-score"),
      QString::number(m_similarityAutoMinScore, 'f', 4),
      QStringLiteral("--similarity-report-max-pairs"),
      QString::number(m_similarityMaxPairs),
  };
  if (!m_toolPath.isEmpty()) {
    arguments << QStringLiteral("--tool") << m_toolPath;
  }

  if (!m_logText.isEmpty()) {
    m_logText.clear();
    emit logTextChanged();
  }
  appendLog(QStringLiteral("$ %1 %2").arg(executable, arguments.join(QLatin1Char(' '))));
  setStatus(QStringLiteral("Running pipeline..."));
  emit pipelineRunningChanged();
  m_pipelineProcess.start(executable, arguments);
}

void FixturePipelineController::saveCurrentDecision() {
  if (m_currentDecisionPath.isEmpty()) {
    appendLog(QStringLiteral("No review group selected."));
    return;
  }

  const QList<ReviewMemberItem> members = m_reviewMemberModel.members();
  if (members.isEmpty()) {
    appendLog(QStringLiteral("Current review group has no members."));
    return;
  }

  QMap<int, QList<ReviewMemberItem>> clusters;
  for (const ReviewMemberItem& member : members) {
    clusters[member.clusterIndex].push_back(member);
  }

  QJsonObject aliases;
  QSet<QString> distinctPairs;
  for (auto clusterIt = clusters.cbegin(); clusterIt != clusters.cend(); ++clusterIt) {
    const QList<ReviewMemberItem>& clusterMembers = clusterIt.value();
    QString winnerLogicalId;
    int representativeCount = 0;
    for (const ReviewMemberItem& member : clusterMembers) {
      if (member.representative) {
        winnerLogicalId = member.logicalId;
        ++representativeCount;
      }
    }
    if (representativeCount > 1) {
      appendLog(QStringLiteral("Only one representative is allowed per decision group."));
      return;
    }
    if (winnerLogicalId.isEmpty()) {
      winnerLogicalId = clusterMembers.front().logicalId;
    }
    for (const ReviewMemberItem& member : clusterMembers) {
      if (member.logicalId != winnerLogicalId) {
        aliases.insert(member.logicalId, winnerLogicalId);
      }
    }
  }

  const QList<int> keys = clusters.keys();
  for (int leftIndex = 0; leftIndex < keys.size(); ++leftIndex) {
    for (int rightIndex = leftIndex + 1; rightIndex < keys.size(); ++rightIndex) {
      for (const ReviewMemberItem& leftMember : clusters.value(keys.at(leftIndex))) {
        for (const ReviewMemberItem& rightMember : clusters.value(keys.at(rightIndex))) {
          distinctPairs.insert(canonicalPairKey(leftMember.logicalId, rightMember.logicalId));
        }
      }
    }
  }

  QJsonArray distinctPairArray;
  QList<QString> pairValues = distinctPairs.values();
  std::sort(pairValues.begin(), pairValues.end());
  for (const QString& value : pairValues) {
    const auto pair = decodePairKey(value);
    distinctPairArray.append(QJsonArray{pair.first, pair.second});
  }

  QJsonArray availableLogicalIds;
  for (const ReviewMemberItem& member : members) {
    availableLogicalIds.append(member.logicalId);
  }

  const QJsonObject payload{
      {QStringLiteral("group_id"), m_reviewGroupModel.groupAt(m_currentGroupIndex).groupId},
      {QStringLiteral("status"), QStringLiteral("reviewed")},
      {QStringLiteral("notes"), m_decisionNotes.trimmed()},
      {QStringLiteral("aliases"), aliases},
      {QStringLiteral("distinct_pairs"), distinctPairArray},
      {QStringLiteral("available_logical_ids"), availableLogicalIds},
  };

  QDir().mkpath(QFileInfo(m_currentDecisionPath).absolutePath());
  QFile file(m_currentDecisionPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    appendLog(QStringLiteral("Could not save decision: %1").arg(m_currentDecisionPath));
    return;
  }
  file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
  appendLog(QStringLiteral("Saved decision: %1").arg(m_currentDecisionPath));
  setStatus(QStringLiteral("Decision saved"));
}

void FixturePipelineController::saveCurrentDecisionAndRerun() {
  saveCurrentDecision();
  if (m_status == QStringLiteral("Decision saved")) {
    runPipeline();
  }
}

void FixturePipelineController::reloadCurrentGroup() {
  if (m_currentGroupIndex >= 0) {
    selectReviewGroup(m_currentGroupIndex);
  }
}

void FixturePipelineController::openLogicalStore() const {
  QDesktopServices::openUrl(QUrl::fromLocalFile(logicalStoreDirectory()));
}

QString FixturePipelineController::findRepoRoot() const {
  QDir dir(QCoreApplication::applicationDirPath());
  while (dir.exists()) {
    if (QFileInfo::exists(dir.filePath(QStringLiteral(".gitmodules")))) {
      return dir.absolutePath();
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir::currentPath();
}

QString FixturePipelineController::defaultSourceDirectory() const {
  const QStringList candidates{
      QDir(m_repoRoot).filePath(QStringLiteral("third_party/libatlas/tests/Images_files")),
      QDir(m_repoRoot).filePath(QStringLiteral("third_party/libatlas/tests/images_files")),
  };
  for (const QString& candidate : candidates) {
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return m_repoRoot;
}

QString FixturePipelineController::defaultWorkspaceRoot() const {
  return QDir(m_repoRoot).filePath(QStringLiteral("build"));
}

QString FixturePipelineController::detectedPipelineExecutable() const {
  const QString binaryName = executableName(QStringLiteral("scrapbook_pipeline"));
  const QString directCandidate = QDir(QCoreApplication::applicationDirPath()).filePath(binaryName);
  if (QFileInfo::exists(directCandidate)) {
    return directCandidate;
  }

  const QStringList fallbacks{
      QDir(m_repoRoot).filePath(QStringLiteral("build/bin/%1/%2").arg(m_config, binaryName)),
      QDir(m_repoRoot).filePath(QStringLiteral("build/bin/%1").arg(binaryName)),
  };
  for (const QString& candidate : fallbacks) {
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

void FixturePipelineController::appendLog(const QString& text) {
  QString normalized = text;
  normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  if (!normalized.endsWith(QLatin1Char('\n'))) {
    normalized.append(QLatin1Char('\n'));
  }
  m_logText.append(normalized);
  emit logTextChanged();
}

void FixturePipelineController::setStatus(const QString& status) {
  if (m_status == status) {
    return;
  }
  m_status = status;
  emit statusChanged();
}

void FixturePipelineController::saveSetting(const QString& key, const QVariant& value) {
  m_settings.setValue(key, value);
}

QJsonObject FixturePipelineController::readJsonObject(const QString& path) const {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  return document.isObject() ? document.object() : QJsonObject{};
}

void FixturePipelineController::clearCurrentGroup(const QString& title) {
  const bool hadCurrentGroup = hasCurrentGroup();
  m_currentDecisionPath.clear();
  m_currentAvailableLogicalIds.clear();
  m_currentGroupIndex = -1;
  m_currentGroupTitle = title;
  m_currentContactSheetUrl = {};
  m_decisionNotes.clear();
  m_reviewMemberModel.setMembers({});
  emit currentGroupIndexChanged();
  emit currentGroupTitleChanged();
  emit currentContactSheetUrlChanged();
  emit decisionNotesChanged();
  if (hadCurrentGroup) {
    emit hasCurrentGroupChanged();
  }
}

void FixturePipelineController::applyCurrentGroup(
    const ReviewGroupItem& group,
    const QJsonObject& groupManifest,
    const QJsonObject& decision) {
  QStringList logicalIds;
  QHash<QString, QString> parent;
  const QJsonArray membersArray = groupManifest.value(QStringLiteral("members")).toArray();
  for (const QJsonValue& value : membersArray) {
    const QString logicalId = value.toObject().value(QStringLiteral("logical_id")).toString();
    if (!logicalId.isEmpty()) {
      logicalIds.push_back(logicalId);
      parent.insert(logicalId, logicalId);
    }
  }

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

  QSet<QString> declaredWinners;
  const QJsonObject aliases = decision.value(QStringLiteral("aliases")).toObject();
  for (auto aliasIt = aliases.begin(); aliasIt != aliases.end(); ++aliasIt) {
    const QString loser = aliasIt.key();
    const QString winner = aliasIt.value().toString();
    if (parent.contains(loser) && parent.contains(winner)) {
      unite(loser, winner);
      declaredWinners.insert(winner);
    }
  }

  QMap<QString, QStringList> groupedLogicalIds;
  for (const QString& logicalId : logicalIds) {
    groupedLogicalIds[find(logicalId)].push_back(logicalId);
  }

  QHash<QString, int> clusterIndices;
  QSet<QString> normalizedWinners;
  int clusterIndex = 1;
  for (auto groupIt = groupedLogicalIds.begin(); groupIt != groupedLogicalIds.end(); ++groupIt) {
    QStringList clusterLogicalIds = groupIt.value();
    clusterLogicalIds.sort();
    QString winner = clusterLogicalIds.front();
    for (const QString& logicalId : clusterLogicalIds) {
      if (declaredWinners.contains(logicalId)) {
        winner = logicalId;
        break;
      }
    }
    normalizedWinners.insert(winner);
    for (const QString& logicalId : clusterLogicalIds) {
      clusterIndices.insert(logicalId, clusterIndex);
    }
    ++clusterIndex;
  }

  QList<ReviewMemberItem> members;
  for (const QJsonValue& value : membersArray) {
    const QJsonObject object = value.toObject();
    ReviewMemberItem member;
    member.logicalId = object.value(QStringLiteral("logical_id")).toString();
    member.representativeName = object.value(QStringLiteral("representative_name")).toString();
    member.representativeExactId = object.value(QStringLiteral("representative_exact_id")).toString();
    member.representativeSourceImage = object.value(QStringLiteral("representative_source_image")).toString();
    member.representativeSourceFileName = object.value(QStringLiteral("representative_source_file_name")).toString();
    if (member.representativeSourceFileName.isEmpty() && !member.representativeSourceImage.isEmpty()) {
      member.representativeSourceFileName = QFileInfo(member.representativeSourceImage).fileName();
    }
    member.reviewImagePath = object.value(QStringLiteral("review_image")).toString();
    member.memberExactIds = stringListFromJson(object.value(QStringLiteral("member_exact_ids")));
    member.occurrenceCount = object.value(QStringLiteral("occurrence_count")).toInt();
    member.fixtures = stringListFromJson(object.value(QStringLiteral("fixtures")));
    member.sourceImages = stringListFromJson(object.value(QStringLiteral("source_images")));
    member.mergedBySimilarity = object.value(QStringLiteral("merged_by_similarity")).toBool();
    member.mergedByReview = object.value(QStringLiteral("merged_by_review")).toBool();
    member.clusterIndex = clusterIndices.value(member.logicalId, 1);
    member.representative = normalizedWinners.contains(member.logicalId);
    members.push_back(member);
  }

  const bool hadCurrentGroup = hasCurrentGroup();
  m_reviewMemberModel.setMembers(members);
  m_currentDecisionPath = group.decisionJsonPath;
  m_currentAvailableLogicalIds = stringListFromJson(decision.value(QStringLiteral("available_logical_ids")));
  if (m_currentAvailableLogicalIds.isEmpty()) {
    m_currentAvailableLogicalIds = logicalIds;
  }
  m_currentGroupTitle = group.groupId;
  m_currentContactSheetUrl = QUrl::fromLocalFile(group.contactSheetPath);
  m_decisionNotes = decision.value(QStringLiteral("notes")).toString();
  emit currentGroupTitleChanged();
  emit currentContactSheetUrlChanged();
  emit decisionNotesChanged();
  if (!hadCurrentGroup) {
    emit hasCurrentGroupChanged();
  }
}
