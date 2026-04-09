#include <QCoreApplication>

#include "pipelinerunner.h"

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("scrapbook_pipeline"));
  QCoreApplication::setOrganizationName(QStringLiteral("Adriwin06"));

  PipelineRunner runner;
  return runner.run(QCoreApplication::arguments());
}
