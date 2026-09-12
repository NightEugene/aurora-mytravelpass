// SPDX-License-Identifier: BSD-3-Clause

#include <auroraapp.h>
#include <QtQuick>
#include <QtQml>

#include "cardreader.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.nighteugene"));
    application->setApplicationName(QStringLiteral("PodorozhnikBalance"));

    // Доступ к перечислению CardReader.State из QML
    qmlRegisterUncreatableType<CardReader>("PodorozhnikBalance", 1, 0, "CardReader",
                                           QStringLiteral("Только перечисления"));

    QScopedPointer<QQuickView> view(Aurora::Application::createView());
    view->rootContext()->setContextProperty(QStringLiteral("cardReader"),
                                            new CardReader(view.data()));
    view->setSource(Aurora::Application::pathTo(
            QStringLiteral("qml/ru.nighteugene.PodorozhnikBalance.qml")));
    view->show();

    return application->exec();
}
