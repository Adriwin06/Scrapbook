#pragma once

#include <QAbstractListModel>
#include <QStringList>

struct ReviewGroupItem {
  QString groupId;
  QString path;
  QString contactSheetPath;
  QString decisionJsonPath;
  int logicalIdCount = 0;
  int memberOccurrenceCount = 0;
  QStringList fixtures;
  QStringList sourceImages;
};

class ReviewGroupModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
  enum Role {
    GroupIdRole = Qt::UserRole + 1,
    LabelRole,
    LogicalIdCountRole,
    MemberOccurrenceCountRole,
    ContactSheetUrlRole,
    FixturesRole,
    SourceImagesRole,
  };

  explicit ReviewGroupModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const;
  void setGroups(QList<ReviewGroupItem> groups);
  ReviewGroupItem groupAt(int row) const;

signals:
  void countChanged();

private:
  QList<ReviewGroupItem> m_groups;
};
