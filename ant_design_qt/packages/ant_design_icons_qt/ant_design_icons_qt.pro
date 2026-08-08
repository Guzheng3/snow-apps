TEMPLATE = lib
TARGET = ant_design_icons_qt
VERSION = 2.0.0

QT += core gui widgets svg
CONFIG += c++17
CONFIG += staticlib
DEFINES += ADQT_ICONS_LIBRARY

win32-msvc {
    QMAKE_CXXFLAGS += /MP
}

INCLUDEPATH += $$PWD/src
INCLUDEPATH += $$PWD/src/generated
INCLUDEPATH += $$clean_path($$PWD/../adqt_icon_core/src)

HEADERS += \
    src/ant_design_icons_qt_global.h \
    src/external_icon_pack.h \
    src/antd_icons.h \
    src/version.h

SOURCES += \
    src/external_icon_pack.cpp \
    src/antd_icons.cpp

isEmpty(PREFIX) {
    PREFIX = /usr/local
}

win32 {
    PREFIX = C:/ant_design_qt
}

target.path = $$PREFIX/lib
headers.path = $$PREFIX/include/ant_design_icons_qt
headers.files = $$HEADERS
INSTALLS += target headers
