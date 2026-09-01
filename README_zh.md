# Rubato

> **When AI thinks, you move.** — AI 在思考，你在生活。

[English](README.md) | 中文说明

![Rubato](assets/product.jpg)

![Rubato 实拍演示](assets/product.gif)

Rubato 是一枚可以捧在手心的复古麦金塔小屏幕——基于 ESP8266 的桌面伴侣（240×240 彩屏）。它监测你的 AI 编程会话，把 AI 的等待时间转化为你的健康休息。

## 工作方式

1. PC 端小插件通过 MQTT（TLS）发布 AI 会话状态：`thinking` → `generating` → `done`
2. 呼吸光团随状态变化——Thinking 奶油慢呼吸 / Generating 冰川蓝快呼吸 / done 转绿收尾
3. 长任务（预估 ≥ 30 秒）触发整屏健康提醒，全彩图标 + 两行文案

## 每天六项微休息

喝水 · 如厕 · 护眼 · 肩颈 · 提肛 · 站立办公

每项活动有每日配额，两次整屏提醒全局间隔 ≥ 30 分钟，节奏温和不轰炸。产品承诺是**提醒到位**，不做完成率打卡。

## 不止提醒

- **桌面时钟**：NTP 校时 + 欧美格式日期 + 天气（Open-Meteo 自动定位）
- **状态光团**：每台独立 MQTT 身份；只镜像会话状态，消息内容永不上屏
- **OTA 自升级**：分段 HTTPS 下载 + 防刷死守护——坏更新进 Safe Mode 远程自愈，无需 USB
- **Web 设置**：亮度 / 方向 / 温度单位浏览器直改；所有设置断电不丢

## 插件

每个编码智能体各一个插件，共用同一套 MQTT 契约——从 [Rubato_Plugins](https://github.com/lovaxi/Rubato_Plugins) 安装。

| 智能体 | 状态 |
|---|---|
| DeepSeek Harness | 可用 |
| OpenClaw | 可用 |
| Cursor | 可用 |
| OpenCode | 可用 |
| Codex | 计划中 |
| Claude Code | 计划中 |

## 硬件与源码

ESP8266（NodeMCU）+ 240×240 TFT，USB Type-C 供电。Arduino 框架（ESP8266 core 3.1.2 / TFT_eSPI）。固件在 `rubato.ino`，图标管线在 `tools/`。烧录、内置热点配网、一条串口指令完成设备发证——即插即用。

## 许可

GPL-3.0——原始时钟作者 Misaka（2021），后由 Rubato 项目重设计。
硬件设计方案来自 [SmallDesktopDisplay](https://github.com/chuxin520922/SmallDesktopDisplay)（作者 chuxin520922）。

完整开发史见 [changelog.md](changelog.md)。
