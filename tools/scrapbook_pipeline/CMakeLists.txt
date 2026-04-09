qt_add_executable(scrapbook_pipeline
  src/main.cpp
  src/pipelinerunner.cpp
  src/pipelinerunner.h
)

target_link_libraries(scrapbook_pipeline
  PRIVATE
    Qt6::Core
    Qt6::Gui
)
