<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en">
<!-- PR2: per-plugin catalog for rhythm. Mixed source convention:
     - English sources (RhythmSettingsPage / RhythmTab headings) follow Qt
       project convention — zh translation filled in below.
     - Chinese sources (BreakOverlay / NeckStretchAnimator / RhythmToast)
       are pre-i18n literals that became qsTr() sources verbatim; zh
       translation is identity, en.ts carries the English rendering.
     The C++ RhythmPlugin context below is hand-maintained — lupdate does
     not scan QCoreApplication::translate, so regen-ts.sh will drop it
     whenever you re-run lupdate. Restore by hand after each regen. -->
<context>
    <name>RhythmPlugin</name>
    <message>
        <source>Rhythm</source>
        <translation>节律</translation>
    </message>
    <message>
        <source>Rhythm: ON</source>
        <translation>节律: 开</translation>
    </message>
    <message>
        <source>Rhythm: OFF</source>
        <translation>节律: 关</translation>
    </message>
    <message>
        <source>Pomodoro: %1 break</source>
        <translation>番茄钟: %1 休息</translation>
    </message>
</context>
<context>
    <name>BreakOverlay</name>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="102"/>
        <source>颈椎放松操</source>
        <translation>颈椎放松操</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="120"/>
        <source>动作 %1/8 · %2</source>
        <translation>动作 %1/8 · %2</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="167"/>
        <source>跳过</source>
        <translation>跳过</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="174"/>
        <source>推迟 5 分钟</source>
        <translation>推迟 5 分钟</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="198"/>
        <source>🎉</source>
        <translation>🎉</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="204"/>
        <source>本节休息已完成</source>
        <translation>本节休息已完成</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="215"/>
        <source>颈椎八式一轮完成。下一节工作番茄 %1 分钟后开始。</source>
        <translation>颈椎八式一轮完成。下一节工作番茄 %1 分钟后开始。</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="223"/>
        <source>%1 秒后自动关闭</source>
        <translation>%1 秒后自动关闭</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="233"/>
        <source>立即关闭</source>
        <translation>立即关闭</translation>
    </message>
</context>
<context>
    <name>NeckStretchAnimator</name>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="26"/>
        <source>双掌擦颈</source>
        <translation>双掌擦颈</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="27"/>
        <source>左顾右盼</source>
        <translation>左顾右盼</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="28"/>
        <source>前后点头</source>
        <translation>前后点头</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="29"/>
        <source>青龙摆尾</source>
        <translation>青龙摆尾</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="30"/>
        <source>旋肩舒颈</source>
        <translation>旋肩舒颈</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="31"/>
        <source>头手相抗</source>
        <translation>头手相抗</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="32"/>
        <source>颈项争力</source>
        <translation>颈项争力</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="33"/>
        <source>仰头望掌</source>
        <translation>仰头望掌</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="40"/>
        <source>从上向下连续捏按颈部，每捏按 3 下为一遍，连续 8 遍。左右手交替。</source>
        <translation>从上向下连续捏按颈部，每捏按 3 下为一遍，连续 8 遍。左右手交替。</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="41"/>
        <source>头向左转至极限位持续 3 秒，再向右转至极限位持续 3 秒。连续 8 遍。</source>
        <translation>头向左转至极限位持续 3 秒，再向右转至极限位持续 3 秒。连续 8 遍。</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="42"/>
        <source>头向前伸至极限位持续 3 秒，再向后仰至极限位持续 3 秒。连续 8 遍。</source>
        <translation>头向前伸至极限位持续 3 秒，再向后仰至极限位持续 3 秒。连续 8 遍。</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="43"/>
        <source>头向左侧屈至极限位持续 3 秒，再向右侧屈至极限位持续 3 秒。连续 8 遍。</source>
        <translation>头向左侧屈至极限位持续 3 秒，再向右侧屈至极限位持续 3 秒。连续 8 遍。</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="44"/>
        <source>双手置于肩前，掌心朝下。两臂由后向前旋转，再由前向后旋转。连续 8 遍。</source>
        <translation>双手置于肩前，掌心朝下。两臂由后向前旋转，再由前向后旋转。连续 8 遍。</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="45"/>
        <source>两手十指交叉置于颈后，手向前用力，头向后用力，持续 3 秒。连续 8 遍。</source>
        <translation>两手十指交叉置于颈后，手向前用力，头向后用力，持续 3 秒。连续 8 遍。</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="46"/>
        <source>左手置于背后，右手置于胸前平推，头向右旋转至极限持续 3 秒。左右交替。</source>
        <translation>左手置于背后，右手置于胸前平推，头向右旋转至极限持续 3 秒。左右交替。</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="47"/>
        <source>两手十指交叉上举过头，掌心朝上，头向后仰，持续 3 秒。连续 8 遍。</source>
        <translation>两手十指交叉上举过头，掌心朝上，头向后仰，持续 3 秒。连续 8 遍。</translation>
    </message>
</context>
<context>
    <name>RhythmSettingsPage</name>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="27"/>
        <source>Rhythm</source>
        <translation>节律</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="34"/>
        <source>Pomodoro work / break cycle.</source>
        <translation>番茄工作 / 休息循环。</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="44"/>
        <source>Work minutes</source>
        <translation>工作分钟数</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="58"/>
        <location filename="../qml/RhythmSettingsPage.qml" line="85"/>
        <source>%1 min</source>
        <translation>%1 分钟</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="71"/>
        <source>Break minutes</source>
        <translation>休息分钟数</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="98"/>
        <source>Max postpones per break</source>
        <translation>每次休息最大推迟次数</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="112"/>
        <location filename="../qml/RhythmSettingsPage.qml" line="139"/>
        <source>%1</source>
        <translation>%1</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="125"/>
        <source>Target rounds per day</source>
        <translation>每日目标番茄数</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="155"/>
        <source>开发者</source>
        <translation>开发者</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="168"/>
        <source>Start</source>
        <translation>开始</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="176"/>
        <source>Stop</source>
        <translation>停止</translation>
    </message>
</context>
<context>
    <name>RhythmTab</name>
    <message>
        <location filename="../qml/RhythmTab.qml" line="49"/>
        <source>Rhythm &amp; Health</source>
        <translation>节律与健康</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="56"/>
        <source>Work / break cycle</source>
        <translation>工作 / 休息循环</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="117"/>
        <source>下次休息</source>
        <translation>下次休息</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="121"/>
        <source>Work %1 min · Break %2 min</source>
        <translation>工作 %1 分钟 · 休息 %2 分钟</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="135"/>
        <source>继续</source>
        <translation>继续</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="135"/>
        <source>暂停</source>
        <translation>暂停</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="144"/>
        <source>跳过</source>
        <translation>跳过</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="158"/>
        <source>设置</source>
        <translation>设置</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="187"/>
        <source>今日番茄 / 目标</source>
        <translation>今日番茄 / 目标</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="204"/>
        <source>推迟剩余</source>
        <translation>推迟剩余</translation>
    </message>
</context>
<context>
    <name>RhythmToast</name>
    <message>
        <location filename="../qml/RhythmToast.qml" line="47"/>
        <source>该起身活动一下了</source>
        <translation>该起身活动一下了</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="54"/>
        <source>已连续工作 %1 分钟</source>
        <translation>已连续工作 %1 分钟</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="61"/>
        <source>还可推迟 %1 次</source>
        <translation>还可推迟 %1 次</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="62"/>
        <source>本次不能再推迟</source>
        <translation>本次不能再推迟</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="76"/>
        <source>推迟 5 分钟</source>
        <translation>推迟 5 分钟</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="86"/>
        <source>开始做操</source>
        <translation>开始做操</translation>
    </message>
</context>
</TS>
