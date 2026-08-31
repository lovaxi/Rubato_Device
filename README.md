# Rubato 小型桌面显示器

[English](README_EN.md) | 简体中文

> **When AI thinks, you move.** — AI 在思考，你在生活。

基于 ESP8266 的智能桌面伴侣（240×240 TFT）：监测 AI Coding 过程中的 Thinking / Generating / Done 状态，把 AI 的等待时间转化为用户的健康时间——喝水、如厕、护眼、肩颈放松、盆底肌锻炼、站立办公。界面为英文，面向欧美用户。

当前版本：**V1.1.1**（发布编号，对齐源码 `#define Version`）

---

## 一、产品定位

Rubato 不是 AI 状态监控器，也不是传统健康提醒 App，而是：

> **AI Workload → Free Time → Best Human Action**
> AI 工作负载 → 可利用的空闲时间 → 最合适的人类行为

AI 替用户工作的时候，Rubato 帮用户照顾自己。产品的诚实承诺是**提醒到位**——不做完成率打卡（设备无法验证用户是否照做，无反馈闭环的数据是伪数据）。

---

## 二、功能总览

- **桌面时钟（默认态）**：NTP 校时（东八区，pool.ntp.org / ntp.aliyun.com / time.cloudflare.com 三源轮换容灾），时:分 + 闪烁冒号 + 欧美格式日期 `Wed, Aug 14`；天气图标/温度/湿度/城市名（Open-Meteo，IP 自动定位，10 分钟刷新）。度数符号为**合成式渲染**（数字 + 手绘圆点 + 单位字母）——所载字体均为纯 ASCII，无 °/℃/℉ 字位（查证见 changelog V1.4.9）
- **AI 状态呼吸灯**：连接 EMQX Cloud（托管 MQTT，TLS），每台设备独立身份（deviceId + token），收到 thinking/generating 后先渐入渐出显示模型名字幕（约 1.8s），随后光团以状态色呼吸——Thinking 奶油慢呼吸 / Generating 冰川蓝快呼吸，600ms 渐变换挡；done 后转绿三段式收尾（转绿 0.5s → 满亮定格 1.45s → 熄灭 0.65s），日期 600ms 渐现接场。纯文本消息不上屏
- **健康提醒页（V1.2，V1.0.4 扩至 6 项）**：长任务触发时**整屏接管**，显示全彩图标（100×100）+ 两行全粗体英文提醒文案，done 后整屏还原
- **开机画面（V1.4.9 重排）**：Rubato 词标（上中部粗体）+ 进度条 + `Connecting to WiFi......` + 底部铭牌行（deviceId 居中灰 / 版本号右下角）；连接成功或失败后由状态屏接管词标区，词标自动让位
- **OTA 防刷死守护（V1.4.9）**：RTC 引导计数器——OTA 成功布防 → 新固件健康运行 60s 自动撤防 → 连续 3 次布防启动未撤防进入 **Safe Mode**（只跑时钟 + NTP + OTA 监听，跳过高风险子系统），修正包可经 OTA 远程推送自愈，无需 USB
- **Web 配网（WiFiManager）**：连接失败或首次使用自动开启热点 `Rubato`，可配置 WiFi 与参数（英文界面）
- **Web 管理服务器（常开）**：浏览器访问设备 IP 即可改亮度、屏幕方向、温度单位（摄氏/华氏，华氏含换算显示），无需重新编译
- **数据持久化**：亮度 / 屏幕方向 / 温度单位 / 城市 / WiFi / 今日提醒计数均存 EEPROM，断电不丢失
- **串口调试**：115200，见"使用方法"

---

## 三、设备行为规格

### UI 设计原则

240×240 小屏，不做 Dashboard：

> **One screen, one message.**

大字体、大图标、极少文字；简单动画、自动切换；信息一眼理解。

### 视觉语法（V1.1 定稿，所有页面复用）

| 元素 | 规范 |
|------|------|
| 背景 | 纯黑 `(0,0,0)`，任何页面不得使用其他底色 |
| 状态色 | Thinking=奶油 `(255,216,168)` / Generating=冰川蓝 `(150,195,240)` / 完成定格=鲜绿 `(88,228,128)` |
| 墨阶 | 中性灰三级（城市/日期/温湿度）+ 暖白正文；冷色仅 Generating，绿色仅 done |
| 动效语言 | 一切过渡用 `lerpColor` 渐变（约 600ms）；呼吸节奏 = 非对称双段余弦（吸气 40%/呼气 60%） |

### AI 工作期：双页模型（V1.2 定稿）

消息带（屏幕下方 240×60 区域）在两个页面间切换：

**页面一：呼吸光团（默认）**——字幕播完后光团以状态色呼吸，无常驻文字；**短任务（未触发阈值）始终停留在此页**。

**页面二：健康提醒（长任务升级）**——触发后**整屏接管**（时钟/天气/城市/日期全部淡出，这是"消息带只管消息"原则的唯一例外）：

```text
     [icon 100x100]
   Do  TEN  Slow Kegels
```

- 全彩图标（Fluent Emoji flat 美术 + 自绘扁平眼，100×100）居中偏上，常亮不闪烁
- 文案为**两行短句**（`FreeSansBold12pt7b` 全粗体，数量/时长词金色高亮，~78% 亮度防刺眼），静态海报无逐帧动画——安静即优雅
- 淡出/淡入用**背光 PWM 压暗/回升**实现（避开 240×240 大画布的堆风险）：暗场中完成内容切换
- 收尾：光团页 done → 绿盘三段式 → 日期渐现；提醒页 done → 整屏淡出 → 暗场还原旧页面 → 背光回升淡入

### 触发与规则（全部满足才提醒）

```text
estSec > 45 秒（含于 thinking/generating 消息中）
且 处于工作时段 9:00–18:30（可调）
且 距上次提醒 ≥ 40 分钟
且 该活动今日配额未用完（喝水 4 / 上厕所 2 / 眼睛 2 / 肩颈 2 / 提肛 2 / 站立 1）
→ 从配额未满的活动中随机抽取一项
```

- 间隔依据：总配额 13 次（第 6 项站立办公加入后，配额为用户手调 4/2/2/2/2/1）/ 9.5h，全局最小间隔 40 分钟仍是节奏主约束（9.5h 理论上限 ~14 次，配额 13 恰在其下）；单项节奏由随机抽取 + 配额自然形成
- `state=Estimate` 只记账不驱动状态机（预估先于任务状态到达，之后没有 done，防悬挂呼吸）
- 闩锁：**每次任务最多提醒一次**；进入提醒页后本次任务不回退光团页
- estSec 为预测会失准：跨任务残留已在任务开始/done 时清零；按实际等待时长兜底触发为后续可选增强

---

## 四、硬件与引脚

| 外设 | 引脚 |
| ---- | ---- |
| 屏幕 SCK | GPIO14 |
| 屏幕 MOSI | GPIO13 |
| 屏幕 RES | GPIO2 |
| 屏幕 DC | GPIO0 |
| 屏幕背光 LCDBL | GPIO5 |

TFT 驱动配置位于 TFT_eSPI 库的 `User_Setup.h`。

> **背光极性**：本面板背光**低电平有效**（占空比越高越暗，由 sketch 的 `blWrite()` 统一换算驱动）。库侧 `User_Setup.h` **不得定义 `TFT_BACKLIGHT_ON`**——定义后 `tft.begin()` 会把 D1 拉高 = 熄屏（3.x 内核 PWM 量程为 0-255，sketch 内部保持 0-1023 惯例）。

---

## 五、使用方法

1. 烧录固件后上电，自动连接已保存的 WiFi；失败或首次使用会开启配置热点 `Rubato`
2. 连接热点完成配网；成功后设备 IP 见串口日志，浏览器访问 `http://<设备IP>` 进入设置页
3. 后续通过 Web 页面或串口指令修改参数

### 串口指令（115200）

| 指令 | 说明 |
| ---- | ---- |
| `0x01` | 亮度设置（0-100，输入值） |
| `0x02` | 城市名称设置（英文，如 Changsha） |
| `0x03` | 屏幕方向设置（0-3） |
| `0x04` | 重置 WiFi 并重启 |
| `0x05` | 随机演练一次健康提醒页（照常计数） |
| `0x05 1~6` | 定向演练指定活动（纯显示测试，不计数）：1 喝水 / 2 厕所 / 3 眼睛 / 4 肩颈 / 5 提肛 / 6 站立 |
| `0x06 <id> <token>` | 逐台发证：写入 deviceId + token 并重启（发货前执行） |
| `0x07` | 擦除设备身份并重启（退货/返修重发） |
| `done` | 等价 MQTT done，结束演练并整屏还原 |

### EEPROM 地址分配（1KB 扇区）

| 地址 | 内容 |
| ---- | ---- |
| 1 | 背光亮度（1-100） |
| 2 | 屏幕方向（0-3） |
| 3 | 温度单位（0=摄氏默认 / 1=华氏） |
| 10-29 | 城市名（首字节长度 + 字符） |
| 30-125 | WiFi ssid/psw 结构体 |
| 140-141 | 健康计数日期键（month×100+day） |
| 142-147 | 六项健康活动今日计数（`HEALTH_ACTS`） |
| 148-149 | 备用（健康计数扩展位） |
| 150-194 | 设备身份 deviceId + token（`0x06` 写入 / `0x07` 擦除） |

新计数位扩容后首次读取到 0xFF 会触发一次性全量计数重置（>9 判废），属预期安全行为。

### 设备发证（发货前，每台 2 分钟）

1. 刷同一份固件（全批次一个 .bin），上电后开机画面显示本机 MAC 派生的 deviceId（如 `TT-A1B2C3`）
2. 生成 token（`crypto.randomBytes(16).toString('hex')`），串口 `0x06 <deviceId> <token>` 写入
3. EMQX Cloud 仪表盘 Authentication 加同名账号（username=deviceId / password=token）
4. 发消息验证链路（`node dsh-mqtt-live-test.cjs`，或 `node send-to-device.cjs thinking` 后接 `done`）→ 登记 ledger.csv（deviceId, token, 日期）→ 发货

deviceId 硬件锚定不可选、不可枚举；未发证设备：MQTT 不连接，时钟/天气照常，开机画面显示建议 ID。

### MQTT 消息协议

Broker：**EMQX Cloud Serverless**（免费档 ≈ 23 台 7×24 在线）。每台设备一个身份，**设备与插件双端共用同一对凭证**：

- **deviceId**：MAC 派生（如 `TT-A1B2C3`），即 MQTT username
- **token**：随机密文，即 MQTT password，随设备卡片交付买家（填入插件配置）

话题：`rubato/<deviceId>/state`——设备只订阅自己的话题；插件用同一对凭证向该话题发布。配置文件两份、内容保持一致（工作区根 `dsh-mqtt-config.json` 与 `Rubato_Plugin_DSH/dsh-mqtt-config.json`，插件先找前者），**逐条记录热加载**，改完即生效无需重启。

```json
{"model": "glm-5.3-flash", "state": "thinking", "ts": 1788000000000}
{"state": "generating"}
{"state": "Estimate", "estSec": 57.5}
{"state": "done"}
```

`state` 含 `thinking`/`generating` 驱动呼吸灯；`done` 收尾；`Estimate` 只记账（阈值判定用）；纯文本忽略。设备端解析**大小写无关**（内部统一转小写再匹配），插件发的 `Thinking`/`Done` 直接受理。

配置文件**必须显式 `"tls": true`**——插件内置默认值 `tls: false` 是阿里云 1883 明文时代的遗产，缺省该键会明文连 EMQX 的 8883 被服务器直接掐断（症状：`connection closed by server`）。文件保存**禁止带 BOM**（Node `JSON.parse` 不容 BOM）。

**clientId 命名约定**：`rubato-<来源工具>[-p<进程号>]`——`rubato-dsh-p3a7`（DSH 插件，进程后缀由插件自动附加防撞）、将来依次 `rubato-codex` / `rubato-claude` / `rubato-cursor`。EMQX Cloud Serverless 共享前端**不接受同 clientId 会话接管**（重复即 CONNACK 2 拒绝），因此每个来源、每个进程实例的 clientId 必须唯一；同一设备话题允许多个发布方同时在线，新工具接入时复用同一 host/凭证/话题、换自己的 clientId 即可。

### 正式接入（DSH 真实会话自动驱动）

插件 `rubato` 注册于 `~/.dsh/profiles/web/cordis.patch.yml`，包体在 `~/.dsh/profiles/node_modules/rubato/`（host-only，随 dsh 启动加载）。host 进程内挂 `llm/stream` 钩子：每次模型调用自动发布 Estimate（kNN 零 token 时长预估）→ Thinking → Generating → Done（含 token 用量回填校准样本），fire-and-forget 不阻塞对话；多步工具调用的中间步骤只发 Thinking，最后一步才 Done。

**低延迟设计**：插件与 broker 保持**一条常驻 MQTT 连接**（PINGREQ 保活，断线后下一条发布自动重连），每条消息只是一个报文——首条发布含建连 ~200ms，后续 ~35ms；Thinking 在流开始即发布（与 DSH 界面同步，不等首个 reasoning token，避免大上下文预填充期间设备落后）；流异常时补发 Done 防设备悬挂在呼吸态。并发发布在连接层串行化（单领导者建连，后来者等待复用），杜绝同 clientId 竞态。

手动验证用 `Rubato_Plugin_DSH/send-to-device.cjs`；改动插件代码后需重启 DSH 生效（config 除外，热加载）。

---

## 六、工具脚本

健康页图标管线（`tools/logos/`）：

1. 取源：六枚全彩 SVG（fluent-emoji-flat 水滴 / 厕所 / 冥想 / 桃子 / 站立人像 + 自绘扁平眼）存于 `tools/icons/color/`（V1.4.9 起直接取用全彩 emoji 美术；早期单色描边方案已弃，`_fetch/_color` 两个脚本为历史工具，仅新活动需要主题色着色时使用）
2. `_gen_health_icons.cjs`：渲染 100×100 黑底 JPEG 并生成 `img/health_icons.h`（PROGMEM 数组），`PICKS` 数组决定启用哪六张（`ICON_SIZE` 与固件 `drawJpg` 调用联动）

运行前需在 `tools/logos/` 下执行 `npm install`（依赖 sharp）。图标来源：Microsoft Fluent Emoji flat（MIT），免费商用，风格与黑底页面适配度最佳。

---

## 七、范围裁决

| 议题 | 裁决 | 理由 |
|------|------|------|
| estSec 上屏 | ❌ 不做 | 预测不准；上屏的只有事实（已进行时长） |
| Token/Cost 完成卡 | ❌ 已移除 | 与健康主线无关 |
| Memory / 完成率 | ⏸️ 暂缓 | 无状态返回通道，伪数据不如不记；提醒到位即可 |
| 健康医疗宣称 | ❌ 永不 | 只做行为提醒，规避监管与伪科学 |
| 每帧大块内存分配 | ❌ 已消灭 | 堆碎片化教训：8 位色画布 + 直绘兜底，日期不可缺席 |
| 固件体积预算 | **≤ ~1019KB/版本** | 设备 4MB flash、方案 4M2M：双 OTA 槽各 ~1020KB，新版本 bin 超限即无法 OTA（编译输出 66% 指单槽占用，非 4MB 总量）；文件系统 2MB 代码未使用，OTA 只擦写目标槽，EEPROM 配置不受影响 |
| ℃/℉ 字位 | ❌ 不引字库 | 所载字体（编号字体 + FreeFonts）均为纯 ASCII（读库源码查证），度数符号走合成式绘制（数字 + 手绘圆点 + 字母）；整片非 ASCII 文案出现时再评估 FS 挂 vlw 平滑字体 |
| 混合字重文案 | ❌ 已回退 | 健康页数量/时间粗体 + 其余常规的混排观感"怪"，全粗体统一（V1.4.9 实验结论） |

---

## 八、路线图

**已完成**：V1.0 欧美化改造（2026-08-15/16）→ V1.1 AI 呼吸灯 + 配色体系（2026-08-25）→ V1.2 健康提醒页整屏接管（2026-08-25）→ V1.3 MQTT 多设备改造 + DSH 正式接入（2026-08-30）→ V1.4 OTA 固件自升级 + 内核 3.1.2 迁移（2026-08-30，OSS HTTPS 全链路真机验证）→ V1.4.9 NTP 三源容灾 + OTA 防刷死守护 + 启动页 v2 + 第 5 项健康活动提肛 + 温度单位切换（2026-08-30）→ 发布编号切换（V1.0.2 起）+ OTA 冷启动下载端到端闭环 → MQTT 证书固定（链 + 主机名 + 双时钟修复）+ 第 6 项健康活动站立办公（2026-08-31）→ 产品更名 **Rubato**（2026-08-31）

**后续可选**：

- [x] OTA（V1.4 已落地，V1.4.9 增加防刷死守护）：MQTT 通知（`{"state":"ota","ver":...,"url":...,"md5":...}`）→ `cmpVersion` 版本门禁 → **分段 HTTPS 下载**（每 256KB 一条连接，HTTP Range + 断点续传）→ `setMD5` 校验 → 重启切槽；bin ≤ ~1019KB（见范围裁决）。完整性模型：md5 经 TLS 加密 MQTT 下发；下载通道 https（OSS/GitHub Releases）或 http（局域网）。产物命名 `ota-releases\Rubato-<ver>.bin`；构建固定 `eesz=4M2M`（详见 changelog V1.4）。防刷死：RTC 引导计数器 + Safe Mode（连续 3 次布防启动未撤防 → 最小服务 + 仅 OTA 监听，修正包远程自愈）。
- [ ] AI 会话历史已在 PC 端 `rubato-records.jsonl` 自然累积，Memory 个性化暂缓、数据就绪
- [ ] Codex / Claude Code / OpenClaw 适配
- [ ] 24小时切换等默认设置

---

## 更新日志

每次功能更新记录见 [changelog.md](changelog.md)。
