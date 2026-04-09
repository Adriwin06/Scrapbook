#pragma once

#include <QStringList>

class PipelineRunner final {
public:
  int run(const QStringList& arguments);
};
