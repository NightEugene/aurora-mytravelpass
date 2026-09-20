TARGET = ru.nighteugene.MyTravelPass

CONFIG += \
    auroraapp \
    auroraapp_i18n

QT += \
    dbus

SOURCES += \
    src/main.cpp \
    src/cardreader.cpp \
    src/podorozhnik.cpp

HEADERS += \
    src/cardreader.h \
    src/podorozhnik.h

DISTFILES += \
    qml/ru.nighteugene.MyTravelPass.qml \
    qml/pages/MainPage.qml \
    qml/cover/DefaultCoverPage.qml \
    rpm/ru.nighteugene.MyTravelPass.spec \
    ru.nighteugene.MyTravelPass.desktop \
    README.md

AURORAAPP_ICONS = 86x86 108x108 128x128 172x172

TRANSLATIONS += \
    translations/ru.nighteugene.MyTravelPass.ts \
    translations/ru.nighteugene.MyTravelPass-ru.ts
