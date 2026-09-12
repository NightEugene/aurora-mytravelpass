TARGET = ru.nighteugene.PodorozhnikBalance

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
    qml/ru.nighteugene.PodorozhnikBalance.qml \
    qml/pages/MainPage.qml \
    qml/cover/DefaultCoverPage.qml \
    rpm/ru.nighteugene.PodorozhnikBalance.spec \
    ru.nighteugene.PodorozhnikBalance.desktop \
    README.md

AURORAAPP_ICONS = 86x86 108x108 128x128 172x172

TRANSLATIONS += \
    translations/ru.nighteugene.PodorozhnikBalance.ts \
    translations/ru.nighteugene.PodorozhnikBalance-ru.ts
