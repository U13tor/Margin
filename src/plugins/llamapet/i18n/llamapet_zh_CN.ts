<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en">
<!-- C++ LlamaPetPlugin context is hand-maintained: lupdate does not scan QCoreApplication::translate -->
<context>
    <name>LlamaPetPlugin</name>
    <message>
        <source>LlamaPet</source>
        <translation>LlamaPet 显卡桌宠</translation>
    </message>
    <message>
        <source>Always on Top</source>
        <translation>窗口始终置顶</translation>
    </message>
    <message>
        <source>Click-through</source>
        <translation>鼠标穿透</translation>
    </message>
    <message>
        <source>Auto-dock Hide</source>
        <translation>贴边自动隐藏</translation>
    </message>
    <message>
        <source>MiniPet (96x96)</source>
        <translation>MiniPet 悬浮宠 (96x96)</translation>
    </message>
    <message>
        <source>Dock (260x50)</source>
        <translation>Dock 状态条 (260x50)</translation>
    </message>
    <message>
        <source>Copy Restart Command</source>
        <translation>复制重启命令</translation>
    </message>
    <message>
        <source>Clear Idle KV Slots</source>
        <translation>清空空闲 KV 槽位</translation>
    </message>
</context>
<context>
    <name>HudTab</name>
    <message>
        <location filename="../ui/HudTab.qml" line="70"/>
        <source>❖ LlamaPet</source>
        <translation>❖ LlamaPet 显卡桌宠</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="88"/>
        <source>Online</source>
        <translation>在线</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="88"/>
        <source>Offline</source>
        <translation>离线</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="117"/>
        <source>NVIDIA GPU initializing...</source>
        <translation>NVIDIA GPU 初始化中...</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="124"/>
        <source>Utilization: %1%</source>
        <translation>利用率: %1%</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="134"/>
        <source>VRAM: %1 GB / %2 GB (%3%)</source>
        <translation>显存: %1 GB / %2 GB (%3%)</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="143"/>
        <source>Temp: %1°C · Power: %2W / %3W</source>
        <translation>温度: %1°C · 功耗: %2W / %3W</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="187"/>
        <source>Realtime Trend (60 samples)</source>
        <translation>实时走势 (60 点采样)</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="195"/>
        <source>VRAM %1%</source>
        <translation>显存 %1%</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="201"/>
        <source>TPS %1</source>
        <translation>TPS %1</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="301"/>
        <source>%1 tok/s · %2% KV Hit</source>
        <translation>%1 tok/s · %2% 缓存命中</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="315"/>
        <source>SLOT</source>
        <translation>槽位</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="316"/>
        <source>STATUS</source>
        <translation>状态</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="317"/>
        <source>CONTEXT</source>
        <translation>上下文</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="318"/>
        <source>KV HIT</source>
        <translation>KV 命中</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="319"/>
        <source>OUTPUT</source>
        <translation>已生成</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="356"/>
        <source>Active</source>
        <translation>生成中</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="356"/>
        <source>Idle</source>
        <translation>就绪</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="398"/>
        <source>(Server started without --slots; running in basic mode)</source>
        <translation>（服务未带 --slots 参数启动，已降级运行）</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="18"/>
        <location filename="../ui/HudTab.qml" line="31"/>
        <source>System standby — ready for inference</source>
        <translation>系统待机就绪</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="22"/>
        <source>Model sleeping... zzz</source>
        <translation>呼.. 模型浅睡中</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="23"/>
        <source>Ready for inference!</source>
        <translation>随时准备推理！</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="24"/>
        <source>Processing prompt...</source>
        <translation>正在吞入代码...</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="25"/>
        <source>Generating tokens (%1 tok/s)</source>
        <translation>全力生成中 (%1 tok/s)</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="26"/>
        <source>✨ KV cache hit!</source>
        <translation>✨ KV 缓存复用达成！</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="27"/>
        <source>🚨 VRAM near capacity!</source>
        <translation>🚨 显存快撑爆啦！要掉速！</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="28"/>
        <source>Local service offline</source>
        <translation>未检测到本地服务</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="439"/>
        <source>Copy Restart Command</source>
        <translation>复制重启命令</translation>
    </message>
    <message>
        <location filename="../ui/HudTab.qml" line="450"/>
        <source>Clear Idle KV Cache</source>
        <translation>清空空闲 KV</translation>
    </message>
</context>
<context>
    <name>SettingsPage</name>
    <message>
        <location filename="../ui/SettingsPage.qml" line="31"/>
        <source>LlamaPet Settings</source>
        <translation>LlamaPet 设置</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="38"/>
        <source>Configure local LLM inference backend, GPU telemetry, and desktop pet behaviors.</source>
        <translation>配置本地大模型推理端点、GPU 遥测与桌宠交互行为。</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="56"/>
        <source>Inference Engine</source>
        <translation>推理服务</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="71"/>
        <source>OpenAI Compatible</source>
        <translation>OpenAI 兼容</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="94"/>
        <source>Endpoint URL</source>
        <translation>端点地址</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="100"/>
        <source>Loopback only (127.0.0.1 / localhost)</source>
        <translation>仅限本地回环 (127.0.0.1 / localhost)</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="140"/>
        <source>API Key</source>
        <translation>API 密钥</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="146"/>
        <source>Encrypted via OS Keyring</source>
        <translation>宿主透明加密存储</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="184"/>
        <source>Poll Interval</source>
        <translation>轮询间隔</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="205"/>
        <source>%1 ms</source>
        <translation>%1 毫秒</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="228"/>
        <source>VRAM Alert Thresholds</source>
        <translation>显存警戒阈值</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="240"/>
        <source>Warning</source>
        <translation>警告阈值</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="264"/>
        <location filename="../ui/SettingsPage.qml" line="304"/>
        <source>%1%</source>
        <translation>%1%</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="280"/>
        <source>Critical</source>
        <translation>危急阈值</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="328"/>
        <source>Desktop Pet Window</source>
        <translation>桌宠悬浮窗</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="341"/>
        <source>Always on Top</source>
        <translation>窗口始终置顶</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="346"/>
        <source>Keep the floating pet window above all other windows</source>
        <translation>桌宠悬浮窗始终保持在其他窗口之上</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="370"/>
        <source>Click-through</source>
        <translation>鼠标穿透</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="375"/>
        <source>Pass mouse events through the window to applications beneath</source>
        <translation>穿透鼠标交互，直接操作桌宠下方的程序</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="399"/>
        <source>Auto-hide on Edge (Peek)</source>
        <translation>贴边自动隐藏 (Peek)</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="404"/>
        <source>Slide pet into the screen edge after 3s of mouse inactivity</source>
        <translation>贴紧屏幕边缘并在无操作 3 秒后自动半隐藏</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="428"/>
        <source>Global Shortcut</source>
        <translation>全局快捷键</translation>
    </message>
    <message>
        <location filename="../ui/SettingsPage.qml" line="433"/>
        <source>Toggle pet window visibility from anywhere</source>
        <translation>任何界面下均可一键显隐桌宠悬浮窗</translation>
    </message>
</context>
</TS>
