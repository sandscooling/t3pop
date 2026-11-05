import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ApplicationWindow {
    id: win
    width: 620
    height: 300
    minimumWidth: 420
    minimumHeight: 220
    visible: true
    flags: Qt.Window | Qt.WindowStaysOnTopHint
    title: qsTr("T3 Pop")

    property bool helpVisible: false
    readonly property bool hasDraft: input.text.trim().length > 0 || backend.images.length > 0
    readonly property string projectName: backend.projects.length > 0 ? backend.projects[backend.projectIndex].name : ""
    readonly property var shortcuts: [
        ["Enter", qsTr("Send to the orchestrator")],
        ["Shift+Enter", qsTr("New line")],
        ["Ctrl+V", qsTr("Paste text or an image")],
        ["Ctrl+Shift+S", qsTr("Screenshot a region")],
        ["Ctrl+1 to 9", qsTr("Choose the project")],
        ["Esc", qsTr("Put away (a draft is kept)")],
        ["Ctrl+Q", qsTr("Quit")],
        ["F1", qsTr("Show or hide shortcuts")]
    ]

    // Called from C++ when the hotkey fires while this popup already exists.
    function summon() {
        win.show()
        win.raise()
        win.requestActivate()
        input.forceActiveFocus()
    }

    // Escape keeps a draft around (hidden) so nothing typed is lost; an empty
    // popup just quits.
    function putAway() {
        if (win.helpVisible)
            win.helpVisible = false
        else if (win.hasDraft)
            win.hide()
        else
            Qt.quit()
    }

    Component.onCompleted: input.forceActiveFocus()

    Connections {
        target: backend
        function onCaptureStarted() { win.hide() }
        function onCaptureFinished() { win.summon() }
        function onSent(threadId) {
            input.clear()
            backend.openT3()
            Qt.quit()
        }
    }

    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: win.putAway() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !backend.busy; context: Qt.ApplicationShortcut; onActivated: backend.capture() }
    Repeater {
        model: 9
        delegate: Item {
            required property int index
            Shortcut {
                sequence: "Ctrl+" + (index + 1)
                enabled: index < backend.projects.length
                context: Qt.ApplicationShortcut
                onActivated: backend.projectIndex = index
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("To") }
            ComboBox {
                id: projectBox
                model: backend.projects
                textRole: "name"
                currentIndex: backend.projectIndex
                onActivated: (i) => { backend.projectIndex = i; input.forceActiveFocus() }
                Layout.preferredWidth: 180
            }
            Label { text: qsTr("orchestrator"); opacity: 0.7 }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("F1 for shortcuts"); opacity: 0.5 }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: input
                wrapMode: TextEdit.Wrap
                enabled: !backend.busy
                placeholderText: qsTr("Message the %1 orchestrator. Paste or capture screenshots.").arg(win.projectName)

                Keys.onPressed: (event) => {
                    if (event.matches(StandardKey.Paste) && backend.pasteImage()) {
                        event.accepted = true
                    } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                               && !(event.modifiers & Qt.ShiftModifier)) {
                        backend.send(input.text)
                        event.accepted = true
                    }
                }
            }
        }

        // Thumbnails of the screenshots going with this message.
        Flow {
            Layout.fillWidth: true
            spacing: 8
            visible: backend.images.length > 0
            Repeater {
                model: backend.images
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    width: 96
                    height: 64
                    color: palette.base
                    border.color: palette.mid
                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: "file://" + modelData
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    ToolButton {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        width: 22
                        height: 22
                        icon.name: "edit-delete-remove"
                        onClicked: backend.removeImage(index)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove")
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: backend.error.length > 0
            text: backend.error
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Screenshot")
                icon.name: "spectacle"
                enabled: !backend.busy
                onClicked: backend.capture()
            }
            Item { Layout.fillWidth: true }
            BusyIndicator { running: backend.busy; visible: running; implicitHeight: 28; implicitWidth: 28 }
            Button {
                text: qsTr("Send")
                icon.name: "document-send"
                highlighted: true
                enabled: !backend.busy && win.hasDraft
                onClicked: backend.send(input.text)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: win.helpVisible
        color: Qt.alpha(palette.window, 0.95)

        MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

        GridLayout {
            anchors.centerIn: parent
            columns: 2
            columnSpacing: 24
            rowSpacing: 6

            Repeater {
                model: win.shortcuts
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: modelData[0]
                    font.bold: true
                    Layout.row: index
                    Layout.column: 0
                    Layout.alignment: Qt.AlignRight
                }
            }
            Repeater {
                model: win.shortcuts
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: modelData[1]
                    Layout.row: index
                    Layout.column: 1
                }
            }
        }
    }
}
