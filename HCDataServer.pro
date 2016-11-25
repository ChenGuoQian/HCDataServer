HEADERS += \
    MyServer.h

SOURCES += \
    MyServer.cpp

CONFIG += C++11
QT += network sql
mac{
INCLUDEPATH += /usr/local/include
LIBS += -L/usr/local/lib -ltufao1
}
linux{
LIBS += -ltufao1
}
