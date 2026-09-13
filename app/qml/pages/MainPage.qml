import QtQuick 2.0
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
import PodorozhnikBalance 1.0

Page {
    id: page

    // Тарифы СПб с 01.01.2026 по карте «Подорожник», ₽ за поездку — для
    // счётчиков «хватит на N поездок». Разовая поездка в метро и наземном
    // транспорте стоит одинаково — 65 ₽.
    readonly property int fareGround: 65
    readonly property int fareMetro: 65

    // Фон полос: чуть заметная заливка в цвете текущей темы (тёмной или светлой)
    readonly property color plateColor: Theme.rgba(Theme.primaryColor, 0.05)

    property int currentTab: 0

    // Плавный свайп вкладок из «хромовых» зон (плашка, вкладки, аппбар):
    // перетаскиваем содержимое pager'а за пальцем, при отпускании страница
    // доснапывается до ближайшей вкладки
    property real _swipeStartX: 0
    property real _swipeContentX: 0
    property bool _swiping: false

    function swipeStart(x) {
        pager.stopAnim()
        _swipeStartX = x
        _swipeContentX = pager.contentX
        _swiping = false
    }

    function swipeMove(x) {
        var dx = x - _swipeStartX
        if (!_swiping && Math.abs(dx) < Theme.paddingSmall)
            return
        _swiping = true
        pager.contentX = Math.max(0, Math.min(pager.contentWidth - pager.width,
                                              _swipeContentX - dx))
    }

    function swipeEnd() {
        var wasSwiping = _swiping
        _swiping = false
        if (wasSwiping)
            pager.snapToNearest()
    }

    onCurrentTabChanged: pager.animateTo(currentTab)

    // Свайп по аппбару. Лежит под всем контентом страницы
    MouseArea {
        anchors.fill: parent
        z: -1
        onPressed: page.swipeStart(mouseX)
        onPositionChanged: page.swipeMove(mouseX)
        onReleased: page.swipeEnd()
        onCanceled: page.swipeEnd()
    }

    AppBar {
        id: appBar
        headerText: qsTr("Подорожник")

        AppBarSpacer {}

        AppBarButton {
            icon.source: "image://theme/icon-m-delete"
            visible: page.currentTab === 1
            onClicked: cardReader.clearHistory()
        }

        AppBarButton {
            icon.source: "image://theme/icon-m-refresh"
            onClicked: cardReader.refresh()
        }
    }

    Item {
        id: content
        anchors {
            top: appBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        // Свайп по «хромовым» зонам (плашка, вкладки, подсказки).
        // Лежит под pager'ом: его область он обрабатывает сам
        MouseArea {
            anchors.fill: parent
            z: -1
            onPressed: page.swipeStart(mouseX)
            onPositionChanged: page.swipeMove(mouseX)
            onReleased: page.swipeEnd()
            onCanceled: page.swipeEnd()
        }

        // Плашка: баланс и время чтения
        Rectangle {
            id: plate
            anchors {
                top: parent.top
                topMargin: Theme.paddingMedium
                horizontalCenter: parent.horizontalCenter
            }
            width: parent.width - 2 * Theme.horizontalPageMargin
            height: Math.round(width * 0.5)
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
                    color: "#E6FFFFFF"
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

        // Вкладки (с отступами сверху и снизу)
        Item {
            id: tabs
            anchors {
                top: plate.bottom
                topMargin: Theme.paddingMedium
            }
            width: parent.width
            height: tabsRow.height + 2 * Theme.paddingMedium

            Row {
                id: tabsRow
                anchors.centerIn: parent
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
                                   ? Theme.highlightColor : Theme.secondaryColor

                            MouseArea {
                                anchors.fill: parent
                                onClicked: page.currentTab = index
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 3
                            radius: 1
                            color: Theme.highlightColor
                            visible: page.currentTab === index
                        }
                    }
                }
            }
        }

        // Ошибка и подсказки
        Column {
            id: statusColumn
            anchors { top: tabs.bottom }
            width: parent.width
            spacing: Theme.paddingSmall

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: cardReader.state === CardReader.Error

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.errorColor
                    text: qsTr("Не удалось прочитать карту")
                }

                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: cardReader.errorText
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.highlightColor
                    text: qsTr("Попробуйте приложить карту ещё раз")
                }
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                visible: cardReader.state === CardReader.Waiting
                text: qsTr("Приложите карту Подорожник к задней панели телефона")
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.errorColor
                visible: cardReader.state === CardReader.Waiting && !cardReader.nfcEnabled
                text: qsTr("NFC выключен — включите его в настройках")
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                visible: cardReader.state === CardReader.Result
                text: qsTr("Приложите карту ещё раз, чтобы обновить данные")
            }
        }

        // Листаемое содержимое вкладок: горизонтальный pager на всю
        // оставшуюся высоту, страницы следуют за пальцем; длинный
        // контент скроллится внутри страниц
        ListView {
            id: pager
            anchors {
                top: statusColumn.bottom
                topMargin: Theme.paddingMedium
                bottom: parent.bottom
            }
            width: parent.width
            orientation: ListView.Horizontal
            snapMode: ListView.SnapOneItem
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            model: 3

            onMovementStarted: contentXAnim.stop()
            onMovementEnded: {
                // Вид сам доехал до точки снапа — только синхронизируем
                // номер вкладки, свою анимацию не запускаем
                var idx = Math.max(0, Math.min(count - 1,
                                               Math.round(contentX / width)))
                if (page.currentTab !== idx)
                    page.currentTab = idx
            }

            // Вкладка переключается в середине жеста, а не после отпускания
            onContentXChanged: {
                if (contentXAnim.running || (!moving && !page._swiping))
                    return
                var idx = Math.max(0, Math.min(count - 1,
                                               Math.round(contentX / width)))
                if (page.currentTab !== idx)
                    page.currentTab = idx
            }

            function stopAnim() {
                contentXAnim.stop()
            }

            function animateTo(idx) {
                idx = Math.max(0, Math.min(count - 1, idx))
                var target = idx * width
                // Не дёргаем contentX, если вид уже едет сам, идёт наш жест
                // или вид уже стоит на месте
                if (moving || page._swiping || Math.abs(contentX - target) < 1)
                    return
                contentXAnim.to = target
                contentXAnim.restart()
            }

            function snapToNearest() {
                var idx = Math.max(0, Math.min(count - 1,
                                               Math.round(contentX / width)))
                if (page.currentTab !== idx)
                    page.currentTab = idx  // дальше animateTo из onCurrentTabChanged
                else
                    animateTo(idx)
            }

            NumberAnimation on contentX {
                id: contentXAnim
                duration: 200
                easing.type: Easing.OutQuad
            }

            delegate: Item {
                width: pager.width
                height: pager.height

                SilicaFlickable {
                    anchors.fill: parent
                    contentHeight: tabColumn.height
                    clip: true

                    Column {
                        id: tabColumn
                        width: parent.width
                        spacing: Theme.paddingSmall

                        // ОСТАТОК: счётчики поездок
                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 2 * Theme.horizontalPageMargin
                            spacing: Theme.paddingMedium
                            visible: index === 0

                            Rectangle {
                                width: parent.width
                                height: Theme.itemSizeSmall
                                radius: Theme.paddingMedium
                                color: page.plateColor

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
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    Label {
                                        text: qsTr("Следующая %1 ₽").arg(page.fareGround)
                                        color: Theme.secondaryColor
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
                                    color: Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeMedium
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: Theme.itemSizeSmall
                                radius: Theme.paddingMedium
                                color: page.plateColor

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
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    Label {
                                        text: qsTr("Следующая %1 ₽").arg(page.fareMetro)
                                        color: Theme.secondaryColor
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
                                    color: Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeMedium
                                }
                            }
                        }

                        // ИЗМЕНЕНИЯ: история чтений
                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 2 * Theme.horizontalPageMargin
                            spacing: Theme.paddingSmall
                            visible: index === 1

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: cardReader.history.length === 0
                                text: qsTr("История появится после первого чтения")
                                color: Theme.secondaryColor
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            Repeater {
                                model: cardReader.history

                                Rectangle {
                                    width: parent.width
                                    height: Theme.itemSizeExtraSmall
                                    radius: Theme.paddingSmall
                                    color: page.plateColor

                                    Label {
                                        anchors {
                                            left: parent.left
                                            leftMargin: Theme.paddingMedium
                                            verticalCenter: parent.verticalCenter
                                        }
                                        text: modelData.split("|")[0]
                                        color: Theme.secondaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }

                                    Label {
                                        anchors {
                                            right: parent.right
                                            rightMargin: Theme.paddingMedium
                                            verticalCenter: parent.verticalCenter
                                        }
                                        text: modelData.split("|")[1]
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        // ИНФО
                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 2 * Theme.horizontalPageMargin
                            spacing: Theme.paddingSmall
                            visible: index === 2

                            Repeater {
                                model: [
                                    [qsTr("Номер карты"), cardReader.cardNumber],
                                    [qsTr("UID карты"), cardReader.uidText],
                                    [qsTr("Время чтения"), cardReader.lastReadTime],
                                    [qsTr("Разовая поездка"), "65 ₽"],
                                    [qsTr("Пересадки (60 мин)"), "65 + 14 ₽, далее 0 ₽"],
                                    [qsTr("Версия"), "1.2.0"]
                                ]

                                Rectangle {
                                    width: parent.width
                                    height: infoColumn.height + 2 * Theme.paddingSmall
                                    radius: Theme.paddingSmall
                                    color: page.plateColor

                                    // Двухэтажная строка: подпись сверху, значение снизу —
                                    // длинные значения (номер карты) не налезают на подпись
                                    Column {
                                        id: infoColumn
                                        x: Theme.paddingMedium
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width - 2 * Theme.paddingMedium

                                        Label {
                                            text: modelData[0]
                                            color: Theme.secondaryColor
                                            font.pixelSize: Theme.fontSizeExtraSmall
                                        }

                                        Label {
                                            width: parent.width
                                            wrapMode: Text.WrapAnywhere
                                            text: modelData[1].length > 0 ? modelData[1] : "—"
                                            color: Theme.primaryColor
                                            font.pixelSize: Theme.fontSizeSmall
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
