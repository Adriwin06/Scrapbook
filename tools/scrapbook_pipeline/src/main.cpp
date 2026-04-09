#include <QGuiApplication>

#include "pipelinerunner.h"

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("scrapbook_pipeline"));
  QGuiApplication::setOrganizationName(QStringLiteral("Adriwin06"));

  PipelineRunner runner;
  return runner.run(QGuiApplication::arguments());
}
