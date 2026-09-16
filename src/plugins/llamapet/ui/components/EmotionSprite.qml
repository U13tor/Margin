import QtQuick

Item {
    id: root

    property int emotion: 0 // 0: Idle, 1: Sleeping, 2: EatingPrefill, 3: SpittingTokens, 4: HappyCache, 5: ConfusedOffline, 6: PanickingOom
    property bool interactiveHeart: false
    property var skinData: null
    property int currentFrame: 0
    property string animState: "idle"

    implicitWidth: 96
    implicitHeight: 96

    Component.onCompleted: {
        loadSkin();
    }

    function loadSkin() {
        if (typeof floatingWindows !== "undefined" && floatingWindows && floatingWindows.skinJson) {
            try {
                skinData = JSON.parse(floatingWindows.skinJson);
                updateAnimation();
                return;
            } catch (e) {
                console.warn("LlamaPet failed to parse floatingWindows.skinJson:", e);
            }
        }
        try {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "qrc:/llamapet/assets/skins/pixel_llama/skin.json", false);
            xhr.send(null);
            if (xhr.status === 200 || xhr.responseText.length > 0) {
                skinData = JSON.parse(xhr.responseText);
                updateAnimation();
            }
        } catch (e) {
            console.warn("LlamaPet failed to load skin.json via XHR:", e);
        }
    }

    onEmotionChanged: updateAnimation()
    onInteractiveHeartChanged: updateAnimation()

    function triggerHeart() {
        interactiveHeart = true;
        heartResetTimer.restart();
    }

    Timer {
        id: heartResetTimer
        interval: 1500 // 动效纪律：互动动画不超过 1.5s
        repeat: false
        onTriggered: {
            root.interactiveHeart = false;
        }
    }

    function updateAnimation() {
        if (interactiveHeart) {
            animState = "heart";
        } else {
            switch (emotion) {
            case 0: animState = "idle"; break;
            case 1: animState = "sleeping"; break;
            case 2: animState = "eating_prefill"; break;
            case 3: animState = "spitting_tokens"; break;
            case 4: animState = "happy_cache"; break;
            case 5: animState = "confused_offline"; break;
            case 6: animState = "panicking_oom"; break;
            default: animState = "idle"; break;
            }
        }

        currentFrame = 0;
        if (skinData && skinData.animations && skinData.animations[animState]) {
            var anim = skinData.animations[animState];
            var fps = anim.fps || 1;
            // 动效纪律：帧率上限 30fps，静息状态 1fps
            fps = Math.min(Math.max(fps, 1), 30);
            frameTimer.interval = Math.round(1000 / fps);
            if (Window.window && Window.window.visible) {
                frameTimer.restart();
            }
        }
        canvas.requestPaint();
    }

    Timer {
        id: frameTimer
        repeat: true
        running: Window.window ? Window.window.visible : true // 动效纪律：窗口隐藏即停帧
        onTriggered: {
            if (!root.skinData || !root.skinData.animations || !root.skinData.animations[root.animState]) return;
            var frames = root.skinData.animations[root.animState].frames;
            if (frames && frames.length > 1) {
                root.currentFrame = (root.currentFrame + 1) % frames.length;
                canvas.requestPaint();
            }
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            if (!root.skinData) return;

            var palette = root.skinData.palette || {};
            var anim = root.skinData.animations ? root.skinData.animations[root.animState] : null;
            if (!anim || !anim.frames || anim.frames.length === 0) return;

            var frameIndex = Math.min(root.currentFrame, anim.frames.length - 1);
            var grid = anim.frames[frameIndex];
            if (!grid || grid.length === 0) return;

            var rows = grid.length;
            var cols = grid[0].length;
            var pixelW = width / cols;
            var pixelH = height / rows;

            for (var r = 0; r < rows; ++r) {
                for (var c = 0; c < cols; ++c) {
                    var colorIdx = grid[r][c];
                    if (colorIdx !== 0) {
                        var colorStr = palette[colorIdx];
                        if (colorStr && colorStr !== "transparent") {
                            ctx.fillStyle = colorStr;
                            ctx.fillRect(Math.floor(c * pixelW), Math.floor(r * pixelH),
                                         Math.ceil(pixelW), Math.ceil(pixelH));
                        }
                    }
                }
            }
        }
    }
}
