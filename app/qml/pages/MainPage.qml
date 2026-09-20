import QtQuick 2.0
import QtGraphicalEffects 1.0
import Sailfish.Silica 1.0
import Aurora.Controls 1.0
import MyTravelPass 1.0

Page {
    id: page

    // Тариф СПб с 01.01.2026 по карте «Подорожник», ₽ за поездку — для
    // счётчика «хватит на N поездок». Разовая поездка в метро и наземном
    // транспорте стоит одинаково — 65 ₽.
    readonly property int fare: 65

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
        // После успешного чтения показываем тип распознанной карты
        headerText: cardReader.state === CardReader.Result
                    && cardReader.cardTypeName.length > 0
                    ? cardReader.cardTypeName : qsTr("Мой проездной")

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

        // Плашка: баланс и время чтения. Цвет — по типу карты:
        // зелёный «Подорожник», бирюзовый «Тройка»
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
                GradientStop {
                    position: 0.0
                    color: cardReader.cardKind === CardReader.KindTroika
                           ? "#4DD0E1" : "#7CB342"
                }
                GradientStop {
                    position: 1.0
                    color: cardReader.cardKind === CardReader.KindTroika
                           ? "#0097A7" : "#558B2F"
                }
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

        // Вкладки (с увеличенным отступом снизу)
        Item {
            id: tabs
            anchors {
                top: plate.bottom
                topMargin: Theme.paddingMedium
            }
            width: parent.width
            height: tabsRow.height + 2 * Theme.paddingLarge

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
                text: qsTr("Приложите карту к считывателю NFC")
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

                        // ОСТАТОК: счётчик поездок (тариф в метро и наземном
                        // транспорте одинаковый — одна общая полоса)
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
                                // Тариф и счётчик поездок — СПб, для Тройки скрываем
                                visible: cardReader.state === CardReader.Result
                                         && cardReader.cardKind === CardReader.KindPodorozhnik

                                // Знак рубля вместо иконки (в теме его нет)
                                Label {
                                    id: rubleSign
                                    x: Theme.paddingMedium
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Theme.iconSizeMedium
                                    horizontalAlignment: Text.AlignHCenter
                                    text: "₽"
                                    color: Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeExtraLarge
                                }

                                Column {
                                    anchors {
                                        left: rubleSign.right
                                        leftMargin: Theme.paddingMedium
                                        right: tripCountLabel.left
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: qsTr("Поездки по кошельку")
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    Label {
                                        text: qsTr("Следующая %1 ₽").arg(page.fare)
                                        color: Theme.secondaryColor
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                    }
                                }

                                Label {
                                    id: tripCountLabel
                                    anchors {
                                        right: parent.right
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }
                                    visible: cardReader.state === CardReader.Result
                                    text: Math.floor(cardReader.balanceKopecks
                                                     / 100 / page.fare)
                                    color: Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeMedium
                                }
                            }

                            // Поездки по проездному (билетная зона, сектор 9)
                            Rectangle {
                                width: parent.width
                                height: Theme.itemSizeSmall
                                radius: Theme.paddingMedium
                                color: page.plateColor
                                visible: cardReader.state === CardReader.Result
                                         && cardReader.cardKind === CardReader.KindPodorozhnik

                                // Автобус (своей иконкой, в теме её нет) + метро
                                Row {
                                    id: passRidesIcons
                                    x: Theme.paddingMedium
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Theme.paddingSmall / 2

                                    Image {
                                        id: passBusIconSource
                                        visible: false
                                        source: "../icons/icon-m-bus.png"
                                    }

                                    ColorOverlay {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Theme.iconSizeSmall
                                        height: Theme.iconSizeSmall
                                        source: passBusIconSource
                                        color: Theme.primaryColor
                                    }

                                    Image {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Theme.iconSizeSmall
                                        height: Theme.iconSizeSmall
                                        source: "image://theme/icon-m-train"
                                    }
                                }

                                Column {
                                    anchors {
                                        left: passRidesIcons.right
                                        leftMargin: Theme.paddingMedium
                                        right: passRidesLabel.left
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: qsTr("Поездки по проездному")
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    Label {
                                        text: qsTr("Метро + Наземный")
                                        color: Theme.secondaryColor
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                    }
                                }

                                Label {
                                    id: passRidesLabel
                                    anchors {
                                        right: parent.right
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }
                                    text: cardReader.passRides.length > 0
                                          ? cardReader.passRides : "—"
                                    color: Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeMedium
                                }
                            }

                            // Остаток дней проездного (билетная зона, сектор 8)
                            Rectangle {
                                width: parent.width
                                height: Theme.itemSizeSmall
                                radius: Theme.paddingMedium
                                color: page.plateColor
                                visible: cardReader.state === CardReader.Result
                                         && cardReader.cardKind === CardReader.KindPodorozhnik

                                Image {
                                    x: Theme.paddingMedium
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: "image://theme/icon-m-calendar-day"
                                }

                                Column {
                                    anchors {
                                        left: parent.left
                                        leftMargin: Theme.paddingMedium
                                            + Theme.iconSizeMedium + Theme.paddingMedium
                                        right: passDaysLabel.left
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: qsTr("Остаток дней")
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    Label {
                                        text: qsTr("Проездной")
                                        color: Theme.secondaryColor
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                    }
                                }

                                Label {
                                    id: passDaysLabel
                                    anchors {
                                        right: parent.right
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }
                                    text: cardReader.passDaysLeft >= 0
                                          ? cardReader.passDaysLeft : "—"
                                    color: Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeMedium
                                }
                            }

                            // Последняя поездка
                            Rectangle {
                                width: parent.width
                                height: Theme.itemSizeSmall
                                radius: Theme.paddingMedium
                                color: page.plateColor
                                visible: cardReader.state === CardReader.Result
                                         && cardReader.lastTripWhen.length > 0

                                Image {
                                    x: Theme.paddingMedium
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: cardReader.lastTripIsMetro
                                    source: "image://theme/icon-m-train"
                                }

                                Image {
                                    id: tripBusIconSource
                                    visible: false
                                    source: "../icons/icon-m-bus.png"
                                }

                                ColorOverlay {
                                    x: Theme.paddingMedium
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Theme.iconSizeMedium
                                    height: Theme.iconSizeMedium
                                    visible: !cardReader.lastTripIsMetro
                                    source: tripBusIconSource
                                    color: Theme.primaryColor
                                }

                                Column {
                                    anchors {
                                        left: parent.left
                                        leftMargin: Theme.paddingMedium
                                            + Theme.iconSizeMedium + Theme.paddingMedium
                                        right: tripFareLabel.left
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: qsTr("Последняя поездка")
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    Label {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        // У «Тройки» вид транспорта может быть
                                        // неизвестен — без висячего разделителя
                                        text: cardReader.lastTripTransport.length > 0
                                              ? cardReader.lastTripTransport
                                                + " · " + cardReader.lastTripWhen
                                              : cardReader.lastTripWhen
                                        color: Theme.secondaryColor
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                    }
                                }

                                Label {
                                    id: tripFareLabel
                                    anchors {
                                        right: parent.right
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }
                                    text: cardReader.lastTripFare
                                    color: Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeMedium
                                }
                            }

                            // Последнее пополнение
                            Rectangle {
                                width: parent.width
                                height: Theme.itemSizeSmall
                                radius: Theme.paddingMedium
                                color: page.plateColor
                                visible: cardReader.state === CardReader.Result
                                         && cardReader.lastTopupWhen.length > 0

                                Image {
                                    x: Theme.paddingMedium
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: "image://theme/icon-m-add"
                                }

                                Column {
                                    anchors {
                                        left: parent.left
                                        leftMargin: Theme.paddingMedium
                                            + Theme.iconSizeMedium + Theme.paddingMedium
                                        right: topupAmountLabel.left
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: qsTr("Последнее пополнение")
                                        color: Theme.primaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    Label {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        text: cardReader.lastTopupWhen
                                        color: Theme.secondaryColor
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                    }
                                }

                                Label {
                                    id: topupAmountLabel
                                    anchors {
                                        right: parent.right
                                        rightMargin: Theme.paddingMedium
                                        verticalCenter: parent.verticalCenter
                                    }
                                    text: cardReader.lastTopupAmount
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

                                    // «дата|баланс|тип»; у записей до 1.3.2
                                    // типа нет — показываем только дату
                                    property var parts: modelData.split("|")

                                    Label {
                                        anchors {
                                            left: parent.left
                                            leftMargin: Theme.paddingMedium
                                            verticalCenter: parent.verticalCenter
                                        }
                                        text: parent.parts.length > 2
                                              && parent.parts[2].length > 0
                                              ? parent.parts[2] + " · " + parent.parts[0]
                                              : parent.parts[0]
                                        color: Theme.secondaryColor
                                        font.pixelSize: Theme.fontSizeSmall
                                    }

                                    Label {
                                        anchors {
                                            right: parent.right
                                            rightMargin: Theme.paddingMedium
                                            verticalCenter: parent.verticalCenter
                                        }
                                        text: parent.parts[1]
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
                                // Счётчики поездок и тарифы — специфичны для
                                // «Подорожника»; для «Тройки» не показываем
                                model: {
                                    var rows = [
                                        [qsTr("Номер карты"), cardReader.cardNumber],
                                        [qsTr("UID карты"), cardReader.uidText],
                                        [qsTr("Время чтения"), cardReader.lastReadTime]
                                    ]
                                    if (cardReader.cardKind === CardReader.KindPodorozhnik) {
                                        rows.push([cardReader.tripsPeriod.length > 0
                                                   ? qsTr("Поездок на метро (%1)").arg(cardReader.tripsPeriod)
                                                   : qsTr("Поездок на метро"),
                                                   String(cardReader.subwayTrips)])
                                        rows.push([cardReader.tripsPeriod.length > 0
                                                   ? qsTr("Поездок на наземном (%1)").arg(cardReader.tripsPeriod)
                                                   : qsTr("Поездок на наземном"),
                                                   String(cardReader.groundTrips)])
                                        rows.push([qsTr("Разовая поездка"), "65 ₽"])
                                        rows.push([qsTr("Пересадки (60 мин)"), "65 + 14 ₽, далее 0 ₽"])
                                    }
                                    rows.push([qsTr("Версия"), "1.4.0"])
                                    return rows
                                }

                                Rectangle {
                                    width: parent.width
                                    height: infoColumn.height + 2 * Theme.paddingSmall
                                    radius: Theme.paddingSmall
                                    color: page.plateColor
                                    visible: cardReader.state === CardReader.Result

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
