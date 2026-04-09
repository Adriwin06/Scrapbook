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
    property int activePageIndex: 0
    property bool pathsPanelExpanded: true
    property bool configPanelExpanded: true
    property bool workspacePanelExpanded: true
    property var selectedMemberRows: []
    readonly property bool allPipelinePanelsExpanded: pathsPanelExpanded && configPanelExpanded && workspacePanelExpanded

    function clearMemberSelection() {
        selectedMemberRows = []
    }

    function isMemberSelected(row) {
        return selectedMemberRows.indexOf(row) !== -1
    }

    function toggleMemberSelection(row) {
        let nextSelection = selectedMemberRows.slice()
        const existingIndex = nextSelection.indexOf(row)
        if (existingIndex >= 0) {
            nextSelection.splice(existingIndex, 1)
        } else {
            nextSelection.push(row)
            nextSelection.sort(function(left, right) { return left - right })
        }
        selectedMemberRows = nextSelection
    }

    function setPipelinePanelsExpanded(expanded) {
        pathsPanelExpanded = expanded
        configPanelExpanded = expanded
        workspacePanelExpanded = expanded
    }

    function mergeSelectedMembers() {
        if (selectedMemberRows.length >= 2) {
            pipelineController.reviewMemberModel.mergeRows(selectedMemberRows)
            clearMemberSelection()
        }
    }

    function splitSelectedMembers() {
        if (selectedMemberRows.length >= 1) {
            pipelineController.reviewMemberModel.splitRows(selectedMemberRows)
            clearMemberSelection()
        }
    }

    function pickSelectedRepresentative() {
        if (selectedMemberRows.length === 1) {
            pipelineController.reviewMemberModel.chooseRepresentative(selectedMemberRows[0])
        }
    }

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

    component DarkMenu: Menu {
        id: control
        x: 0
        y: parent ? parent.height + 4 : 0
        popupType: Popup.Window
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        delegate: DarkMenuItem {}

        background: Rectangle {
            radius: 14
            color: panel
            border.color: edge
            border.width: 1
        }
    }

    component AppMenu: Menu {
        id: control
        popupType: Popup.Window
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent
        delegate: DarkMenuItem {}

        background: Rectangle {
            radius: 14
            color: panel
            border.color: edge
            border.width: 1
        }
    }

    component DarkMenuItem: MenuItem {
        id: menuItem
        implicitWidth: 220
        implicitHeight: 36
        padding: 10

        contentItem: Text {
            text: menuItem.text
            color: menuItem.enabled ? textPrimary : textFaint
            font.family: bodyFont
            font.pixelSize: 13
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 10
            color: menuItem.highlighted ? "#262626" : "transparent"
        }
    }

    component AppMenuBarItem: MenuBarItem {
        id: control
        implicitWidth: contentItem.implicitWidth + 24
        implicitHeight: 32
        leftPadding: 12
        rightPadding: 12
        topPadding: 0
        bottomPadding: 0
        hoverEnabled: true

        contentItem: Text {
            text: control.text
            color: textPrimary
            font.family: bodyFont
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 6
            color: control.highlighted ? "#202020" : "transparent"
            border.color: control.highlighted ? edgeStrong : "transparent"
            border.width: 1
        }
    }

    component MenuTrigger: Item {
        id: control
        property string text: ""
        property Menu menu
        implicitWidth: triggerButton.implicitWidth
        implicitHeight: triggerButton.implicitHeight

        Button {
            id: triggerButton
            anchors.fill: parent
            hoverEnabled: true
            padding: 10

            background: Rectangle {
                radius: 10
                color: control.menu && control.menu.visible
                    ? "#202020"
                    : (triggerButton.hovered ? "#1b1b1b" : "transparent")
                border.color: control.menu && control.menu.visible ? edgeStrong : edge
                border.width: 1
            }

            contentItem: Text {
                text: control.text
                color: textPrimary
                font.family: bodyFont
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                if (control.menu) {
                    control.menu.visible = !control.menu.visible
                }
            }
        }
    }

    component WorkspaceTabButton: TabButton {
        id: control
        required property int pageIndex
        font.family: bodyFont
        font.pixelSize: 14
        implicitHeight: 46
        onClicked: activePageIndex = pageIndex

        background: Item {
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: activePageIndex === control.pageIndex ? "#f2f2f2" : "transparent"
            }
        }

        contentItem: Text {
            text: control.text
            color: activePageIndex === control.pageIndex ? textPrimary : textMuted
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
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

    component SplitHandle: Rectangle {
        color: "transparent"
        implicitWidth: 10
        implicitHeight: 10

        Rectangle {
            anchors.centerIn: parent
            width: parent.width > parent.height ? 48 : 2
            height: parent.width > parent.height ? 2 : 48
            radius: 1
            color: edgeStrong
            opacity: 0.65
        }
    }

    component CollapsiblePanel: Rectangle {
        id: control
        property string title: ""
        property string subtitle: ""
        property bool expanded: true
        default property alias content: bodyColumn.data

        radius: 16
        color: panelRaised
        border.color: edge
        border.width: 1
        implicitHeight: panelColumn.implicitHeight + 32

        ColumnLayout {
            id: panelColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 12

            Item {
                Layout.fillWidth: true
                implicitHeight: headerLayout.implicitHeight

                RowLayout {
                    id: headerLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    Text {
                        text: control.expanded ? "\u25BE" : "\u25B8"
                        color: textMuted
                        font.family: bodyFont
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: control.title
                            color: textPrimary
                            font.family: bodyFont
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: control.subtitle.length > 0
                            text: control.subtitle
                            color: textFaint
                            font.family: bodyFont
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: control.expanded = !control.expanded
                }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: control.expanded ? bodyColumn.implicitHeight : 0
                visible: control.expanded
                clip: true

                ColumnLayout {
                    id: bodyColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 12
                }
            }
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

    Connections {
        target: pipelineController.reviewMemberModel
        function onCountChanged() {
            window.clearMemberSelection()
        }
    }

    Action {
        id: runPipelineAction
        text: "Run pipeline"
        enabled: !pipelineController.pipelineRunning
        onTriggered: pipelineController.runPipeline()
    }

    Action {
        id: refreshGroupsAction
        text: "Refresh groups"
        onTriggered: pipelineController.refreshReviewGroups()
    }

    Action {
        id: openLogicalStoreAction
        text: "Open logical store"
        onTriggered: pipelineController.openLogicalStore()
    }

    Action {
        id: exitAction
        text: "Exit"
        onTriggered: Qt.quit()
    }

    Action {
        id: clearSelectionAction
        text: "Clear selection"
        enabled: selectedMemberRows.length > 0
        onTriggered: clearMemberSelection()
    }

    Action {
        id: mergeSelectionAction
        text: "Same group"
        enabled: selectedMemberRows.length >= 2
        onTriggered: mergeSelectedMembers()
    }

    Action {
        id: splitSelectionAction
        text: "Split selected"
        enabled: selectedMemberRows.length >= 1
        onTriggered: splitSelectedMembers()
    }

    Action {
        id: toggleRepresentativeAction
        text: "Toggle representative"
        enabled: selectedMemberRows.length === 1
        onTriggered: pipelineController.reviewMemberModel.toggleRepresentative(selectedMemberRows[0])
    }

    Action {
        id: showPipelineWorkspaceAction
        text: "Pipeline workspace"
        checkable: true
        checked: activePageIndex === 0
        onTriggered: activePageIndex = 0
    }

    Action {
        id: showReviewWorkspaceAction
        text: "Review workspace"
        checkable: true
        checked: activePageIndex === 1
        onTriggered: activePageIndex = 1
    }

    Action {
        id: togglePipelineSectionsAction
        text: allPipelinePanelsExpanded ? "Collapse pipeline sections" : "Expand pipeline sections"
        onTriggered: setPipelinePanelsExpanded(!allPipelinePanelsExpanded)
    }

    Action {
        id: saveDecisionAction
        text: "Save decision"
        enabled: pipelineController.hasCurrentGroup
        onTriggered: pipelineController.saveCurrentDecision()
    }

    Action {
        id: saveAndRerunAction
        text: "Save and rerun"
        enabled: pipelineController.hasCurrentGroup && !pipelineController.pipelineRunning
        onTriggered: pipelineController.saveCurrentDecisionAndRerun()
    }

    Action {
        id: reloadGroupAction
        text: "Reload group"
        enabled: pipelineController.hasCurrentGroup
        onTriggered: pipelineController.reloadCurrentGroup()
    }

    Action {
        id: aboutAction
        text: "About Scrapbook"
        enabled: false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Surface {
            Layout.fillWidth: true
            implicitHeight: 72

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Text {
                    text: "Scrapbook"
                    color: textPrimary
                    font.family: uiFont
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter
                }

                Rectangle {
                    width: 1
                    height: 26
                    color: edge
                    Layout.alignment: Qt.AlignVCenter
                }

                Rectangle {
                    Layout.preferredWidth: 720
                    Layout.preferredHeight: 40
                    Layout.alignment: Qt.AlignVCenter
                    radius: 12
                    color: panelRaised
                    border.color: edge
                    border.width: 1

                    MenuBar {
                        id: appMenuBar
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        anchors.topMargin: 4
                        anchors.bottomMargin: 4
                        spacing: 2
                        delegate: AppMenuBarItem {}

                        background: Item {}

                        AppMenu {
                            title: "File"
                            DarkMenuItem { action: openLogicalStoreAction }
                            MenuSeparator {}
                            DarkMenuItem { action: exitAction }
                        }

                        AppMenu {
                            title: "Edit"
                            DarkMenuItem { action: clearSelectionAction }
                            DarkMenuItem { action: mergeSelectionAction }
                            DarkMenuItem { action: splitSelectionAction }
                            DarkMenuItem { action: toggleRepresentativeAction }
                        }

                        AppMenu {
                            title: "View"
                            DarkMenuItem { action: showPipelineWorkspaceAction }
                            DarkMenuItem { action: showReviewWorkspaceAction }
                            MenuSeparator {}
                            DarkMenuItem { action: togglePipelineSectionsAction }
                        }

                        AppMenu {
                            title: "Run"
                            DarkMenuItem { action: runPipelineAction }
                            DarkMenuItem { action: refreshGroupsAction }
                        }

                        AppMenu {
                            title: "Review"
                            DarkMenuItem { action: saveDecisionAction }
                            DarkMenuItem { action: saveAndRerunAction }
                            DarkMenuItem { action: reloadGroupAction }
                        }

                        AppMenu {
                            title: "Help"
                            DarkMenuItem { action: aboutAction }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 240
                    Layout.preferredHeight: 44
                    Layout.alignment: Qt.AlignVCenter
                    radius: 14
                    color: panelRaised
                    border.color: edge
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
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
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        Surface {
            Layout.fillWidth: true
            implicitHeight: 78

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 16

                TabBar {
                    id: workspaceTabs
                    Layout.preferredWidth: 320
                    currentIndex: activePageIndex
                    spacing: 18

                    background: Item {
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: edge
                        }
                    }

                    WorkspaceTabButton {
                        text: "Pipeline"
                        pageIndex: 0
                    }

                    WorkspaceTabButton {
                        text: "Review"
                        pageIndex: 1
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: activePageIndex === 0
                        ? "Pipeline workspace"
                        : "Review workspace"
                    color: textMuted
                    font.family: bodyFont
                    font.pixelSize: 13
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                SplitView {
                    anchors.fill: parent
                    orientation: Qt.Horizontal
                    handle: SplitHandle {}

                    Surface {
                        SplitView.preferredWidth: activePageIndex === 0 ? 380 : 0
                        SplitView.minimumWidth: activePageIndex === 0 ? 340 : 0
                        SplitView.maximumWidth: activePageIndex === 0 ? 520 : 0
                        SplitView.fillHeight: true
                        visible: activePageIndex === 0

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

                        CollapsiblePanel {
                            Layout.fillWidth: true
                            title: "Paths"
                            subtitle: "Input folder, workspace root, and optional tool override."
                            expanded: window.pathsPanelExpanded
                            onExpandedChanged: window.pathsPanelExpanded = expanded

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

                        CollapsiblePanel {
                            Layout.fillWidth: true
                            title: "Configuration"
                            subtitle: "Build config, split strategy, and similarity budget."
                            expanded: window.configPanelExpanded
                            onExpandedChanged: window.configPanelExpanded = expanded

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

                        CollapsiblePanel {
                            Layout.fillWidth: true
                            title: "Workspace"
                            subtitle: "Derived storage paths used by the pipeline and review tooling."
                            expanded: window.workspacePanelExpanded
                            onExpandedChanged: window.workspacePanelExpanded = expanded

                            Text { text: "Asset store"; color: textFaint; font.family: bodyFont; font.pixelSize: 12 }
                            Text { Layout.fillWidth: true; text: pipelineController.assetStoreDirectory; color: textMuted; font.family: monoFont; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
                            Rectangle { Layout.fillWidth: true; height: 1; color: edge }

                            Text { text: "Logical store"; color: textFaint; font.family: bodyFont; font.pixelSize: 12 }
                            Text { Layout.fillWidth: true; text: pipelineController.logicalStoreDirectory; color: textMuted; font.family: monoFont; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
                            Rectangle { Layout.fillWidth: true; height: 1; color: edge }

                            Text { text: "Pipeline work dir"; color: textFaint; font.family: bodyFont; font.pixelSize: 12 }
                            Text { Layout.fillWidth: true; text: pipelineController.pipelineWorkDirectory; color: textMuted; font.family: monoFont; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
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
                    }
                }
            }

                    Surface {
                        SplitView.preferredWidth: activePageIndex === 1 ? 300 : 0
                        SplitView.minimumWidth: activePageIndex === 1 ? 260 : 0
                        SplitView.maximumWidth: activePageIndex === 1 ? 420 : 0
                        SplitView.fillHeight: true
                        visible: activePageIndex === 1

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
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: 560
                        SplitView.fillHeight: true

                StackLayout {
                    anchors.fill: parent
                    currentIndex: activePageIndex

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 22
                            spacing: 14

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: "Pipeline workspace"
                                    color: textPrimary
                                    font.family: uiFont
                                    font.pixelSize: 28
                                    font.weight: Font.DemiBold
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: pipelineController.status
                                    color: textMuted
                                    font.family: bodyFont
                                    font.pixelSize: 14
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "Run the pipeline from the left panel. Live output is shown below."
                                color: textMuted
                                font.family: bodyFont
                                font.pixelSize: 14
                                wrapMode: Text.Wrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                InsetSurface {
                                    Layout.fillWidth: true
                                    implicitHeight: 88

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 6

                                        SectionLabel { text: "Status" }

                                        Text {
                                            Layout.fillWidth: true
                                            text: pipelineController.status
                                            color: textPrimary
                                            font.family: uiFont
                                            font.pixelSize: 22
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                InsetSurface {
                                    Layout.preferredWidth: 220
                                    implicitHeight: 88

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 6

                                        SectionLabel { text: "Review groups" }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(pipelineController.reviewGroupModel.count)
                                            color: textPrimary
                                            font.family: uiFont
                                            font.pixelSize: 22
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                InsetSurface {
                                    Layout.preferredWidth: 220
                                    implicitHeight: 88

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 6

                                        SectionLabel { text: "Mode" }

                                        Text {
                                            Layout.fillWidth: true
                                            text: pipelineController.splitMode + " / " + pipelineController.config
                                            color: textPrimary
                                            font.family: uiFont
                                            font.pixelSize: 22
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
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
                                        spacing: 10

                                        Text {
                                            text: "Pipeline log"
                                            color: textPrimary
                                            font.family: uiFont
                                            font.pixelSize: 20
                                            font.weight: Font.DemiBold
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: pipelineController.pipelineRunning ? "Live output" : "Latest run"
                                            color: textFaint
                                            font.family: bodyFont
                                            font.pixelSize: 13
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: 14
                                        color: panelInset
                                        border.color: edge
                                        border.width: 1

                                        ScrollView {
                                            id: pipelineLogScroll
                                            anchors.fill: parent
                                            anchors.margins: 1
                                            clip: true
                                            ScrollBar.vertical: RailScrollBar {}
                                            ScrollBar.horizontal: RailScrollBar {}

                                            TextArea {
                                                id: pipelineLogOutput
                                                width: Math.max(pipelineLogScroll.availableWidth, implicitWidth)
                                                color: textPrimary
                                                font.family: monoFont
                                                font.pixelSize: 13
                                                readOnly: true
                                                selectByMouse: true
                                                persistentSelection: true
                                                padding: 14
                                                wrapMode: TextEdit.NoWrap
                                                placeholderText: "Pipeline output will appear here."
                                                placeholderTextColor: textFaint
                                                text: pipelineController.logText
                                                background: Rectangle {
                                                    color: panelInset
                                                }

                                                onTextChanged: {
                                                    cursorPosition = length
                                                    if (parent && parent.contentItem) {
                                                        parent.contentItem.contentY = Math.max(0, parent.contentItem.contentHeight - parent.contentItem.height)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
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

                    SplitView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 320
                        orientation: Qt.Horizontal
                        handle: SplitHandle {}

                        InsetSurface {
                            SplitView.fillWidth: true
                            SplitView.fillHeight: true

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
                            SplitView.preferredWidth: 300
                            SplitView.minimumWidth: 260
                            SplitView.fillHeight: true

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

                            Text {
                                Layout.fillWidth: true
                                text: selectedMemberRows.length > 0
                                    ? selectedMemberRows.length + " selected. Merge them if they are the same image, split them if they are not, then choose the representative kept for the merged group."
                                    : "Click member cards below to select them. Use Same group, Split selected, and Pick representative so the whole review can be done from the UI."
                                color: textMuted
                                font.family: bodyFont
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                MonoButton {
                                    text: "Same group"
                                    enabled: pipelineController.hasCurrentGroup && selectedMemberRows.length >= 2
                                    onClicked: {
                                        pipelineController.reviewMemberModel.mergeRows(selectedMemberRows)
                                        window.clearMemberSelection()
                                    }
                                }

                                MonoButton {
                                    text: "Split selected"
                                    enabled: pipelineController.hasCurrentGroup && selectedMemberRows.length >= 1
                                    onClicked: {
                                        pipelineController.reviewMemberModel.splitRows(selectedMemberRows)
                                        window.clearMemberSelection()
                                    }
                                }

                                MonoButton {
                                    text: "Pick representative"
                                    enabled: pipelineController.hasCurrentGroup && selectedMemberRows.length === 1
                                    onClicked: pipelineController.reviewMemberModel.chooseRepresentative(selectedMemberRows[0])
                                }

                                MonoButton {
                                    text: "Clear selection"
                                    enabled: selectedMemberRows.length > 0
                                    onClicked: window.clearMemberSelection()
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
                                    required property string representativeSourceFileName
                                    required property url reviewImageUrl
                                    required property int occurrenceCount
                                    required property int clusterIndex
                                    required property bool representative
                                    required property bool mergedBySimilarity
                                    required property bool mergedByReview
                                    property bool selected: window.isMemberSelected(index)

                                    width: ListView.view.width - 8
                                    height: 166
                                    radius: 14
                                    color: selected ? "#171c22" : panelInset
                                    border.color: selected ? "#f4f4f4" : (representative ? edgeStrong : edge)
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
                                            border.color: selected ? edgeStrong : edge
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

                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: window.toggleMemberSelection(index)
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
                                                text: representativeSourceFileName.length > 0 ? representativeSourceFileName : logicalId
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
                                                    if (mergedBySimilarity)
                                                        flags.push("Similarity merge")
                                                    if (mergedByReview)
                                                        flags.push("Review merge")
                                                    if (selected)
                                                        flags.push("Selected")
                                                    return flags.length > 0 ? flags.join(" | ") : "No flags"
                                                }
                                                color: textFaint
                                                font.family: bodyFont
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.preferredWidth: 132
                                            Layout.alignment: Qt.AlignTop
                                            spacing: 6

                                            SectionLabel { text: "Group " + clusterIndex }

                                            MonoCheckBox {
                                                checked: selected
                                                text: "Selected"
                                                onClicked: window.toggleMemberSelection(index)
                                            }

                                            MenuTrigger {
                                                text: representative ? "Representative" : "Actions"
                                                menu: memberActionsMenu

                                                DarkMenu {
                                                    id: memberActionsMenu
                                                    Action {
                                                        text: representative ? "Clear representative" : "Make representative"
                                                        onTriggered: {
                                                            window.selectedMemberRows = [index]
                                                            pickSelectedRepresentative()
                                                        }
                                                    }
                                                    Action {
                                                        text: "Split out"
                                                        onTriggered: {
                                                            window.selectedMemberRows = [index]
                                                            splitSelectedMembers()
                                                        }
                                                    }
                                                }
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
    }
}
}
}
