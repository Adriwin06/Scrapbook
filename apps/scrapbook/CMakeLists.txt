qt_add_executable(scrapbook
  src/main.cpp
  src/fixturepipelinecontroller.cpp
  src/fixturepipelinecontroller.h
  src/reviewgroupmodel.cpp
  src/reviewgroupmodel.h
  src/reviewmembermodel.cpp
  src/reviewmembermodel.h
)

qt_add_qml_module(scrapbook
  URI Scrapbook
  VERSION 1.0
  QML_FILES
    qml/Main.qml
)

target_link_libraries(scrapbook
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(scrapbook PROPERTIES
  WIN32_EXECUTABLE TRUE
)
