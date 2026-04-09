#include <QGuiApplication>
#include <QTextStream>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "fixturepipelinecontroller.h"

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("Scrapbook"));
  QGuiApplication::setOrganizationName(QStringLiteral("Adriwin06"));
  QQuickStyle::setStyle(QStringLiteral("Basic"));

  FixturePipelineController pipelineController;

  QQmlApplicationEngine engine;
  QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError>& warnings) {
    QTextStream stream(stderr);
    for (const QQmlError& warning : warnings) {
      stream << warning.toString() << Qt::endl;
    }
  });
  engine.rootContext()->setContextProperty(QStringLiteral("pipelineController"), &pipelineController);

  QObject::connect(
      &engine,
      &QQmlApplicationEngine::objectCreationFailed,
      &app,
      []() { QCoreApplication::exit(-1); },
      Qt::QueuedConnection);
  engine.loadFromModule(QStringLiteral("Scrapbook"), QStringLiteral("Main"));

  return app.exec();
}
