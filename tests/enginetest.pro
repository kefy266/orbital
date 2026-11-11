QT = core network
CONFIG += c++17 console
TARGET = enginetest
DESTDIR = ../build/tests
OBJECTS_DIR = ../build/tests/obj
MOC_DIR = ../build/tests/moc
RCC_DIR = ../build/tests/rcc
SOURCES += enginetest.cpp ../src/paths.cpp ../src/config.cpp ../src/llm.cpp ../src/mcpclient.cpp ../src/tools.cpp \
           ../src/nativeengine.cpp ../src/opencodeengine.cpp ../src/skillmanager.cpp ../src/pluginmanager.cpp
HEADERS += ../src/engine.h ../src/llm.h ../src/mcpclient.h ../src/tools.h ../src/nativeengine.h ../src/opencodeengine.h \
           ../src/skillmanager.h ../src/pluginmanager.h
RESOURCES += ../resources/orbital.qrc
