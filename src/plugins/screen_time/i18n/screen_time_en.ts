<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en" sourcelanguage="en">
<!-- PR2: per-plugin catalog for screen_time. English sources
     (ScreenTimeSettingsPage + format strings) are identity; pre-i18n
     Chinese literals (ScreenTimeTab UI / ExportClearDialog) carry English
     renderings so en mode flips them. The C++ ScreenTimePlugin context
     (tabInfo / pageInfo / tray label) is hand-maintained. -->
<context>
    <name>ScreenTimePlugin</name>
    <message>
        <source>Screen Time</source>
        <translation>Screen Time</translation>
    </message>
    <message>
        <source>Today's Focus: %1h %2m</source>
        <translation>Today's Focus: %1h %2m</translation>
    </message>
</context>
<context>
    <name>ExportClearDialog</name>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="30"/>
        <source>当前已记录 %1 条 session</source>
        <translation>%1 sessions recorded</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="62"/>
        <source>数据导出 / 清除</source>
        <translation>Data export / clear</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="70"/>
        <source>导出会把每条 session 的窗口标题解密成明文 — 数据属于你,但请注意保存路径的安全。</source>
        <translation>Export decrypts the window titles of every session into the file you pick. Data is yours — mind where you save it.</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="92"/>
        <location filename="../qml/ExportClearDialog.qml" line="131"/>
        <source>导出 JSON</source>
        <translation>Export JSON</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="100"/>
        <location filename="../qml/ExportClearDialog.qml" line="144"/>
        <source>导出 CSV</source>
        <translation>Export CSV</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="113"/>
        <source>清除全部数据</source>
        <translation>Clear all data</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="122"/>
        <source>关闭</source>
        <translation>Close</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="134"/>
        <source>JSON 文件 (*.json)</source>
        <translation>JSON files (*.json)</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="134"/>
        <location filename="../qml/ExportClearDialog.qml" line="147"/>
        <source>所有文件 (*)</source>
        <translation>All files (*)</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="137"/>
        <source>JSON 已导出</source>
        <translation>JSON exported</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="147"/>
        <source>CSV 文件 (*.csv)</source>
        <translation>CSV files (*.csv)</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="150"/>
        <source>CSV 已导出</source>
        <translation>CSV exported</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="161"/>
        <source>确认清除?</source>
        <translation>Confirm clear?</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="165"/>
        <source>已清除</source>
        <translation>Cleared</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="167"/>
        <source>清除失败 — 见日志</source>
        <translation>Clear failed — see logs</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="173"/>
        <source>将永久删除全部 session 与 pickup 记录,无法恢复。</source>
        <translation>Permanently deletes all sessions and pickup records. Cannot be undone.</translation>
    </message>
</context>
<context>
    <name>ScreenTimeSettingsPage</name>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="28"/>
        <source>Screen Time</source>
        <translation>Screen Time</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="35"/>
        <source>Idle detection and data export.</source>
        <translation>Idle detection and data export.</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="42"/>
        <source>Idle</source>
        <translation>Idle</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="52"/>
        <source>Idle threshold (seconds)</source>
        <translation>Idle threshold (seconds)</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="66"/>
        <source>%1 s</source>
        <translation>%1 s</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="75"/>
        <source>Below this idle time, you&apos;re considered still active. Longer = fewer pickup events; shorter = more accurate focus tracking.</source>
        <translation>Below this idle time, you&apos;re considered still active. Longer = fewer pickup events; shorter = more accurate focus tracking.</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="84"/>
        <source>Data</source>
        <translation>Data</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="94"/>
        <source>Export / Clear Data...</source>
        <translation>Export / Clear Data...</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="100"/>
        <source>Export decrypts the window titles of every session into the file you pick. Data is yours — mind where you save it.</source>
        <translation>Export decrypts the window titles of every session into the file you pick. Data is yours — mind where you save it.</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="109"/>
        <source>Advanced</source>
        <translation>Advanced</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="115"/>
        <source>Category overrides: 9 built-in rules cover common apps. Per-app JSON overrides land in v1.1.</source>
        <translation>Category overrides: 9 built-in rules cover common apps. Per-app JSON overrides land in v1.1.</translation>
    </message>
</context>
<context>
    <name>ScreenTimeTab</name>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="52"/>
        <source>%1h %2m</source>
        <translation>%1h %2m</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="53"/>
        <source>%1m %2s</source>
        <translation>%1m %2s</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="54"/>
        <source>%1s</source>
        <translation>%1s</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="61"/>
        <source>%1-%2</source>
        <translation>%1-%2</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="100"/>
        <source>应用时长</source>
        <translation>App Time</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="112"/>
        <source>今日</source>
        <translation>Today</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="113"/>
        <source>本周</source>
        <translation>This week</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="114"/>
        <source>本月</source>
        <translation>This month</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="130"/>
        <source>刷新</source>
        <translation>Refresh</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="137"/>
        <source>数据…</source>
        <translation>Data…</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="144"/>
        <source>设置</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="170"/>
        <source>💤 闲置中</source>
        <translation>💤 Idle</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="172"/>
        <source>前台: %1</source>
        <translation>Foreground: %1</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="173"/>
        <source>等待第一个窗口切换…</source>
        <translation>Waiting for the first window switch…</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="180"/>
        <source>已持续 %1</source>
        <translation>Active for %1</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="192"/>
        <source>闲置恢复次数: %1</source>
        <translation>Idle recoveries: %1</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="226"/>
        <source>总计</source>
        <translation>Total</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="252"/>
        <source>每日总时长(近 7 天)</source>
        <translation>Daily total (last 7 days)</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="252"/>
        <source>每日总时长(近 30 天)</source>
        <translation>Daily total (last 30 days)</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="281"/>
        <source>还没有数据 — 切换几个窗口让 Margin 记录一些活动</source>
        <translation>No data yet — switch a few windows so Margin can record some activity</translation>
    </message>
</context>
</TS>
