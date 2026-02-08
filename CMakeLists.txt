cmake_minimum_required(VERSION 3.16)

project(DepthCalc VERSION 1.033 LANGUAGES CXX)

set(PROJECT_NAME "DepthCalc")
set(PROJECT_VERSION "1.033")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets PrintSupport OpenGLWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets PrintSupport OpenGLWidgets Concurrent)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui)
set(RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)

set(PROJECT_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp 
    ${INCLUDE_DIR}/mainwindow.h 
    ${UI_DIR}/mainwindow.ui
    ${SRC_DIR}/FileConverter.cpp
    ${INCLUDE_DIR}/FileConverter.h
    ${SRC_DIR}/qcustomplot.cpp 
    ${INCLUDE_DIR}/qcustomplot.h
    ${SRC_DIR}/plotwidget.cpp
    ${INCLUDE_DIR}/plotwidget.h
    ${SRC_DIR}/dataloader.cpp
    ${INCLUDE_DIR}/dataloader.h
)

if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_SOURCE_DIR}/app_icon.rc")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DepthCalc
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        icons.qrc

        ${APP_ICON_RESOURCE}
        app_icon.rc
        ${INCLUDE_DIR}/calibrationmanager.h 
        ${SRC_DIR}/calibrationmanager.cpp
        ${INCLUDE_DIR}/dcvmanager.h 
        ${SRC_DIR}/dcvmanager.cpp
        ${INCLUDE_DIR}/gl1manager.h 
        ${SRC_DIR}/gl1manager.cpp
        ${INCLUDE_DIR}/dccontroller.h 
        ${SRC_DIR}/dccontroller.cpp
        ${INCLUDE_DIR}/progressdialog.h 
        ${SRC_DIR}/progressdialog.cpp 
        ${UI_DIR}/progressdialog.ui
        ${INCLUDE_DIR}/dcsettings.h 
        ${SRC_DIR}/dcsettings.cpp
        ${INCLUDE_DIR}/settingsdialog.h 
        ${SRC_DIR}/settingsdialog.cpp 
        ${UI_DIR}/settingsdialog.ui
        ${INCLUDE_DIR}/przmanager.h 
        ${SRC_DIR}/przmanager.cpp
        ${INCLUDE_DIR}/snapshotmanager.h 
        ${SRC_DIR}/snapshotmanager.cpp
        ${RESOURCES_DIR}/redo.png 
        ${RESOURCES_DIR}/undo.png

    )

    target_precompile_headers(DepthCalc PRIVATE ${INCLUDE_DIR}/qcustomplot.h)
else()
    if(ANDROID)
        add_library(DepthCalc SHARED ${PROJECT_SOURCES})
    else()
        add_executable(DepthCalc ${PROJECT_SOURCES})
    endif()
endif()

target_compile_definitions(DepthCalc PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_include_directories(DepthCalc PRIVATE 
    ${INCLUDE_DIR}
    ${SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(OpenGL REQUIRED)

target_link_libraries(DepthCalc PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    OpenGL::GL
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.DepthCalc)
endif()

set_target_properties(DepthCalc PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS DepthCalc
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DepthCalc)
endif()
