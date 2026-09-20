import QtQuick 2.0
import Sailfish.Silica 1.0
import MyTravelPass 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        spacing: Theme.paddingSmall

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: cardReader.state === CardReader.Result
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            text: cardReader.cardTypeName
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: cardReader.state === CardReader.Result
                            ? Theme.fontSizeLarge : Theme.fontSizeMedium
            color: Theme.highlightColor
            text: cardReader.state === CardReader.Result
                  ? cardReader.balanceText : qsTr("Мой проездной")
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: cardReader.state === CardReader.Result
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            text: qsTr("баланс")
        }
    }
}
