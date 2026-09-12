import QtQuick 2.0
import Sailfish.Silica 1.0
import PodorozhnikBalance 1.0

Page {
    id: page

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Баланс Подорожника")
            }

            // Ожидание карты
            Column {
                width: parent.width
                spacing: Theme.paddingLarge
                visible: cardReader.state === CardReader.Waiting

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.highlightColor
                    text: qsTr("Приложите карту Подорожник к задней панели телефона")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    visible: !cardReader.nfcEnabled
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.secondaryHighlightColor
                    text: qsTr("NFC выключен — включите его в настройках")
                }
            }

            // Чтение карты
            Column {
                width: parent.width
                spacing: Theme.paddingLarge
                visible: cardReader.state === CardReader.Reading

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: cardReader.state === CardReader.Reading
                    size: BusyIndicatorSize.Large
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.highlightColor
                    text: qsTr("Чтение карты…")
                }
            }

            // Результат
            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: cardReader.state === CardReader.Result

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.secondaryColor
                    text: qsTr("Баланс")
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeHuge
                    color: Theme.highlightColor
                    text: cardReader.balanceText
                }

                Separator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    color: Theme.secondaryColor
                }

                DetailItem {
                    visible: cardReader.cardNumber.length > 0
                    label: qsTr("Номер карты")
                    value: cardReader.cardNumber
                }

                DetailItem {
                    label: qsTr("UID карты")
                    value: cardReader.uidText
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: qsTr("Приложите карту ещё раз, чтобы обновить данные")
                }
            }

            // Ошибка
            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: cardReader.state === CardReader.Error

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.highlightColor
                    text: qsTr("Не удалось прочитать карту")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: cardReader.errorText
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.secondaryHighlightColor
                    text: qsTr("Попробуйте приложить карту ещё раз")
                }
            }
        }
    }
}
