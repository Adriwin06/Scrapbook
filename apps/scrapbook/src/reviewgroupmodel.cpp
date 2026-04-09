#include "reviewgroupmodel.h"

#include <QUrl>

ReviewGroupModel::ReviewGroupModel(QObject* parent)
    : QAbstractListModel(parent) {}

int ReviewGroupModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return m_groups.size();
}

QVariant ReviewGroupModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_groups.size()) {
    return {};
  }

  const ReviewGroupItem& group = m_groups.at(index.row());
  switch (role) {
    case GroupIdRole:
      return group.groupId;
    case LabelRole:
      return QStringLiteral("%1 (%2 logical IDs, %3 occurrences)")
          .arg(group.groupId)
          .arg(group.logicalIdCount)
          .arg(group.memberOccurrenceCount);
    case LogicalIdCountRole:
      return group.logicalIdCount;
    case MemberOccurrenceCountRole:
      return group.memberOccurrenceCount;
    case ContactSheetUrlRole:
      return QUrl::fromLocalFile(group.contactSheetPath);
    case FixturesRole:
      return group.fixtures;
    case SourceImagesRole:
      return group.sourceImages;
    default:
      return {};
  }
}

QHash<int, QByteArray> ReviewGroupModel::roleNames() const {
  return {
      {GroupIdRole, "groupId"},
      {LabelRole, "label"},
      {LogicalIdCountRole, "logicalIdCount"},
      {MemberOccurrenceCountRole, "memberOccurrenceCount"},
      {ContactSheetUrlRole, "contactSheetUrl"},
      {FixturesRole, "fixtures"},
      {SourceImagesRole, "sourceImages"},
  };
}

int ReviewGroupModel::count() const {
  return m_groups.size();
}

void ReviewGroupModel::setGroups(QList<ReviewGroupItem> groups) {
  beginResetModel();
  m_groups = std::move(groups);
  endResetModel();
  emit countChanged();
}

ReviewGroupItem ReviewGroupModel::groupAt(int row) const {
  if (row < 0 || row >= m_groups.size()) {
    return {};
  }
  return m_groups.at(row);
}
