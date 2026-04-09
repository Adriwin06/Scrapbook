import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1640
    height: 980
    minimumWidth: 1280
    minimumHeight: 820
    visible: true
    title: "Scrapbook"
    color: "#0a0a0a"

    readonly property color chrome: "#0a0a0a"
    readonly property color panel: "#141414"
    readonly property color panelRaised: "#1b1b1b"
    readonly property color panelInset: "#101010"
    readonly property color edge: "#2c2c2c"
    readonly property color edgeStrong: "#4e4e4e"
    readonly property color textPrimary: "#f4f4f4"
    readonly property color textMuted: "#b2b2b2"
    readonly property color textFaint: "#747474"
    readonly property color textInverse: "#121212"
    readonly property string uiFont: Qt.platform.os === "windows" ? "Segoe UI Variable Display" : "Noto Sans"
    readonly property string bodyFont: Qt.platform.os === "windows" ? "Segoe UI Variable Text" : "Noto Sans"
    readonly property string monoFont: Qt.platform.os === "windows" ? "Cascadia Mono" : "Noto Sans Mono"

    component Surface: Rectangle {
        radius: 20
        color: panel
        border.color: edge
        border.width: 1
    }

    component InsetSurface: Rectangle {
        radius: 16
        color: panelRaised
        border.color: edge
        border.width: 1
    }

    component SectionLabel: Text {
        color: textFaint
        font.family: bodyFont
        font.pixelSize: 12
        font.weight: Font.Medium
        font.letterSpacing: 0.3
    }

    component MonoButton: Button {
        id: control
        property bool emphasized: false

        implicitHeight: 40
        font.family: bodyFont
        font.pixelSize: 14
        hoverEnabled: true
        padding: 12

        background: Rectangle {
            radius: 12
            color: !control.enabled
                ? panelRaised
                : control.emphasized
                    ? (control.down ? "#d8d8d8" : (control.hovered ? "#ffffff" : "#f2f2f2"))
                    : (control.down ? "#262626" : (control.hovered ? "#202020" : panelRaised))
            border.color: control.emphasized ? "#ffffff" : (control.activeFocus ? edgeStrong : edge)
            border.width: 1
        }

        contentItem: Text {
            text: control.text
            color: control.enabled ? (control.emphasized ? textInverse : textPrimary) : textFaint
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component MonoField: TextField {
        id: control

        implicitHeight: 42
        font.family: bodyFont
        font.pixelSize: 14
        color: textPrimary
        selectByMouse: true
        padding: 12
        placeholderTextColor: textFaint

        background: Rectangle {
            radius: 12
            color: panelInset
            border.color: control.activeFocus ? edgeStrong : edge
            border.width: 1
        }
    }

    component MonoComboBox: ComboBox {
        id: control

        implicitHeight: 42
        font.family: bodyFont
        font.pixelSize: 14
        leftPadding: 12
        rightPadding: 36

        delegate: ItemDelegate {
            width: control.width - 12
            height: 36
            highlighted: control.highlightedIndex === index
            padding: 10

            background: Rectangle {
                radius: 10
                color: parent.highlighted ? "#262626" : "transparent"
            }

            contentItem: Text {
                text: modelData
                color: textPrimary
                font.family: bodyFont
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        indicator: Text {
            x: control.width - width - 12
            anchors.verticalCenter: parent.verticalCenter
            text: "\u25BE"
            color: textMuted
            font.family: bodyFont
            font.pixelSize: 12
        }

        contentItem: Text {
            text: control.displayText
            color: textPrimary
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 12
            color: panelInset
            border.color: control.popup.visible || control.activeFocus ? edgeStrong : edge
            border.width: 1
        }

        popup: Popup {
            y: control.height + 6
            width: control.width
            padding: 6

            background: Rectangle {
                radius: 14
                color: panel
                border.color: edge
                border.width: 1
            }

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: control.popup.visible ? control.delegateModel : null
                currentIndex: control.highlightedIndex
            }
        }
    }

    component MonoSpinBox: SpinBox {
        id: control

        implicitWidth: 112
        implicitHeight: 40
        editable: true
        font.family: bodyFont
        font.pixelSize: 14
        leftPadding: 12
        rightPadding: 34

        validator: IntValidator {
            bottom: Math.min(control.from, control.to)
            top: Math.max(control.from, control.to)
        }

        contentItem: TextInput {
            text: control.displayText
            font: control.font
            color: textPrimary
            selectionColor: textPrimary
            selectedTextColor: textInverse
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            validator: control.validator
            readOnly: !control.editable
            selectByMouse: true
            inputMethodHints: Qt.ImhDigitsOnly
        }

        background: Rectangle {
            radius: 12
            color: panelInset
            border.color: control.activeFocus ? edgeStrong : edge
            border.width: 1
        }

        up.indicator: Rectangle {
            x: control.width - width
            y: 0
            width: 30
            height: control.height / 2
            color: control.up.pressed ? "#262626" : panelRaised
            border.color: edge
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "+"
                color: textPrimary
                font.family: bodyFont
                font.pixelSize: 13
                font.weight: Font.Bold
            }
        }

        down.indicator: Rectangle {
            x: control.width - width
            y: control.height / 2
            width: 30
            height: control.height / 2
            color: control.down.pressed ? "#262626" : panelRaised
            border.color: edge
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "-"
                color: textPrimary
                font.family: bodyFont
                font.pixelSize: 15
                font.weight: Font.Bold
            }
        }
    }

    component MonoCheckBox: CheckBox {
        id: control
        font.family: bodyFont
        font.pixelSize: 13
        spacing: 8

        indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: 18
            radius: 5
            color: control.checked ? textPrimary : "transparent"
            border.color: control.checked ? textPrimary : edgeStrong
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: control.checked ? "x" : ""
                color: textInverse
                font.family: bodyFont
                font.pixelSize: 10
                font.weight: Font.Bold
            }
        }

        contentItem: Text {
            text: control.text
            color: control.enabled ? textMuted : textFaint
            font: control.font
            leftPadding: control.indicator.width + control.spacing
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    component MonoTextArea: TextArea {
        id: control
        property bool monospace: false

        font.family: monospace ? monoFont : bodyFont
        font.pixelSize: 14
        color: textPrimary
        selectByMouse: true
        padding: 12
        wrapMode: TextArea.Wrap
        placeholderTextColor: textFaint

        background: Rectangle {
            radius: 14
            color: panelInset
            border.color: control.activeFocus ? edgeStrong : edge
            border.width: 1
        }
    }

    component RailScrollBar: ScrollBar {
        policy: ScrollBar.AsNeeded
        implicitWidth: 8

        background: Item {}

        contentItem: Rectangle {
            implicitWidth: 6
            radius: 3
            color: parent.pressed ? "#d4d4d4" : "#5f5f5f"
        }
    }

    background: Rectangle {
        color: chrome

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0a0a0a" }
            GradientStop { position: 0.55; color: "#0d0d0d" }
            GradientStop { position: 1.0; color: "#121212" }
        }

        Rectangle {
            x: -180
            y: -180
            width: 540
            height: 540
            radius: 270
            color: "#ffffff"
            opacity: 0.018
        }

        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: -180
            width: 620
            height: 620
            radius: 310
            color: "#ffffff"
            opacity: 0.012
        }
    }

    FolderDialog {
        id: sourceFolderDialog
        title: "Choose source atlas folder"
        onAccepted: pipelineController.sourceDirectory = selectedFolder.toLocalFile()
    }

    FolderDialog {
        id: workspaceFolderDialog
        title: "Choose workspace root"
        onAccepted: pipelineController.workspaceRoot = selectedFolder.toLocalFile()
    }

    FileDialog {
        id: toolFileDialog
        title: "Choose libatlas_tool"
        fileMode: FileDialog.OpenFile
        onAccepted: pipelineController.toolPath = selectedFile.toLocalFile()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Surface {
            Layout.fillWidth: true
            implicitHeight: 96

            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Scrapbook"
                        color: textPrimary
                        font.family: uiFont
                        font.pixelSize: 32
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: "Pipeline execution and duplicate review."
                        color: textMuted
                        font.family: bodyFont
                        font.pixelSize: 14
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    radius: 16
                    color: panelRaised
                    border.color: edge
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: pipelineController.pipelineRunning ? "#f4f4f4" : "#9a9a9a"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            SectionLabel {
                                text: pipelineController.pipelineRunning ? "Pipeline running" : "Pipeline idle"
                            }

                            Text {
                                Layout.fillWidth: true
                                text: pipelineController.status
                                color: textPrimary
                                font.family: bodyFont
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Surface {
                Layout.preferredWidth: 380
                Layout.minimumWidth: 340
                Layout.fillHeight: true

                ScrollView {
                    id: leftScroll
                    anchors.fill: parent
                    anchors.margins: 18
                    clip: true
                    ScrollBar.vertical: RailScrollBar {}

                    ColumnLayout {
                        width: leftScroll.availableWidth
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "Pipeline"
                                color: textPrimary
                                font.family: uiFont
                                font.pixelSize: 24
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "Paths, build configuration, and pipeline controls."
                                color: textMuted
                                font.family: bodyFont
                                font.pixelSize: 14
                                wrapMode: Text.Wrap
                            }
                        }

                        InsetSurface {
                            Layout.fillWidth: true
                            implicitHeight: pathsContent.implicitHeight + 32

                            ColumnLayout {
                                id: pathsContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 16
                                spacing: 12

                                SectionLabel { text: "Paths" }

                                Text { text: "Source atlas folder"; color: textMuted; font.family: bodyFont; font.pixelSize: 13 }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    MonoField { Layout.fillWidth: true; text: pipelineController.sourceDirectory; onEditingFinished: pipelineController.sourceDirectory = text }
                                    MonoButton { text: "Browse"; onClicked: sourceFolderDialog.open() }
                                }

                                Text { text: "Workspace root"; color: textMuted; font.family: bodyFont; font.pixelSize: 13 }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    MonoField { Layout.fillWidth: true; text: pipelineController.workspaceRoot; onEditingFinished: pipelineController.workspaceRoot = text }
                                    MonoButton { text: "Browse"; onClicked: workspaceFolderDialog.open() }
                                }

                                Text { text: "libatlas tool override"; color: textMuted; font.family: bodyFont; font.pixelSize: 13 }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    MonoField { Layout.fillWidth: true; text: pipelineController.toolPath; placeholderText: "Optional"; onEditingFinished: pipelineController.toolPath = text }
                                    MonoButton { text: "Browse"; onClicked: toolFileDialog.open() }
                                }
                            }
                        }

                        InsetSurface {
                            Layout.fillWidth: true
                            implicitHeight: configContent.implicitHeight + 32

                            ColumnLayout {
                                id: configContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 16
                                spacing: 12

                                SectionLabel { text: "Configuration" }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text { text: "Config"; color: textMuted; font.family: bodyFont; font.pixelSize: 13 }
                                        MonoComboBox {
                                            Layout.fillWidth: true
                                            model: ["Debug", "Release"]
                                            currentIndex: model.indexOf(pipelineController.config)
                                            onActivated: pipelineController.config = currentText
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text { text: "Split mode"; color: textMuted; font.family: bodyFont; font.pixelSize: 13 }
                                        MonoComboBox {
                                            Layout.fillWidth: true
                                            model: ["components", "auto", "bbox"]
                                            currentIndex: model.indexOf(pipelineController.splitMode)
                                            onActivated: pipelineController.splitMode = currentText
                                        }
                                    }
                                }

                                Text { text: "Similarity max pairs"; color: textMuted; font.family: bodyFont; font.pixelSize: 13 }
                                MonoField {
                                    Layout.fillWidth: true
                                    text: String(pipelineController.similarityMaxPairs)
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    onEditingFinished: {
                                        const nextValue = parseInt(text)
                                        if (!isNaN(nextValue)) {
                                            pipelineController.similarityMaxPairs = nextValue
                                        }
                                    }
                                }
                            }
                        }

                        InsetSurface {
                            Layout.fillWidth: true
                            implicitHeight: workspaceContent.implicitHeight + 32

                            ColumnLayout {
                                id: workspaceContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 16
                                spacing: 10

                                SectionLabel { text: "Workspace" }

                                Text { text: "Asset store"; color: textFaint; font.family: bodyFont; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: pipelineController.assetStoreDirectory; color: textMuted; font.family: monoFont; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
                                Rectangle { Layout.fillWidth: true; height: 1; color: edge }

                                Text { text: "Logical store"; color: textFaint; font.family: bodyFont; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: pipelineController.logicalStoreDirectory; color: textMuted; font.family: monoFont; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
                                Rectangle { Layout.fillWidth: true; height: 1; color: edge }

                                Text { text: "Pipeline work dir"; color: textFaint; font.family: bodyFont; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: pipelineController.pipelineWorkDirectory; color: textMuted; font.family: monoFont; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            MonoButton {
                                Layout.fillWidth: true
                                emphasized: true
                                text: pipelineController.pipelineRunning ? "Running" : "Run pipeline"
                                enabled: !pipelineController.pipelineRunning
                                onClicked: pipelineController.runPipeline()
                            }

                            MonoButton {
                                Layout.fillWidth: true
                                text: "Refresh groups"
                                onClicked: pipelineController.refreshReviewGroups()
                            }
                        }

                        MonoButton {
                            Layout.fillWidth: true
                            text: "Open logical store"
                            onClicked: pipelineController.openLogicalStore()
                        }

                        InsetSurface {
                            Layout.fillWidth: true
                            implicitHeight: 300

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 10

                                Text {
                                    text: "Pipeline log"
                                    color: textPrimary
                                    font.family: uiFont
                                    font.pixelSize: 20
                                    font.weight: Font.DemiBold
                                }

                                MonoTextArea {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    monospace: true
                                    readOnly: true
                                    wrapMode: TextArea.WrapAnywhere
                                    placeholderText: "Pipeline output will appear here."
                                    text: pipelineController.logText
                                }
                            }
                        }
                    }
                }
            }

            Surface {
                Layout.preferredWidth: 300
                Layout.minimumWidth: 260
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Text {
                        text: "Review groups"
                        color: textPrimary
                        font.family: uiFont
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: pipelineController.reviewGroupModel.count + " unresolved groups"
                        color: textMuted
                        font.family: bodyFont
                        font.pixelSize: 14
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 10
                        model: pipelineController.reviewGroupModel
                        ScrollBar.vertical: RailScrollBar {}

                        delegate: Rectangle {
                            required property int index
                            required property string groupId
                            required property int logicalIdCount
                            required property int memberOccurrenceCount

                            property bool selected: index === pipelineController.currentGroupIndex

                            width: ListView.view.width - 8
                            height: 88
                            radius: 14
                            color: selected ? "#202020" : panelRaised
                            border.color: selected ? edgeStrong : edge
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: groupId
                                    color: textPrimary
                                    font.family: bodyFont
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: logicalIdCount + " logical IDs"
                                    color: textMuted
                                    font.family: bodyFont
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: memberOccurrenceCount + " occurrences"
                                    color: textFaint
                                    font.family: bodyFont
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: pipelineController.selectReviewGroup(index)
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: parent.count === 0
                            text: "No review groups loaded."
                            color: textFaint
                            font.family: bodyFont
                            font.pixelSize: 14
                        }
                    }
                }
            }

            Surface {
                Layout.fillWidth: true
                Layout.minimumWidth: 560
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            Layout.fillWidth: true
                            text: pipelineController.hasCurrentGroup ? pipelineController.currentGroupTitle : "No unresolved review groups."
                            color: textPrimary
                            font.family: uiFont
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }

                        MonoButton {
                            text: "Save"
                            emphasized: true
                            enabled: pipelineController.hasCurrentGroup
                            onClicked: pipelineController.saveCurrentDecision()
                        }

                        MonoButton {
                            text: "Save and rerun"
                            enabled: pipelineController.hasCurrentGroup && !pipelineController.pipelineRunning
                            onClicked: pipelineController.saveCurrentDecisionAndRerun()
                        }

                        MonoButton {
                            text: "Reload"
                            enabled: pipelineController.hasCurrentGroup
                            onClicked: pipelineController.reloadCurrentGroup()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 320
                        spacing: 12

                        InsetSurface {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 10

                                Text {
                                    text: "Contact sheet"
                                    color: textPrimary
                                    font.family: uiFont
                                    font.pixelSize: 20
                                    font.weight: Font.DemiBold
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 14
                                    color: panelInset
                                    border.color: edge
                                    border.width: 1

                                    Item {
                                        anchors.fill: parent
                                        anchors.margins: 12

                                        Image {
                                            id: contactSheetImage
                                            anchors.fill: parent
                                            source: pipelineController.currentContactSheetUrl
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            visible: source.toString().length > 0
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: !contactSheetImage.visible
                                            text: pipelineController.hasCurrentGroup
                                                ? "No contact sheet available for this group."
                                                : "Select a review group to inspect its contact sheet."
                                            color: textFaint
                                            font.family: bodyFont
                                            font.pixelSize: 14
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                }
                            }
                        }

                        InsetSurface {
                            Layout.preferredWidth: 300
                            Layout.fillHeight: true

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 10

                                Text {
                                    text: "Decision notes"
                                    color: textPrimary
                                    font.family: uiFont
                                    font.pixelSize: 20
                                    font.weight: Font.DemiBold
                                }

                                MonoTextArea {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    enabled: pipelineController.hasCurrentGroup
                                    placeholderText: "Describe the merge or split decision."
                                    text: pipelineController.decisionNotes
                                    onTextChanged: {
                                        if (pipelineController.decisionNotes !== text) {
                                            pipelineController.decisionNotes = text
                                        }
                                    }
                                }
                            }
                        }
                    }

                    InsetSurface {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: "Members"
                                    color: textPrimary
                                    font.family: uiFont
                                    font.pixelSize: 20
                                    font.weight: Font.DemiBold
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: pipelineController.reviewMemberModel.count + " loaded"
                                    color: textFaint
                                    font.family: bodyFont
                                    font.pixelSize: 13
                                }
                            }

                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 10
                                model: pipelineController.reviewMemberModel
                                ScrollBar.vertical: RailScrollBar {}

                                delegate: Rectangle {
                                    required property int index
                                    required property string logicalId
                                    required property string representativeName
                                    required property string representativeExactId
                                    required property url reviewImageUrl
                                    required property int occurrenceCount
                                    required property int clusterIndex
                                    required property bool representative
                                    required property bool mergedBySimilarity
                                    required property bool mergedByReview

                                    width: ListView.view.width - 8
                                    height: 142
                                    radius: 14
                                    color: panelInset
                                    border.color: representative ? edgeStrong : edge
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 14

                                        Rectangle {
                                            Layout.preferredWidth: 104
                                            Layout.preferredHeight: 104
                                            radius: 12
                                            color: "#171717"
                                            border.color: edge
                                            border.width: 1

                                            Item {
                                                anchors.fill: parent
                                                anchors.margins: 8

                                                Image {
                                                    id: memberImage
                                                    anchors.fill: parent
                                                    source: reviewImageUrl
                                                    fillMode: Image.PreserveAspectFit
                                                    smooth: true
                                                    visible: source.toString().length > 0
                                                }

                                                Text {
                                                    anchors.centerIn: parent
                                                    visible: !memberImage.visible
                                                    text: "No image"
                                                    color: textFaint
                                                    font.family: bodyFont
                                                    font.pixelSize: 12
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 6

                                            Text {
                                                Layout.fillWidth: true
                                                text: representativeName
                                                color: textPrimary
                                                font.family: bodyFont
                                                font.pixelSize: 16
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: logicalId
                                                color: textMuted
                                                font.family: monoFont
                                                font.pixelSize: 12
                                                elide: Text.ElideMiddle
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                visible: representativeExactId.length > 0
                                                text: representativeExactId
                                                color: textFaint
                                                font.family: monoFont
                                                font.pixelSize: 11
                                                elide: Text.ElideMiddle
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: occurrenceCount + " occurrences"
                                                color: textMuted
                                                font.family: bodyFont
                                                font.pixelSize: 13
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: {
                                                    let flags = []
                                                    if (representative)
                                                        flags.push("Representative")
                                                    if (mergedBySimilarity)
                                                        flags.push("Similarity merge")
                                                    if (mergedByReview)
                                                        flags.push("Review merge")
                                                    return flags.length > 0 ? flags.join(" • ") : "No flags"
                                                }
                                                color: textFaint
                                                font.family: bodyFont
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.preferredWidth: 164
                                            spacing: 8

                                            SectionLabel { text: "Decision group" }

                                            MonoSpinBox {
                                                from: 1
                                                to: Math.max(1, pipelineController.reviewMemberModel.count)
                                                value: clusterIndex
                                                onValueModified: pipelineController.reviewMemberModel.setClusterIndex(index, value)
                                            }

                                            MonoCheckBox {
                                                checked: representative
                                                text: "Representative"
                                                onToggled: pipelineController.reviewMemberModel.setRepresentative(index, checked)
                                            }
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.count === 0
                                    text: pipelineController.hasCurrentGroup
                                        ? "No members loaded for this group."
                                        : "Select a group to review its members."
                                    color: textFaint
                                    font.family: bodyFont
                                    font.pixelSize: 14
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
