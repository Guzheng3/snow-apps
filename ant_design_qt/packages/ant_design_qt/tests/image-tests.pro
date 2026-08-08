QT += core gui widgets svg network testlib
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = image-tests

INCLUDEPATH += ../src
INCLUDEPATH += ../../adqt_icon_core/src
INCLUDEPATH += ../../ant_design_icons_qt/src

SOURCES += tst_image.cpp

win32-msvc {
    ADQT_DEFAULT_BUILD_DIR = build-msvc
} else {
    ADQT_DEFAULT_BUILD_DIR = build-mingw
}

isEmpty(ADQT_LIB_BUILD_DIR) {
    ADQT_LIB_BUILD_DIR = $$clean_path($$OUT_PWD/..)
}
isEmpty(ADQT_ICON_CORE_LIB_BUILD_DIR) {
    ADQT_ICON_CORE_LIB_BUILD_DIR = $$clean_path($$OUT_PWD/../../adqt_icon_core)
}
isEmpty(ADQT_ICONS_LIB_BUILD_DIR) {
    ADQT_ICONS_LIB_BUILD_DIR = $$clean_path($$OUT_PWD/../../ant_design_icons_qt)
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

win32-msvc {
    PRE_TARGETDEPS += $$ADQT_LIB_DIR/ant_design_qt.lib
    PRE_TARGETDEPS += $$ADQT_ICON_CORE_LIB_DIR/adqt_icon_core.lib
    PRE_TARGETDEPS += $$ADQT_ICONS_LIB_DIR/ant_design_icons_qt.lib
    LIBS += $$ADQT_LIB_DIR/ant_design_qt.lib
    LIBS += $$ADQT_ICON_CORE_LIB_DIR/adqt_icon_core.lib
    LIBS += $$ADQT_ICONS_LIB_DIR/ant_design_icons_qt.lib
}

win32-g++ {
    LIBS += -L$$ADQT_LIB_DIR -lant_design_qt
    LIBS += -L$$ADQT_ICON_CORE_LIB_DIR -ladqt_icon_core
    LIBS += -L$$ADQT_ICONS_LIB_DIR -lant_design_icons_qt
}

win32:LIBS += -lcomctl32 -ldwmapi -luser32
