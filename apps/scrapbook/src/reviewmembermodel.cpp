#include "reviewmembermodel.h"

#include <QUrl>

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
