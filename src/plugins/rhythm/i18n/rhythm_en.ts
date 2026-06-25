<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en" sourcelanguage="en">
<!-- PR2: per-plugin catalog for rhythm. English sources get identity
     translation; pre-i18n Chinese sources (BreakOverlay / NeckStretchAnimator /
     RhythmToast) carry English renderings so en mode flips them.
     The C++ RhythmPlugin context below is hand-maintained — lupdate does
     not scan QCoreApplication::translate, so regen-ts.sh will drop it
     whenever you re-run lupdate. Restore by hand after each regen. -->
<context>
    <name>RhythmPlugin</name>
    <message>
        <source>Rhythm</source>
        <translation>Rhythm</translation>
    </message>
    <message>
        <source>Rhythm: ON</source>
        <translation>Rhythm: ON</translation>
    </message>
    <message>
        <source>Rhythm: OFF</source>
        <translation>Rhythm: OFF</translation>
    </message>
    <message>
        <source>Pomodoro: %1 break</source>
        <translation>Pomodoro: %1 break</translation>
    </message>
</context>
<context>
    <name>BreakOverlay</name>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="102"/>
        <source>颈椎放松操</source>
        <translation>Neck relaxation routine</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="120"/>
        <source>动作 %1/8 · %2</source>
        <translation>Step %1/8 · %2</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="167"/>
        <source>跳过</source>
        <translation>Skip</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="174"/>
        <source>推迟 5 分钟</source>
        <translation>Postpone 5 min</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="198"/>
        <source>🎉</source>
        <translation>🎉</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="204"/>
        <source>本节休息已完成</source>
        <translation>Break complete</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="215"/>
        <source>颈椎八式一轮完成。下一节工作番茄 %1 分钟后开始。</source>
        <translation>One round of the 8-step neck routine done. Next work pomodoro starts in %1 min.</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="223"/>
        <source>%1 秒后自动关闭</source>
        <translation>Auto-close in %1 s</translation>
    </message>
    <message>
        <location filename="../qml/BreakOverlay.qml" line="233"/>
        <source>立即关闭</source>
        <translation>Close now</translation>
    </message>
</context>
<context>
    <name>NeckStretchAnimator</name>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="26"/>
        <source>双掌擦颈</source>
        <translation>Palms rubbing neck</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="27"/>
        <source>左顾右盼</source>
        <translation>Look left and right</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="28"/>
        <source>前后点头</source>
        <translation>Nod forward and back</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="29"/>
        <source>青龙摆尾</source>
        <translation>Dragon wagging tail</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="30"/>
        <source>旋肩舒颈</source>
        <translation>Shoulder rolls</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="31"/>
        <source>头手相抗</source>
        <translation>Head vs. hands press</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="32"/>
        <source>颈项争力</source>
        <translation>Neck isometric hold</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="33"/>
        <source>仰头望掌</source>
        <translation>Look up at palms</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="40"/>
        <source>从上向下连续捏按颈部，每捏按 3 下为一遍，连续 8 遍。左右手交替。</source>
        <translation>Pinch-and-press down the neck from top to bottom; every 3 presses is one round, do 8 rounds. Alternate left and right hands.</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="41"/>
        <source>头向左转至极限位持续 3 秒，再向右转至极限位持续 3 秒。连续 8 遍。</source>
        <translation>Turn head left to the limit, hold 3 s; then right to the limit, hold 3 s. Repeat 8 times.</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="42"/>
        <source>头向前伸至极限位持续 3 秒，再向后仰至极限位持续 3 秒。连续 8 遍。</source>
        <translation>Stretch head forward to the limit, hold 3 s; then tilt back to the limit, hold 3 s. Repeat 8 times.</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="43"/>
        <source>头向左侧屈至极限位持续 3 秒，再向右侧屈至极限位持续 3 秒。连续 8 遍。</source>
        <translation>Tilt head left to the limit, hold 3 s; then right to the limit, hold 3 s. Repeat 8 times.</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="44"/>
        <source>双手置于肩前，掌心朝下。两臂由后向前旋转，再由前向后旋转。连续 8 遍。</source>
        <translation>Hands in front of shoulders, palms down; rotate arms back-to-front, then front-to-back. Repeat 8 times.</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="45"/>
        <source>两手十指交叉置于颈后，手向前用力，头向后用力，持续 3 秒。连续 8 遍。</source>
        <translation>Interlock fingers behind the neck; hands press forward while head presses back, hold 3 s. Repeat 8 times.</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="46"/>
        <source>左手置于背后，右手置于胸前平推，头向右旋转至极限持续 3 秒。左右交替。</source>
        <translation>Left hand behind back, right palm pushes across the chest; rotate head right to the limit, hold 3 s. Alternate sides.</translation>
    </message>
    <message>
        <location filename="../qml/NeckStretchAnimator.qml" line="47"/>
        <source>两手十指交叉上举过头，掌心朝上，头向后仰，持续 3 秒。连续 8 遍。</source>
        <translation>Interlock fingers, raise hands above head palms up; tilt head back, hold 3 s. Repeat 8 times.</translation>
    </message>
</context>
<context>
    <name>RhythmSettingsPage</name>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="27"/>
        <source>Rhythm</source>
        <translation>Rhythm</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="34"/>
        <source>Pomodoro work / break cycle.</source>
        <translation>Pomodoro work / break cycle.</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="44"/>
        <source>Work minutes</source>
        <translation>Work minutes</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="58"/>
        <location filename="../qml/RhythmSettingsPage.qml" line="85"/>
        <source>%1 min</source>
        <translation>%1 min</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="71"/>
        <source>Break minutes</source>
        <translation>Break minutes</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="98"/>
        <source>Max postpones per break</source>
        <translation>Max postpones per break</translation>
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
        <translation>Target rounds per day</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="155"/>
        <source>开发者</source>
        <translation>Developer</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="168"/>
        <source>Start</source>
        <translation>Start</translation>
    </message>
    <message>
        <location filename="../qml/RhythmSettingsPage.qml" line="176"/>
        <source>Stop</source>
        <translation>Stop</translation>
    </message>
</context>
<context>
    <name>RhythmTab</name>
    <message>
        <location filename="../qml/RhythmTab.qml" line="49"/>
        <source>Rhythm &amp; Health</source>
        <translation>Rhythm &amp; Health</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="56"/>
        <source>Work / break cycle</source>
        <translation>Work / break cycle</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="117"/>
        <source>下次休息</source>
        <translation>Next break</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="121"/>
        <source>Work %1 min · Break %2 min</source>
        <translation>Work %1 min · Break %2 min</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="135"/>
        <source>继续</source>
        <translation>Continue</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="135"/>
        <source>暂停</source>
        <translation>Pause</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="144"/>
        <source>跳过</source>
        <translation>Skip</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="158"/>
        <source>设置</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="187"/>
        <source>今日番茄 / 目标</source>
        <translation>Today&apos;s pomodoros / target</translation>
    </message>
    <message>
        <location filename="../qml/RhythmTab.qml" line="204"/>
        <source>推迟剩余</source>
        <translation>Postpones left</translation>
    </message>
</context>
<context>
    <name>RhythmToast</name>
    <message>
        <location filename="../qml/RhythmToast.qml" line="47"/>
        <source>该起身活动一下了</source>
        <translation>Time to move</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="54"/>
        <source>已连续工作 %1 分钟</source>
        <translation>Working for %1 min straight</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="61"/>
        <source>还可推迟 %1 次</source>
        <translation>%1 postpone(s) left</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="62"/>
        <source>本次不能再推迟</source>
        <translation>No more postpones this round</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="76"/>
        <source>推迟 5 分钟</source>
        <translation>Postpone 5 min</translation>
    </message>
    <message>
        <location filename="../qml/RhythmToast.qml" line="86"/>
        <source>开始做操</source>
        <translation>Start the routine</translation>
    </message>
</context>
</TS>
