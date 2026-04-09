#include "reviewmembermodel.h"

#include <QSet>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace {

QList<int> normalizeRows(const QVariantList& rows, int memberCount) {
  QSet<int> seenRows;
  QList<int> normalizedRows;
  for (const QVariant& value : rows) {
    const int row = value.toInt();
    if (row < 0 || row >= memberCount || seenRows.contains(row)) {
      continue;
    }
    seenRows.insert(row);
    normalizedRows.push_back(row);
  }
  std::sort(normalizedRows.begin(), normalizedRows.end());
  return normalizedRows;
}

}  // namespace

ReviewMemberModel::ReviewMemberModel(QObject* parent)
    : QAbstractListModel(parent) {}

int ReviewMemberModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return m_members.size();
}

QVariant ReviewMemberModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_members.size()) {
    return {};
  }

  const ReviewMemberItem& member = m_members.at(index.row());
  switch (role) {
    case LogicalIdRole:
      return member.logicalId;
    case RepresentativeNameRole:
      return member.representativeName;
    case RepresentativeExactIdRole:
      return member.representativeExactId;
    case RepresentativeSourceImageRole:
      return member.representativeSourceImage;
    case ReviewImageUrlRole:
      return member.reviewImagePath.isEmpty()
                 ? QVariant{}
                 : QVariant::fromValue(QUrl::fromLocalFile(member.reviewImagePath));
    case OccurrenceCountRole:
      return member.occurrenceCount;
    case ClusterIndexRole:
      return member.clusterIndex;
    case RepresentativeRole:
      return member.representative;
    case FixturesRole:
      return member.fixtures;
    case SourceImagesRole:
      return member.sourceImages;
    case MergedBySimilarityRole:
      return member.mergedBySimilarity;
    case MergedByReviewRole:
      return member.mergedByReview;
    default:
      return {};
  }
}

bool ReviewMemberModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_members.size()) {
    return false;
  }

  ReviewMemberItem& member = m_members[index.row()];
  bool changed = false;
  switch (role) {
    case ClusterIndexRole: {
      const int clusterIndex = value.toInt();
      if (clusterIndex >= 1 && member.clusterIndex != clusterIndex) {
        member.clusterIndex = clusterIndex;
        changed = true;
      }
      break;
    }
    case RepresentativeRole: {
      const bool representative = value.toBool();
      if (member.representative != representative) {
        member.representative = representative;
        changed = true;
      }
      break;
    }
    default:
      break;
  }

  if (changed) {
    emit dataChanged(index, index, {role});
  }
  return changed;
}

Qt::ItemFlags ReviewMemberModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> ReviewMemberModel::roleNames() const {
  return {
      {LogicalIdRole, "logicalId"},
      {RepresentativeNameRole, "representativeName"},
      {RepresentativeExactIdRole, "representativeExactId"},
      {RepresentativeSourceImageRole, "representativeSourceImage"},
      {ReviewImageUrlRole, "reviewImageUrl"},
      {OccurrenceCountRole, "occurrenceCount"},
      {ClusterIndexRole, "clusterIndex"},
      {RepresentativeRole, "representative"},
      {FixturesRole, "fixtures"},
      {SourceImagesRole, "sourceImages"},
      {MergedBySimilarityRole, "mergedBySimilarity"},
      {MergedByReviewRole, "mergedByReview"},
  };
}

int ReviewMemberModel::count() const {
  return m_members.size();
}

void ReviewMemberModel::setMembers(QList<ReviewMemberItem> members) {
  beginResetModel();
  m_members = std::move(members);
  endResetModel();
  emit countChanged();
}

QList<ReviewMemberItem> ReviewMemberModel::members() const {
  return m_members;
}

void ReviewMemberModel::setClusterIndex(int row, int clusterIndex) {
  if (row < 0 || row >= m_members.size()) {
    return;
  }
  setData(index(row, 0), clusterIndex, ClusterIndexRole);
}

void ReviewMemberModel::setRepresentative(int row, bool representative) {
  if (row < 0 || row >= m_members.size()) {
    return;
  }
  setData(index(row, 0), representative, RepresentativeRole);
}

void ReviewMemberModel::mergeRows(const QVariantList& rows) {
  const QList<int> normalizedRows = normalizeRows(rows, m_members.size());
  if (normalizedRows.size() < 2) {
    return;
  }

  const int targetClusterIndex = m_members.at(normalizedRows.front()).clusterIndex;
  int representativeRow = -1;
  for (int row = 0; row < m_members.size(); ++row) {
    if (m_members.at(row).clusterIndex == targetClusterIndex && m_members.at(row).representative) {
      representativeRow = row;
      break;
    }
  }
  if (representativeRow < 0) {
    for (const int row : normalizedRows) {
      if (m_members.at(row).representative) {
        representativeRow = row;
        break;
      }
    }
  }
  if (representativeRow < 0) {
    representativeRow = normalizedRows.front();
  }

  for (const int row : normalizedRows) {
    ReviewMemberItem& member = m_members[row];
    const bool clusterChanged = member.clusterIndex != targetClusterIndex;
    const bool representativeChanged = member.representative != (row == representativeRow);
    if (!clusterChanged && !representativeChanged) {
      continue;
    }
    member.clusterIndex = targetClusterIndex;
    member.representative = (row == representativeRow);
    emit dataChanged(index(row, 0), index(row, 0), {ClusterIndexRole, RepresentativeRole});
  }
}

void ReviewMemberModel::splitRows(const QVariantList& rows) {
  const QList<int> normalizedRows = normalizeRows(rows, m_members.size());
  if (normalizedRows.isEmpty()) {
    return;
  }

  int nextClusterIndex = 1;
  for (const ReviewMemberItem& member : std::as_const(m_members)) {
    nextClusterIndex = std::max(nextClusterIndex, member.clusterIndex + 1);
  }

  for (const int row : normalizedRows) {
    ReviewMemberItem& member = m_members[row];
    member.clusterIndex = nextClusterIndex++;
    member.representative = true;
    emit dataChanged(index(row, 0), index(row, 0), {ClusterIndexRole, RepresentativeRole});
  }
}

void ReviewMemberModel::chooseRepresentative(int row) {
  if (row < 0 || row >= m_members.size()) {
    return;
  }

  const int clusterIndex = m_members.at(row).clusterIndex;
  for (int memberRow = 0; memberRow < m_members.size(); ++memberRow) {
    ReviewMemberItem& member = m_members[memberRow];
    if (member.clusterIndex != clusterIndex) {
      continue;
    }
    const bool nextRepresentative = memberRow == row;
    if (member.representative == nextRepresentative) {
      continue;
    }
    member.representative = nextRepresentative;
    emit dataChanged(index(memberRow, 0), index(memberRow, 0), {RepresentativeRole});
  }
}

void ReviewMemberModel::toggleRepresentative(int row) {
  if (row < 0 || row >= m_members.size()) {
    return;
  }

  if (!m_members.at(row).representative) {
    chooseRepresentative(row);
    return;
  }

  ReviewMemberItem& member = m_members[row];
  member.representative = false;
  emit dataChanged(index(row, 0), index(row, 0), {RepresentativeRole});
}
