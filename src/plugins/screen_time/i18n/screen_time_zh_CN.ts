<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en">
<!-- PR2: per-plugin catalog for screen_time. Mixed source convention:
     English sources (ScreenTimeSettingsPage + format strings) follow Qt
     convention with zh translation below. Pre-i18n Chinese literals
     (ScreenTimeTab UI / ExportClearDialog) are identity in zh, en.ts
     carries English renderings. The C++ ScreenTimePlugin context
     (tabInfo / pageInfo / tray label) is hand-maintained. -->
<context>
    <name>ScreenTimePlugin</name>
    <message>
        <source>Screen Time</source>
        <translation>屏幕时间</translation>
    </message>
    <message>
        <source>Today's Focus: %1h %2m</source>
        <translation>今日专注: %1小时 %2分</translation>
    </message>
</context>
<context>
    <name>ExportClearDialog</name>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="30"/>
        <source>当前已记录 %1 条 session</source>
        <translation>当前已记录 %1 条 session</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="62"/>
        <source>数据导出 / 清除</source>
        <translation>数据导出 / 清除</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="70"/>
        <source>导出会把每条 session 的窗口标题解密成明文 — 数据属于你,但请注意保存路径的安全。</source>
        <translation>导出会把每条 session 的窗口标题解密成明文 — 数据属于你,但请注意保存路径的安全。</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="92"/>
        <location filename="../qml/ExportClearDialog.qml" line="131"/>
        <source>导出 JSON</source>
        <translation>导出 JSON</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="100"/>
        <location filename="../qml/ExportClearDialog.qml" line="144"/>
        <source>导出 CSV</source>
        <translation>导出 CSV</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="113"/>
        <source>清除全部数据</source>
        <translation>清除全部数据</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="122"/>
        <source>关闭</source>
        <translation>关闭</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="134"/>
        <source>JSON 文件 (*.json)</source>
        <translation>JSON 文件 (*.json)</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="134"/>
        <location filename="../qml/ExportClearDialog.qml" line="147"/>
        <source>所有文件 (*)</source>
        <translation>所有文件 (*)</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="137"/>
        <source>JSON 已导出</source>
        <translation>JSON 已导出</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="147"/>
        <source>CSV 文件 (*.csv)</source>
        <translation>CSV 文件 (*.csv)</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="150"/>
        <source>CSV 已导出</source>
        <translation>CSV 已导出</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="161"/>
        <source>确认清除?</source>
        <translation>确认清除?</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="165"/>
        <source>已清除</source>
        <translation>已清除</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="167"/>
        <source>清除失败 — 见日志</source>
        <translation>清除失败 — 见日志</translation>
    </message>
    <message>
        <location filename="../qml/ExportClearDialog.qml" line="173"/>
        <source>将永久删除全部 session 与 pickup 记录,无法恢复。</source>
        <translation>将永久删除全部 session 与 pickup 记录,无法恢复。</translation>
    </message>
</context>
<context>
    <name>ScreenTimeSettingsPage</name>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="28"/>
        <source>Screen Time</source>
        <translation>屏幕时间</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="35"/>
        <source>Idle detection and data export.</source>
        <translation>闲置检测与数据导出。</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="42"/>
        <source>Idle</source>
        <translation>闲置</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="52"/>
        <source>Idle threshold (seconds)</source>
        <translation>闲置阈值(秒)</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="66"/>
        <source>%1 s</source>
        <translation>%1 秒</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="75"/>
        <source>Below this idle time, you&apos;re considered still active. Longer = fewer pickup events; shorter = more accurate focus tracking.</source>
        <translation>低于此闲置时间视为仍在使用。设长 → 误判拿起事件少;设短 → 专注记录更精确。</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="84"/>
        <source>Data</source>
        <translation>数据</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="94"/>
        <source>Export / Clear Data...</source>
        <translation>导出 / 清除数据...</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="100"/>
        <source>Export decrypts the window titles of every session into the file you pick. Data is yours — mind where you save it.</source>
        <translation>导出会把每条 session 的窗口标题解密成明文。数据属于你 — 请注意保存路径安全。</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="109"/>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeSettingsPage.qml" line="115"/>
        <source>Category overrides: 9 built-in rules cover common apps. Per-app JSON overrides land in v1.1.</source>
        <translation>分类覆盖:9 条内置规则覆盖常见应用。按应用 JSON 覆盖将在 v1.1 上线。</translation>
    </message>
</context>
<context>
    <name>ScreenTimeTab</name>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="52"/>
        <source>%1h %2m</source>
        <translation>%1小时 %2分</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="53"/>
        <source>%1m %2s</source>
        <translation>%1分 %2秒</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="54"/>
        <source>%1s</source>
        <translation>%1秒</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="61"/>
        <source>%1-%2</source>
        <translation>%1-%2</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="100"/>
        <source>应用时长</source>
        <translation>应用时长</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="112"/>
        <source>今日</source>
        <translation>今日</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="113"/>
        <source>本周</source>
        <translation>本周</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="114"/>
        <source>本月</source>
        <translation>本月</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="130"/>
        <source>刷新</source>
        <translation>刷新</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="137"/>
        <source>数据…</source>
        <translation>数据…</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="144"/>
        <source>设置</source>
        <translation>设置</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="170"/>
        <source>💤 闲置中</source>
        <translation>💤 闲置中</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="172"/>
        <source>前台: %1</source>
        <translation>前台: %1</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="173"/>
        <source>等待第一个窗口切换…</source>
        <translation>等待第一个窗口切换…</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="180"/>
        <source>已持续 %1</source>
        <translation>已持续 %1</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="192"/>
        <source>闲置恢复次数: %1</source>
        <translation>闲置恢复次数: %1</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="226"/>
        <source>总计</source>
        <translation>总计</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="252"/>
        <source>每日总时长(近 7 天)</source>
        <translation>每日总时长(近 7 天)</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="252"/>
        <source>每日总时长(近 30 天)</source>
        <translation>每日总时长(近 30 天)</translation>
    </message>
    <message>
        <location filename="../qml/ScreenTimeTab.qml" line="281"/>
        <source>还没有数据 — 切换几个窗口让 Margin 记录一些活动</source>
        <translation>还没有数据 — 切换几个窗口让 Margin 记录一些活动</translation>
    </message>
</context>
</TS>
