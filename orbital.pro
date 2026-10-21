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
