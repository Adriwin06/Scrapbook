#pragma once

#include <QAbstractListModel>
#include <QStringList>

struct ReviewMemberItem {
  QString logicalId;
  QString representativeName;
  QString representativeExactId;
  QString representativeSourceImage;
  QString representativeSourceFileName;
  QString reviewImagePath;
  QStringList memberExactIds;
  int occurrenceCount = 0;
  QStringList fixtures;
  QStringList sourceImages;
  bool mergedBySimilarity = false;
  bool mergedByReview = false;
  int clusterIndex = 1;
  bool representative = false;
};

class ReviewMemberModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
  enum Role {
    LogicalIdRole = Qt::UserRole + 1,
    RepresentativeNameRole,
    RepresentativeExactIdRole,
    RepresentativeSourceImageRole,
    RepresentativeSourceFileNameRole,
    ReviewImageUrlRole,
    OccurrenceCountRole,
    ClusterIndexRole,
    RepresentativeRole,
    FixturesRole,
    SourceImagesRole,
    MergedBySimilarityRole,
    MergedByReviewRole,
  };

  explicit ReviewMemberModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const;
  void setMembers(QList<ReviewMemberItem> members);
  QList<ReviewMemberItem> members() const;

  Q_INVOKABLE void setClusterIndex(int row, int clusterIndex);
  Q_INVOKABLE void setRepresentative(int row, bool representative);
  Q_INVOKABLE void mergeRows(const QVariantList& rows);
  Q_INVOKABLE void splitRows(const QVariantList& rows);
  Q_INVOKABLE void chooseRepresentative(int row);
  Q_INVOKABLE void toggleRepresentative(int row);

signals:
  void countChanged();

private:
  QList<ReviewMemberItem> m_members;
};
