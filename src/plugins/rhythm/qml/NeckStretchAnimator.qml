// NeckStretchAnimator — 8-frame stretch-pose view (M4-C13b).
//
// Pure view: renders one of 8 PNG frames
// (qrc:/rhythm/icons/neck-stretch-{1..8}.png) based on currentStep.
// Progression is owned by BreakOverlay, which computes currentStep from
// rhythm.remainingSeconds so one full 8-pose round fills the entire
// break (e.g. 5min/8 ≈ 37.5s per pose).
//
// stepNames maps the 8 classic cervical stretches (颈椎八式). Numbering +
// names are aligned with docs/06-ui-design.md §4.7.2 — change there first
// if renaming here.

import QtQuick

Item {
    id: root

    // Total number of poses in one round. Fixed at 8 (matches asset set).
    property int totalSteps: 8

    // Current step, 1-based. Exposed for BreakOverlay bindings + unit tests.
    // Driven externally — set this from the parent to advance the animation.
    property int currentStep: 1

    readonly property var stepNames: [
        qsTr("双掌擦颈"),
        qsTr("左顾右盼"),
        qsTr("前后点头"),
        qsTr("青龙摆尾"),
        qsTr("旋肩舒颈"),
        qsTr("头手相抗"),
        qsTr("颈项争力"),
        qsTr("仰头望掌")
    ]

    // Per-step instruction text. Canonical source: docs/06-ui-design.md
    // §4.7.2 column "界面文字指示文案" — keep verbatim. Surfaced in
    // BreakOverlay under the "动作 N/8 · name" line.
    readonly property var stepDescriptions: [
        qsTr("从上向下连续捏按颈部，每捏按 3 下为一遍，连续 8 遍。左右手交替。"),
        qsTr("头向左转至极限位持续 3 秒，再向右转至极限位持续 3 秒。连续 8 遍。"),
        qsTr("头向前伸至极限位持续 3 秒，再向后仰至极限位持续 3 秒。连续 8 遍。"),
        qsTr("头向左侧屈至极限位持续 3 秒，再向右侧屈至极限位持续 3 秒。连续 8 遍。"),
        qsTr("双手置于肩前，掌心朝下。两臂由后向前旋转，再由前向后旋转。连续 8 遍。"),
        qsTr("两手十指交叉置于颈后，手向前用力，头向后用力，持续 3 秒。连续 8 遍。"),
        qsTr("左手置于背后，右手置于胸前平推，头向右旋转至极限持续 3 秒。左右交替。"),
        qsTr("两手十指交叉上举过头，掌心朝上，头向后仰，持续 3 秒。连续 8 遍。")
    ]

    readonly property string currentName: root.stepNames[root.currentStep - 1]
    readonly property string currentDescription: root.stepDescriptions[root.currentStep - 1]
    readonly property string currentImage: "qrc:/rhythm/icons/neck-stretch-%1.png".arg(root.currentStep)

    Image {
        anchors.fill: parent
        source: root.currentImage
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(width, height)
        // Smooth crossfade between frames would need a ShaderEffect; for MVP
        // a straight swap matches the pose fidelity requirement (8 distinct
        // frames at ~37s cadence reads as "held position", not "animation").
        asynchronous: true
    }
}
