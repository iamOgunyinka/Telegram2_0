QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
include(src/xlsx/qtxlsx.pri)
INCLUDEPATH += "C:\\tdlib64\\include"

LIBS += -L"C:\\tdlib64\\debug\\lib" -ltdnet -ltdcore -ltdutils -ltdclient -ltdactor \
    -ltddb -ltdsqlite -ltdjson
LIBS += -L"C:\\vcpkg\\installed\\x64-windows\\debug\\lib" -llibeay32 -lssleay32 -lzlibd

#LIBS += -L"C:\\tdlib64\\release\\lib" -ltdnet -ltdcore -ltdutils -ltdclient -ltdactor \
#    -ltddb -ltdsqlite -ltdjson
#LIBS += -L"C:\\vcpkg\\installed\\x64-windows\\lib" -llibeay32 -lssleay32 -lzlib


SOURCES += \
    account.cpp \
    backgroundworker.cpp \
    main.cpp \
    maindialog.cpp \
    registrationdialog.cpp \
    schedulerdialog.cpp

HEADERS += \
    account.hpp \
    backgroundworker.hpp \
    maindialog.hpp \
    registrationdialog.hpp \
    schedulerdialog.hpp

FORMS += \
    maindialog.ui \
    registrationdialog.ui \
    schedulerdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
