QT += core gui widgets svg network
CONFIG += c++17 console release
TEMPLATE = app
TARGET = date-picker-perf

INCLUDEPATH += ../../packages/ant_design_qt/src
INCLUDEPATH += ../../packages/adqt_icon_core/src
INCLUDEPATH += ../../packages/ant_design_icons_qt/src

SOURCES += main.cpp

isEmpty(ADQT_LIB_BUILD_DIR) {
    ADQT_LIB_BUILD_DIR = $$clean_path($$PWD/../../packages/ant_design_qt/build-mingw)
}

isEmpty(ADQT_ICON_CORE_LIB_BUILD_DIR) {
    ADQT_ICON_CORE_LIB_BUILD_DIR = $$clean_path($$PWD/../../packages/adqt_icon_core/build-mingw)
}

isEmpty(ADQT_ICONS_LIB_BUILD_DIR) {
    ADQT_ICONS_LIB_BUILD_DIR = $$clean_path($$PWD/../../packages/ant_design_icons_qt/build-mingw)
}

CONFIG(debug, debug|release) {
    ADQT_LIB_DIR = $$ADQT_LIB_BUILD_DIR/debug
    ADQT_ICON_CORE_LIB_DIR = $$ADQT_ICON_CORE_LIB_BUILD_DIR/debug
    ADQT_ICONS_LIB_DIR = $$ADQT_ICONS_LIB_BUILD_DIR/debug
} else {
    ADQT_LIB_DIR = $$ADQT_LIB_BUILD_DIR/release
    ADQT_ICON_CORE_LIB_DIR = $$ADQT_ICON_CORE_LIB_BUILD_DIR/release
    ADQT_ICONS_LIB_DIR = $$ADQT_ICONS_LIB_BUILD_DIR/release
}

win32-g++ {
    PRE_TARGETDEPS += $$ADQT_LIB_DIR/libant_design_qt.a
    PRE_TARGETDEPS += $$ADQT_ICON_CORE_LIB_DIR/libadqt_icon_core.a
    PRE_TARGETDEPS += $$ADQT_ICONS_LIB_DIR/libant_design_icons_qt.a
    LIBS += -L$$ADQT_LIB_DIR -lant_design_qt
    LIBS += -L$$ADQT_ICON_CORE_LIB_DIR -ladqt_icon_core
    LIBS += -L$$ADQT_ICONS_LIB_DIR -lant_design_icons_qt
}

win32-msvc {
    PRE_TARGETDEPS += $$ADQT_LIB_DIR/ant_design_qt.lib
    PRE_TARGETDEPS += $$ADQT_ICON_CORE_LIB_DIR/adqt_icon_core.lib
    PRE_TARGETDEPS += $$ADQT_ICONS_LIB_DIR/ant_design_icons_qt.lib
    LIBS += $$ADQT_LIB_DIR/ant_design_qt.lib
    LIBS += $$ADQT_ICON_CORE_LIB_DIR/adqt_icon_core.lib
    LIBS += $$ADQT_ICONS_LIB_DIR/ant_design_icons_qt.lib
}
