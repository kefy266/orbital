# Orbital — Qt Quick (QML) UI over the Qt Core/Network engines.
QT       += core gui network quick quickcontrols2 svg
CONFIG   += c++17
DEFINES  += QT_NO_KEYWORDS
TARGET    = orbital
TEMPLATE  = app

DESTDIR     = build
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc

SOURCES += \
    src/paths.cpp \
    src/config.cpp \
    src/llm.cpp \
    src/mcpclient.cpp \
    src/tools.cpp \
    src/nativeengine.cpp \
    src/opencodeengine.cpp \
    src/skillmanager.cpp \
    src/pluginmanager.cpp \
    src/app/chatmodel.cpp \
    src/app/controller.cpp \
    src/app/iconprovider.cpp \
    src/app/main.cpp

HEADERS += \
    src/platform.h \
    src/paths.h \
    src/config.h \
    src/engine.h \
    src/llm.h \
    src/mcpclient.h \
    src/tools.h \
    src/nativeengine.h \
    src/opencodeengine.h \
    src/skillmanager.h \
    src/pluginmanager.h \
    src/app/chatmodel.h \
    src/app/controller.h \
    src/app/iconprovider.h

RESOURCES += resources/orbital.qrc

# Windows / macOS
win32-msvc*: QMAKE_CXXFLAGS += /utf-8 /permissive-
win32 {
    TARGET = Orbital
    QMAKE_TARGET_DESCRIPTION = Orbital
    QMAKE_TARGET_PRODUCT = Orbital
    VERSION = 0.3.0
}
macx {
    TARGET = Orbital
    QMAKE_TARGET_BUNDLE_PREFIX = io.github.kefy266
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 12.0
}
