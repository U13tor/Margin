// Theme — design-token singleton. Mirrors docs/06-ui-design.md §2 (dark theme).
// Full token set: the fixed 12-colour palette, the 7-step type scale, the
// 8-multiple spacing scale, radii, motion (durations + easing), and the
// sans/mono font stacks (faces registered in M4-C2).
pragma Singleton
import QtQuick

QtObject {
    // §2.1 background layer
    readonly property color bgBase: "#0E0E10"
    readonly property color bgElevated: "#161618"
    readonly property color bgHover: "#1F1F23"

    // §2.1 foreground layer
    readonly property color fgPrimary: "#E4E4E7"
    readonly property color fgSecondary: "#A1A1AA"
    readonly property color fgMuted: "#71717A"

    // §2.1 accent layer
    readonly property color accentBrand: "#7C3AED"
    readonly property color accentSuccess: "#10B981"
    readonly property color accentWarning: "#F59E0B"
    readonly property color accentDanger: "#EF4444"

    // §2.1 border layer
    readonly property color borderSubtle: "#27272A"
    readonly property color borderStrong: "#3F3F46"

    // §2.2 type scale (px)
    readonly property int textXs: 12
    readonly property int textSm: 13
    readonly property int textBase: 14
    readonly property int textLg: 16
    readonly property int textXl: 20
    readonly property int text2xl: 24
    readonly property int text3xl: 32

    // §2.2 font stacks — primary family passed to `font.family`. Inter / JetBrains
    // Mono faces are bundled + registered in M4-C2. Qt's QML font value type
    // (QQuickFontValueType) only exposes `family` (singular) in 6.5, not the
    // QFont `families` list, so we ship one primary name and rely on Qt's
    // glyph-level font merging for CJK characters not covered by Inter.
    readonly property string fontSans: "Inter"
    readonly property string fontMono: "JetBrains Mono"

    // §2.3 spacing — strict 8-multiple system. space1 (4px) is the only sub-8
    // value, reserved for icon insets.
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space6: 24
    readonly property int space8: 32
    readonly property int space12: 48

    // §2.4 radius
    readonly property int radiusSm: 4
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12
    readonly property int radiusFull: 9999

    // §2.5 motion — durations (ms) + easing as bezierCurve-ready control points
    // ([c1x,c1y,c2x,c2y,1,1], assignable straight to `easing.bezierCurve`).
    readonly property int durationFast: 100
    readonly property int durationNormal: 200
    readonly property int durationSlow: 300
    readonly property var easeOut: [0.16, 1, 0.3, 1, 1, 1]
    readonly property var easeInOut: [0.4, 0, 0.2, 1, 1, 1]

    // §6.4 chart category palette — cycles the fixed accent set so charts
    // introduce no new hex (the 12-colour rule still holds).
    readonly property var categoryPalette: [
        accentBrand, accentSuccess, accentWarning, accentDanger, fgSecondary, fgMuted
    ]
}
