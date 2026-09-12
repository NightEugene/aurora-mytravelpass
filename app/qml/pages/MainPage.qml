import QtQuick 2.0
import Sailfish.Silica 1.0
import PodorozhnikBalance 1.0

Page {
    id: page

    // Тарифы из макета (наземный/метро), ₽ за поездку — для счётчиков
    // «хватит на N поездок». TODO: подтвердить актуальные тарифы СПб.
    readonly property int fareGround: 30
    readonly property int fareMetro: 38

    property int currentTab: 0

    // Бирюзовый фон (макет)
    Rectangle {
        anchors.fill: parent
        z: -1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#5ED3C7" }
            GradientStop { position: 1.0; color: "#3FAFA4" }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: mainColumn.height + Theme.paddingLarge

        Column {
            id: mainColumn
            width: parent.width
            spacing: Theme.paddingMedium

            Item { width: 1; height: Theme.paddingSmall }

            Label {
                x: Theme.horizontalPageMargin
                text: qsTr("Баланс\nподорожника")
                color: "white"
                font.pixelSize: Theme.fontSizeHuge
                font.bold: true
            }

            // Белая карточка
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: cardColumn.height + 2 * Theme.paddingMedium
                radius: Theme.paddingLarge
                color: "white"

                Column {
                    id: cardColumn
                    x: Theme.paddingMedium
                    y: Theme.paddingMedium
                    width: parent.width - 2 * Theme.paddingMedium
                    spacing: Theme.paddingMedium

                    // Верхний ряд: обновить слева, корзина справа
                    Item {
                        width: parent.width
                        height: Theme.iconSizeMedium

                        IconButton {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            icon.source: "image://theme/icon-m-refresh"
                            onClicked: cardReader.refresh()
                        }

                        IconButton {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            icon.source: "image://theme/icon-m-delete"
                            onClicked: cardReader.clearHistory()
                        }
                    }

                    // Зелёная плашка: баланс и время чтения
                    Rectangle {
                        width: parent.width
                        height: Math.round(width * 0.55)
                        radius: Theme.paddingMedium
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#7CB342" }
                            GradientStop { position: 1.0; color: "#558B2F" }
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: Theme.paddingSmall

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: cardReader.state === CardReader.Result
                                text: cardReader.balanceText
                                color: "white"
                                font.pixelSize: Theme.fontSizeHuge * 1.2
                                font.bold: true
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: cardReader.state === CardReader.Result
                                text: cardReader.lastReadTime
                                color: "#CCFFFFFF"
                                font.pixelSize: Theme.fontSizeMedium
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: cardReader.state === CardReader.Waiting
                                         || cardReader.state === CardReader.Error
                                text: qsTr("Приложите карту")
                                color: "#B3FFFFFF"
                                font.pixelSize: Theme.fontSizeExtraLarge
                            }

                            BusyIndicator {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: cardReader.state === CardReader.Reading
                                running: visible
                                size: BusyIndicatorSize.Large
                            }
                        }
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Подорожник")
                        color: "#212121"
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                    }

                    // Вкладки
                    Item {
                        width: parent.width
                        height: tabsRow.height

                        Row {
                            id: tabsRow
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: Theme.paddingLarge

                            Repeater {
                                model: [qsTr("ОСТАТОК"), qsTr("ИЗМЕНЕНИЯ"), qsTr("ИНФО")]

                                Column {
                                    spacing: Theme.paddingSmall / 2

                                    Label {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                        font.bold: page.currentTab === index
                                        color: page.currentTab === index
                                               ? "#558B2F" : "#9E9E9E"

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: page.currentTab = index
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: 3
                                        radius: 1
                                        color: "#7CB342"
                                        visible: page.currentTab === index
                                    }
                                }
                            }
                        }
                    }

                    // ОСТАТОК: счётчики поездок
                    Column {
                        width: parent.width
                        spacing: Theme.paddingMedium
                        visible: page.currentTab === 0

                        Rectangle {
                            width: parent.width
                            height: Theme.itemSizeSmall
                            radius: Theme.paddingMedium
                            color: "#F2F2F2"

                            Image {
                                x: Theme.paddingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                source: "image://theme/icon-m-car"
                            }

                            Column {
                                anchors {
                                    left: parent.left
                                    leftMargin: Theme.paddingMedium
                                        + Theme.iconSizeMedium + Theme.paddingMedium
                                    verticalCenter: parent.verticalCenter
                                }

                                Label {
                                    text: qsTr("Наземный транспорт")
                                    color: "#212121"
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                Label {
                                    text: qsTr("Следующая %1 ₽").arg(page.fareGround)
                                    color: "#757575"
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                }
                            }

                            Label {
                                anchors {
                                    right: parent.right
                                    rightMargin: Theme.paddingMedium
                                    verticalCenter: parent.verticalCenter
                                }
                                visible: cardReader.state === CardReader.Result
                                text: Math.floor(cardReader.balanceKopecks
                                                 / 100 / page.fareGround)
                                color: "#212121"
                                font.pixelSize: Theme.fontSizeMedium
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: Theme.itemSizeSmall
                            radius: Theme.paddingMedium
                            color: "#F2F2F2"

                            Image {
                                x: Theme.paddingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                source: "image://theme/icon-m-train"
                            }

                            Column {
                                anchors {
                                    left: parent.left
                                    leftMargin: Theme.paddingMedium
                                        + Theme.iconSizeMedium + Theme.paddingMedium
                                    verticalCenter: parent.verticalCenter
                                }

                                Label {
                                    text: qsTr("Метро")
                                    color: "#212121"
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                Label {
                                    text: qsTr("Следующая %1 ₽").arg(page.fareMetro)
                                    color: "#757575"
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                }
                            }

                            Label {
                                anchors {
                                    right: parent.right
                                    rightMargin: Theme.paddingMedium
                                    verticalCenter: parent.verticalCenter
                                }
                                visible: cardReader.state === CardReader.Result
                                text: Math.floor(cardReader.balanceKopecks
                                                 / 100 / page.fareMetro)
                                color: "#212121"
                                font.pixelSize: Theme.fontSizeMedium
                            }
                        }
                    }

                    // ИЗМЕНЕНИЯ: история чтений
                    Column {
                        width: parent.width
                        spacing: Theme.paddingSmall
                        visible: page.currentTab === 1

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: cardReader.history.length === 0
                            text: qsTr("История появится после первого чтения")
                            color: "#9E9E9E"
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        Repeater {
                            model: cardReader.history

                            Rectangle {
                                width: parent.width
                                height: Theme.itemSizeExtraSmall
                                radius: Theme.paddingSmall
                                color: "#F2F2F2"

                                Label {
                                    anchors {
                                        left: parent.left
                                        leftMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }
                                    text: modelData.split("|")[0]
                                    color: "#757575"
                                    font.pixelSize: Theme.fontSizeSmall
                                }

                                Label {
                                    anchors {
                                        right: parent.right
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }
                                    text: modelData.split("|")[1]
                                    color: "#212121"
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.bold: true
                                }
                            }
                        }
                    }

                    // ИНФО
                    Column {
                        width: parent.width
                        spacing: Theme.paddingSmall
                        visible: page.currentTab === 2

                        Repeater {
                            model: [
                                [qsTr("Номер карты"), cardReader.cardNumber],
                                [qsTr("UID карты"), cardReader.uidText],
                                [qsTr("Время чтения"), cardReader.lastReadTime],
                                [qsTr("Версия"), "1.1.0"]
                            ]

                            Item {
                                width: parent.width
                                height: infoValue.height

                                Label {
                                    id: infoLabel
                                    anchors.left: parent.left
                                    text: modelData[0]
                                    color: "#757575"
                                    font.pixelSize: Theme.fontSizeSmall
                                }

                                Label {
                                    id: infoValue
                                    anchors.right: parent.right
                                    text: modelData[1].length > 0 ? modelData[1] : "—"
                                    color: "#212121"
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                            }
                        }
                    }

                    // Ошибка
                    Column {
                        width: parent.width
                        spacing: Theme.paddingMedium
                        visible: cardReader.state === CardReader.Error

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: Theme.fontSizeMedium
                            color: "#D32F2F"
                            text: qsTr("Не удалось прочитать карту")
                        }

                        Label {
                            x: Theme.horizontalPageMargin
                            width: parent.width - 2 * x
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeSmall
                            color: "#757575"
                            text: cardReader.errorText
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: Theme.fontSizeSmall
                            color: "#558B2F"
                            text: qsTr("Попробуйте приложить карту ещё раз")
                        }
                    }
                }
            }

            // Подсказки под белой карточкой
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: "#E6FFFFFF"
                visible: cardReader.state === CardReader.Waiting
                text: qsTr("Приложите карту Подорожник к задней панели телефона")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: "white"
                visible: cardReader.state === CardReader.Waiting && !cardReader.nfcEnabled
                text: qsTr("NFC выключен — включите его в настройках")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: "#E6FFFFFF"
                visible: cardReader.state === CardReader.Result
                text: qsTr("Приложите карту ещё раз, чтобы обновить данные")
            }
        }
    }
}
