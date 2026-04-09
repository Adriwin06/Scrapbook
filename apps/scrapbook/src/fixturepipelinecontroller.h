#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QUrl>

#include "reviewgroupmodel.h"
#include "reviewmembermodel.h"

class FixturePipelineController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString sourceDirectory READ sourceDirectory WRITE setSourceDirectory NOTIFY sourceDirectoryChanged)
  Q_PROPERTY(QString workspaceRoot READ workspaceRoot WRITE setWorkspaceRoot NOTIFY workspaceRootChanged)
  Q_PROPERTY(QString toolPath READ toolPath WRITE setToolPath NOTIFY toolPathChanged)
  Q_PROPERTY(QString config READ config WRITE setConfig NOTIFY configChanged)
  Q_PROPERTY(QString splitMode READ splitMode WRITE setSplitMode NOTIFY splitModeChanged)
  Q_PROPERTY(int similarityMaxPairs READ similarityMaxPairs WRITE setSimilarityMaxPairs NOTIFY similarityMaxPairsChanged)
  Q_PROPERTY(QString assetStoreDirectory READ assetStoreDirectory NOTIFY workspacePathsChanged)
  Q_PROPERTY(QString logicalStoreDirectory READ logicalStoreDirectory NOTIFY workspacePathsChanged)
  Q_PROPERTY(QString pipelineWorkDirectory READ pipelineWorkDirectory NOTIFY workspacePathsChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
  Q_PROPERTY(bool pipelineRunning READ pipelineRunning NOTIFY pipelineRunningChanged)
  Q_PROPERTY(ReviewGroupModel* reviewGroupModel READ reviewGroupModel CONSTANT)
  Q_PROPERTY(ReviewMemberModel* reviewMemberModel READ reviewMemberModel CONSTANT)
  Q_PROPERTY(int currentGroupIndex READ currentGroupIndex NOTIFY currentGroupIndexChanged)
  Q_PROPERTY(QString currentGroupTitle READ currentGroupTitle NOTIFY currentGroupTitleChanged)
  Q_PROPERTY(QUrl currentContactSheetUrl READ currentContactSheetUrl NOTIFY currentContactSheetUrlChanged)
  Q_PROPERTY(QString decisionNotes READ decisionNotes WRITE setDecisionNotes NOTIFY decisionNotesChanged)
  Q_PROPERTY(bool hasCurrentGroup READ hasCurrentGroup NOTIFY hasCurrentGroupChanged)

public:
  explicit FixturePipelineController(QObject* parent = nullptr);

  QString sourceDirectory() const;
  QString workspaceRoot() const;
  QString toolPath() const;
  QString config() const;
  QString splitMode() const;
  int similarityMaxPairs() const;

  QString assetStoreDirectory() const;
  QString logicalStoreDirectory() const;
  QString pipelineWorkDirectory() const;

  QString status() const;
  QString logText() const;
  bool pipelineRunning() const;

  ReviewGroupModel* reviewGroupModel();
  ReviewMemberModel* reviewMemberModel();

  int currentGroupIndex() const;
  QString currentGroupTitle() const;
  QUrl currentContactSheetUrl() const;
  QString decisionNotes() const;
  bool hasCurrentGroup() const;

  void setSourceDirectory(const QString& sourceDirectory);
  void setWorkspaceRoot(const QString& workspaceRoot);
  void setToolPath(const QString& toolPath);
  void setConfig(const QString& config);
  void setSplitMode(const QString& splitMode);
  void setSimilarityMaxPairs(int similarityMaxPairs);
  void setDecisionNotes(const QString& decisionNotes);

  Q_INVOKABLE void refreshReviewGroups();
  Q_INVOKABLE void selectReviewGroup(int index);
  Q_INVOKABLE void runPipeline();
  Q_INVOKABLE void saveCurrentDecision();
  Q_INVOKABLE void saveCurrentDecisionAndRerun();
  Q_INVOKABLE void reloadCurrentGroup();
  Q_INVOKABLE void openLogicalStore() const;

signals:
  void sourceDirectoryChanged();
  void workspaceRootChanged();
  void toolPathChanged();
  void configChanged();
  void splitModeChanged();
  void similarityMaxPairsChanged();
  void workspacePathsChanged();
  void statusChanged();
  void logTextChanged();
  void pipelineRunningChanged();
  void currentGroupIndexChanged();
  void currentGroupTitleChanged();
  void currentContactSheetUrlChanged();
  void decisionNotesChanged();
  void hasCurrentGroupChanged();

private:
  QString findRepoRoot() const;
  QString defaultSourceDirectory() const;
  QString defaultWorkspaceRoot() const;
  QString detectedPipelineExecutable() const;
  void appendLog(const QString& text);
  void setStatus(const QString& status);
  void saveSetting(const QString& key, const QVariant& value);
  QJsonObject readJsonObject(const QString& path) const;
  void clearCurrentGroup(const QString& title);
  void applyCurrentGroup(const ReviewGroupItem& group, const QJsonObject& groupManifest, const QJsonObject& decision);

  QString m_repoRoot;
  QString m_sourceDirectory;
  QString m_workspaceRoot;
  QString m_toolPath;
  QString m_config;
  QString m_splitMode;
  int m_similarityMaxPairs = 20000;
  QString m_status;
  QString m_logText;
  QProcess m_pipelineProcess;
  ReviewGroupModel m_reviewGroupModel;
  ReviewMemberModel m_reviewMemberModel;
  int m_currentGroupIndex = -1;
  QString m_currentGroupTitle;
  QUrl m_currentContactSheetUrl;
  QString m_decisionNotes;
  QString m_currentDecisionPath;
  QStringList m_currentAvailableLogicalIds;
  QSettings m_settings;
};
