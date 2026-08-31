# Rubato 更新日志 (Changelog)

> 本文件记录每次功能更新内容。README.md 仅保留项目正常说明，**由用户维护，AI 修改前必须重读原文、只做小步定点编辑，禁止整篇重写**（2026-08-30 整篇重写引发编码事故的教训）。

---

## 2026-08-31 · V1.0.6 二次更名 rubato（fadai → rubato）

- **更名动机**：用户终审拍板 **Rubato**——意大利乐术语 *tempo rubato*（"被偷走的时间"）：AI 占用时间，产品把时间偷回来还给用户，与产品承诺（AI Workload → Free Time）叙事严丝合缝；面向欧美人群好读好记
- **商标终检（本次改名依据）**：US 第 9 类无活标障碍——1996 年唯一 9 类 RUBATO（Reg. 2048079）已 **Inactive**；深圳 Obuy/Aoyang 活标（Reg. 7165832）为 **Class 8 手工具**；Rubato Media（Reg. 6046786）为 Class 35 广告；NICE TIME WINES（2025 申请）为 33 类酒。**遗留未验证**：英国 UK00003777501 与加拿大 1992160 两件 LIVE 标的类别（官方页 JS 挡抓取）——上 Kickstarter 前建议做一次专业 clearance 检索
- **固件落点**（同 V1.0.5 的 8 处，fadai → rubato）：开机词标 ×2、话题前缀 `rubato/<id>/state`、配网热点名、mDNS（`rubato.local`）、Web 设置页 `<h1>`、文件头注释（顺带 ASCII 化，移除非 ASCII 的"发呆"字样）；`#define Version` V1.1.0 → **V1.1.1**
- **刻意保留**（延续 V1.0.5 决策）：deviceId 前缀 `TT-xxxxxx`（EEPROM 已持久化 + EMQX 凭证库已注册）、数据文件名 `thinktime-records.jsonl` / `thinktime-stats.json`（校准数据连续性）、`Thinktime_DshPlugin` 目录名、GitHub 仓库名
- **同步清单**：✅ 插件源码仓 `Thinktime_Plugin_DSH`（dsh + opencode 双套 lib 的 topic/品牌字样 + package.json name：fadai → rubato、fadai-opencode → rubato-opencode，导出符号 Fadai → Rubato，注释 FD- → TT- 对齐固件）；⬜ 部署副本 `~/.dsh/profiles/node_modules/thinktime/`（lib 两文件覆盖 + cordis.patch.yml 校对 + 重启 DSH——**须与设备重刷同一维护窗口执行，否则 AI→设备链路中断**）；⬜ 其余 5 插件仓（OpenCode / Codex / ClaudeCode / Cursor / OpenClaw 独立仓）按同规则平移；⬜ GitHub 仓库名 Rename（旧名自动 301）；⬜ OTA 产物命名 `Rubato-<ver>.bin`
- 历史条目中的 Thinktime / fadai 字样保持原貌（历史事实）
- **补充（同日晚，GitHub 文档全量同步）**：用户要求 README 中 thinktime 全量调整为 rubato——已改 4 处：插件目录引用 `Thinktime_DshPlugin/` → `Rubato_Plugin_DSH/`（×2）、部署路径 `node_modules/thinktime/` → `node_modules/rubato/`、数据文件名 → `rubato-records.jsonl`；changelog 标题同步为 Rubato。**此四项覆盖上一行"刻意保留"中的对应项**（插件目录名、数据文件名），本地目录/数据文件/部署副本的物理重命名随维护窗口跟进；`TT-` 前缀保留决策不变

## 2026-08-31 · V1.0.5 产品更名 fadai（避开 Thinktime 海外商标冲突）

- **更名动机**：Thinktime 在海外已有在先商标使用，Kickstarter 前换名成本最低的窗口就是现在；`fadai`（发呆拼音）独特性强、好读好记，且与产品"陪你发呆"的气质天然契合
- **固件内全部落点**（8 处）：开机词标 ×2（tft 直绘 + 240×120 精灵）、MQTT 话题前缀 `thinktime/<id>/state` → **`fadai/<id>/state`**、配网热点名、mDNS 主机名（`fadai.local`）、Web 设置页 `<h1>`、文件头注释
- **刻意保留**：deviceId 前缀 `TT-xxxxxx` 不变——身份区 EEPROM 已持久化 + EMQX Cloud 凭证库已注册，换前缀 = 全量设备重新发证，成本 > 收益；历史条目中的 Thinktime 字样保持原貌（历史事实）
- **改名的同步清单（固件外，需手动）**：① PC 插件/工具的 MQTT 话题前缀（`Thinktime_DshPlugin` + 工作区根 `dsh-mqtt-config.json`）——**不改则设备收不到消息**；② README 品牌字样（用户自维护）；③ GitHub 仓库名（Settings → Rename，旧名自动 301）；④ 本地目录/草 sketch 名 `Thinktime\` → `fadai\`（Arduino 要求 .ino 基名与目录一致，改时 IDE 需关闭）；⑤ OTA 产物命名 `fadai-<ver>.bin`
- 版本号跳过空档直接对齐 V1.0.5（V1.0.4 为站立办公，未单独推送）

---

## 2026-08-31 · V1.0.4 MQTT 证书固定 + 第 6 项健康活动"站立办公"（真机验证通过）

### MQTT 证书固定（安全清单最后一项，MQTT 端点）

- 嵌入 DigiCert Global Root G2 根 CA（PROGMEM，源文件 `certs/emqxsl-ca.crt`，取自 EMQX Cloud 部署页），`mqttConnect` 以 `setTrustAnchors` 替换 `setInsecure`——**证书链 + 主机名**（`*.ala.cn-shenzhen.emqxsl.cn`）双重验证，冒牌 broker 被拒
- 安全语义：账号密码证明"我是真设备"，证书证明"你是真服务器"——设备不再会把 token 交给冒名者，OTA 门禁"MD5 来自可信通道"的前提从此真正成立
- **OTA/OSS 端点维持 `setInsecure`**（用户决策：只做 EMQX），OSS 证书固定另行待办
- **双时钟隐形坑（真机排障实录）**：首连报 "Certificate is expired or not yet valid"——PC 侧 Node 探针证明服务器链/日期/CA 全对（`tools/_tls_check.cjs` 留作诊断工具），设备 `now()` 也正确——根因：**设备有两个时钟**，自研 NTP 只喂 TimeLib（`setTime`），而 BearSSL 日期校验读 SDK 系统时钟（`time(nullptr)`），后者从未被设置、永远停在 1970 纪元。修复：`getNtpTime()` 成功时 `settimeofday()` 同步喂 SDK 时钟（一处钩子覆盖开机 + 300s 周期同步）。教训：**TimeLib 的 now() ≠ time(nullptr)**

### 第 6 项健康活动：站立办公

- 每日 1 次、30 分钟；文案 `On Your Feet / for THIRTY Mins`（金色高亮数字拼写，与 ONE/TWO/THREE/TEN 同语法）
- 图标 fluent-emoji-flat person-standing（第 6 枚全彩，100×100 管线照旧，`icon_stand_jpg` 2325B）
- EEPROM 计数启用预留位 **147**（148-149 转备用）——**首刷一次性全量计数重置属预期**（0xFF 判废机制，147 从未写过）
- 串口演练扩至 `0x05 1~6`；`0x09` 计数表自动含第 6 行（HEALTH_NAMES：water/toilet/eyes/neck/kegel/stand）
- 配额表（用户手调定稿）：water 4 / toilet 2 / eyes 2 / neck 2 / kegel 2 / stand 1 = **每日 13 次**

### 设置页回显 + 计数查询

- 设置页回显已存值（小石子清账）：亮度输入框 `value=` 实值、方向/单位 radio 的 `checked` 跟随 `LCD_Rotation`/`tempUnits`，不再硬编码默认态
- 健康提醒计数查询：串口 `0x09` 打印当日日期键 + 各活动计数/配额（Web 页方案提出后否决，未实装）

---

## 2026-08-31 · V1.0.3 OTA 下载器韧性硬化（冷启动链路三轮实测收敛）

- **冷启动下载架构落地**：收到 OTA 命令 → URL/MD5 落 EEPROM（200-455 / 460-499，身份区 194 之后无冲突）+ RTC `rsvd=1` 标志 → 立即重启 → 下次开机在一切子系统分配前、堆最干净时刻下载；标志**先消费后下载**（断电/崩溃不重入）；WiFi 未连保留标志自动重试；失败照常开机、串口报错、旧槽完好。设计动机：真机实测长运行堆碎片化（总空闲 20.5KB、最大连续块 10.8KB、**碎片率 43%**）把 seg-0 TLS 握手拖进 HW WDT 硬复位（`rst cause:4`，旧槽完好自愈，**防刷死守护实战通过**）——冷启动后碎片 1%、握手一次到位
- **首轮试跑两修**：① 下载路径补 `WIFI_NONE_SLEEP`（重写 startOtaUpdate 时遗漏——modem 在 yield 间隙打盹，512B TLS 记录下吞吐雪崩）；② 暂存重启前补 `WiFi.disconnect()`（猝死重启的无线关联残留在路由器侧，下次入网撞车滑进配网门户）。**重启次数为设计使然**：顺利 2 次（暂存 + 装完），进过配网 3 次
- **二轮教训**：分段 HTTP 硬错误零重试（TLS 瞬断即整体失败）→ 任何分段失败（HTTP 错误/停滞）都从 Updater 精确断点换新连接续传，重试预算 4 次、段成功清零
- **三轮实测定调瓶颈**：~1.1KB/s = **TLS 记录数 × 跨洋 RTT**（512B/条 × CN→美西 ~450ms/条含 ACK 往返；PC 快是因为 TCP 窗口在途 MB 级，ESP 几 KB 窗口 + 512B 记录注定慢）。改 **MFLN 尺寸阶梯**：开机探针 2048→1024→512 取最大（OSS 实测认 2048 ✓ 4096 拒）；**总死线改"无进展死线"**（每 64KB 进展续命 240s，只杀真死传输）；停滞判死 15s→20s（2048 记录下一个 64KB hop ≈14s，防误杀）；修一处自查揪出的**变量遮蔽**（阶梯结果写进内层副本，seg 客户端永远读外层默认 4096/1024——非 MFLN 服务器 = 握手卡死风险）
- **端到端闭环达成（国内节点实测）**：推送 → 暂存重启 → 干净堆（碎片 1%）下载 → MD5 校验 → 装槽 → 防刷死武装 → 60s 撤防。速度取舍：国内桶"凑合"、美西桶（美国买家生产配置）慢而必达——几 KB TCP 窗口 + 每条记录一次 RTT 是 2 美元芯片的物理天花板，代码侧已到顶，买家视角（RTT 50-80ms）体验为测试视角的 3-5 倍

---

## 2026-08-30 · V1.0.2（= 内部 V1.4.9）NTP 容灾 + 自动时区 + OTA 防刷死守护 + 启动页 v2 + 第 5 项健康活动"提肛保菊"（真机验证通过）

### NTP 三源轮换容灾

- `pool.ntp.org` 单点偶发失败 → 服务器池 `{ pool.ntp.org, ntp.aliyun.com, time.cloudflare.com }`：每次失败自动轮换到下一个（海外 + 国内网络双覆盖），串口 `[NTP] try xxx (n/3)` / `[NTP] synced via xxx` 可观测

### OTA 防刷死守护（Safe Mode）

- 风险界定：OTA 真变砖不可能（bootloader 永不写 + eboot 校验失败自动回退旧槽 + MD5 提交前把关）；真实风险 = **镜像合法但固件启动即崩**的假砖
- 机制：RTC 内存引导计数器（软重启存活、断电清零，offset 32）——OTA 成功布防（pending=1）→ 新固件健康运行 60s 自动撤防 → **连续 3 次布防启动未撤防 = SAFE MODE**：只跑时钟 + NTP + 已验证的 OTA 监听，跳过天气 HTTP/JSON 等高风险子系统，屏幕顶部红字 `SAFE MODE`
- 自愈闭环：安全模式继续接受修正版 OTA（远程恢复，无需 USB）；修复版跑满 60s 自动退出安全模式并补拉天气，无需重启
- 越界防御：`0x05` 类参数、计数器全部改用 `HEALTH_ACTS` 常量（V1.4.9 顺手根治魔法数字）

### 启动页 v2（UI 三轮打磨收敛）

- 布局：**Thinktime** 词标（FreeSansBold18pt7b 白，上中部 y=52）+ 进度条 + `Connecting to WiFi......`（font 2 原字体原文案）+ 底部铭牌行（EMQX deviceId 居中灰 + **版本号右下角**）；240×120 全宽精灵 @ (0,110)
- 互斥规则：连接成功 → 原有 IP 屏（"Open Browser: <ip>"）接管顶部并清空词标条；失败进配网同理——**不新增任何成功提示文案**（尊重原版行为）
- 教训：24pt 粗体太大、font 4 换字体难看、文案不得自创——三轮反馈收敛为"小词标 + 原版文字"
- 配网热点名 `ThinkTime` → **`Thinktime`**（与产品词标大小写统一）

### 第 5 项健康活动：提肛保菊（Kegels）

- 文案 `Do TEN / Slow Kegels`（TEN 金色，每次 10 下），配额 **4 次/天**，总配额 14 → 18；EEPROM 计数器 142-146（deviceId@150 之前 147-149 空置带，旧设备升级后新槽 0xFF 触发一次性计数重置，安全）
- 图标（终版）：**五枚全彩 112×112**——droplet 💧 / toilet 🚽 / **自绘扁平眼**（白巩膜+蓝虹膜+高光，与水滴同族蓝调） / person-in-lotus-position 🧘 / peach 🍑，黑底页上色彩统一饱满。选型链：桃子先入（西方语境"屁股"的全民 emoji 语义，花/菊谐音不跨文化已否决）→ 全集合一切换 fluent-emoji-flat → 眼睛四连否（👀 滑稽/🔭 偏题/👓 非本意/👁️ 不够好）→ 自绘扁平眼定稿；Unicode 无白水杯 emoji，补水符号用水滴表达；图标 88→112px（y=28-140，文案中心不动）
- 健康页文案字重：**仅数量/时间 token 粗体**（ONE/TWO/THREE/TEN 金色 + Mins），其余常规体（FreeSans12pt7b）——逐段切换字体并按各段实际字体测宽，行居中以粗体 fontHeight 为基准保持两行垂直一致
- 图标管线：`tools/logos/_gen_health_icons.cjs` PICKS 5 项；SVG 源经 iconify CDN 取用（`fluent-emoji-flat:*`），sharp 渲染链不变（88×88 黑底 4:4:4 q90）；`npm install sharp` 可随时重建依赖
- 演练命令为 **1 起始**：`0x05 5` = 提肛页（帮助文本已同步 [1-5]）

### 版本编号切换（产品发布编号启用）

- 自本版起启用**产品发布编号 `V1.0.2`**（源码 `#define Version`；此后 changelog 使用发布编号），OTA 产物同步命名 `Thinktime-1.0.2.bin`
- OTA 门禁 `cmpVersion` 兼容性已核验：V 前缀可选、**逐段数值比较**（`1.0.10 > 1.0.9` 正确）、缺段补 0、上限 8 段
- OTA 冷启动下载改造自本版源码起步，三轮实测收敛与端到端闭环详见 V1.0.3/V1.0.4 条目

### 清理

- 删除 `.build\`（64MB，含被 1M 布局污染的 CLI 构建产物）与 `build\`（17MB 陈旧编译树，IDE 占用残留待用户空闲时清）；拒绝方案的图标源文件与候选预览一并清除

### 健康页图标/文案定稿（用户三轮反馈收敛）

- 图标 112 → **100×100**（112 过大），屏上 (70,32)-(170,132)，文案行中心 156/184 → **164/192** 下移留出呼吸空隙
- 文案字重回退：混排（数量/时间粗体 + 其余常规）观感"怪"，**恢复全粗体**（FreeSansBold12pt7b 统一）
- 图标管线 `ICON_SIZE` 常量与固件 `drawJpg` 坐标联动（页宽 240 居中 x=70）

### 温度单位 Celsius / Fahrenheit（合成式度数显示）

- 字体事实：TFT_eSPI 编号字体（ASCII 32-127）与 GFX FreeFonts（0x20-0x7E）**均无 °/℃/℉ 字位**（读库源码查证），度数显示走**合成式**：数字 + 手绘度数圆点（`fillCircle`，x 跟随数字实测宽度，负温/三位数不重叠）+ 单位字母
- Web 设置页新增 **Temperature Units** 单选（Celsius/Fahrenheit）；EEPROM 地址 3（0=C 默认，1=F），开机加载；切换后即时重绘温度行
- 华氏为**显示层换算**（`C*9/5+32` 四舍五入），Open-Meteo 拉取与缓存保持摄氏不重复请求；湿度 % 单位无关

### 时区自动检测（跨时区自愈，海外适配）

- 原缺陷：NTP 纪元 +8 硬编码（`timeZone=8`），海外客户时间错
- 新方案：NTP 仅作 UTC 基准，本地偏移取自 **Open-Meteo 响应根节点 `utc_offset_seconds`**（设备 IP 定位经纬度的当前偏移，服务端含夏令时），**零额外请求、零时区数据库**；偏移变化时 `adjustTime()` 平移运行时钟（不重同步）并持久化 EEPROM 4-7（int32，范围守卫 -12h~+14h，另加 **900s 整倍数检查**——真实时区偏移全是 15 分钟整倍数，可挡 0xFF=-1 等垃圾值）
- **必带 `&timezone=auto`**：Open-Meteo 的 timezone 参数**默认 GMT**（上海坐标也返回 utc_offset_seconds=0，设备忠实显示 UTC 的"08 之谜"由此而来）；带 auto 才按坐标返回当地偏移
- 池耗尽暗坑：`DynamicJsonDocument(1024)` 装不下带 `current_units`/时区字符串的完整响应，v6 池耗尽**静默跳字段不报错**（`deserializeJson` 返回值被忽略）→ 1536 + 错误打印 + `doc.memoryUsage()` 水位日志
- 行为：国内新机默认 +8 即对；跨时区首次天气到达（≈1 分钟）跳当地；此后每次开机立即本地时间；DST 切换随天气刷新（10 分钟）自愈。经纬度每开机定位一次（桌面设备移动即重启，天然覆盖）；保存的城市名仅为显示标签，不影响定位与偏移

---

## 2026-08-30 · V1.4 OTA 固件自升级（OSS HTTPS 全链路真机验证通过）+ ESP8266 内核 3.1.2 迁移

### OTA 机制（V1.4.0–V1.4.8 迭代落地）

- **触发链**：MQTT `{"state":"ota","ver","url","md5"}` → `cmpVersion()` 只接受严格更新 → 静态进度屏（黄绿进度条，每 10% 重绘）→ `mqttClient.disconnect()` + `delete mqttNet`（释放 ~12KB TLS 堆）→ 分段下载 → `Update.begin/setMD5/write/end`（MD5 校验）→ 重启切槽
- **完整性模型（v1）**：md5 经 TLS 加密的 MQTT 通道下发作为载荷门禁；下载通道可为 https（OSS/GitHub Releases）或 http（局域网测试）；TLS 证书固定仍是量产前待办
- **分段下载（核心设计）**：每 256KB 一条新连接（HTTP Range + 206/Content-Range），段间 50ms 排水——单 pcb 的 flash 写停顿暴露窗口缩短 4 倍以上，且天然支持断点续传（短段从 Updater 精确位置重试，上限 4 次）；6×128KB 会在第 6 次连接触发 lwip pcb 池耗尽崩溃（TIME_WAIT 占位），3×256KB 远离该上限
- **MFLN 教训（2.6.3 时代，留档）**：Aliyun OSS 探测谎报 MFLN 支持（probe yes / 实际握手卡死）；Node https 不支持 MFLN → 16384/4096 大缓冲路径堆不够必失败； BearSSL 512/512 小缓冲只在真 MFLN 服务器可用
- **发送侧**：`Thinktime_DshPlugin/send-to-device.cjs ota <ver> <url> <md5>`（md5 小写 32 位 hex，Windows 侧 `certutil -hashfile <file> MD5` 去空格或 `Get-FileHash`）；本地测试服务器 `serve-ota.cjs`（http 8080 / https 8443 自签名，per-request 现读，缺 bin 回 503 不崩）
- **产物命名**：`Thinktime\ota-releases\Thinktime-<ver>.bin`（版本号后缀，防错版）；**bin 必须校验内容版本串 + md5 唯一**（构建缓存曾产生同名同 md5 假产物）

### ESP8266 内核 2.6.3 → 3.1.2 迁移（根因修复，2026-08-30 真机验证）

- **迁移动因**：2.6.3 的 lwip2（2019）在真实互联网 TLS 会话中于 **SYS 上下文死自旋**（`ctx: sys` 软 WDT，串口栈转储确证）——LAN 上永不复现，仅外网 RTT/丢包触发；Arduino 层所有等待路径均正常 yield，无应用层修复可能
- 升级 **TFT_eSPI 2.3.58 → 2.5.43**（User_Setup.h 兼容保留）、**WiFiManager 2.0.3-alpha → 2.0.17**；sketch 侧唯一 API 改动：`setFollowRedirects(bool)` → `HTTPC_STRICT_FOLLOW_REDIRECTS` 枚举
- **迁移回归三连（全部踩过，留档）**：
  1. **背光**：3.x `analogWrite` 默认量程 1023→255，本面板背光**低电平有效**，超量程值被钳制成常高电平 = 永久熄屏 → 新增 `blWrite()` 统一换算（sketch 内部保持 0-1023 惯例），全部 8 处背光写入收口；**TFT_eSPI 2.5.43 无 `TFT_BACKLIGHT_ON` 默认值，且本面板绝不可由库驱动 D1**（User_Setup.h 已注释禁用）
  2. **flash 布局**：3.x `Update.begin` 从 FS 区底部向下放置更新；CLI 构建漏写 `eesz` 会烙入 1M 布局 → 732KB 更新无处安放（`UPDATE_ERROR_SPACE`）。**CLI 构建固定 `--fqbn esp8266:esp8266:nodemcuv2:eesz=4M2M`**；换内核后 IDE 的 Tools→Flash Size 必须重新核对
  3. **IDE 库缓存**：修改库内部头文件/源文件（User_Setup.h、TFT_eSPI.cpp）IDE 2.x 可能不重编库——"改了没生效"时先清 `%LOCALAPPDATA%\Temp\arduino` 再怀疑代码
- 终验：**OSS HTTPS OTA 全链路真机通过**（V1.4.6 → V1.4.8，732,192 字节，3 段×256KB，MD5 verified，堆量全程 13.9–16.7K 无泄漏趋势）；RAM 45,004/80,192 (56%) / flash 685,936 (65%)

---

## 2026-08-26 · V1.3 MQTT 多设备改造（2026-08-30 真机验证通过，DSH 正式接入）

- 迁移阿里云 IoT → **EMQX Cloud Serverless**（托管、TLS 8883）：删除一机一密 HMAC 动态签名整块代码，认证简化为 deviceId + token
- **逐台发证**：deviceId + token 存 EEPROM（地址 **150-194**，置于 WiFi 结构体 30-125 与健康计数 140-145 之后的真空带；旧址 60-104 会被 `savewificonfig()` 每次开机写入的 ssid/psw 共 96 字节整体覆盖，属必炸时炸弹），串口 `0x06 <id> <token>` 写入、`0x07` 擦除；固件不再内置任何共享秘钥
- **deviceId = MAC 去分隔符后末 6 位 hex**（`3c:8a:1f:43:21:6c` → `TT-43216c`，即 MQTT username）；clientId 即 deviceId（不叠 `tt-` 前缀）
- 话题 `thinktime/<deviceId>/state`，每台独立身份；未发证设备跳过 MQTT（时钟/天气照常，开机画面显示 MAC 建议 ID）
- 启动页保持原版布局，仅在进度条下新增一行 font 2 黄绿 ID（已发证显示 deviceId，未发证显示建议 ID）
- TLS：`WiFiClientSecure` + BearSSL 缓冲裁剪（rx 4096 / tx 1024，握手堆峰值 ~16KB）；v1 暂 `setInsecure()`，量产前内嵌 ISRG Root X1 做证书校验
- NTP 服务器换 `pool.ntp.org`（海外买家可达）；MQTT 连接不再依赖 NTP 先行
- PubSubClient 缓冲 512、socket 超时 5s（broker 失联快速失败）
- **PC 侧正式接入**（真机验证 2026-08-30）：
  - EMQX Cloud Serverless 共享前端**按 SNI 路由租户**——桥（`dsh-mqtt-bridge.cjs`）与插件进程内发布（`thinktime/lib/mqtt.js`）的 `tls.connect` 都必须显式 `servername: host`，否则报 CONNACK 5 / connection closed
  - 配置必须**显式 `"tls": true`**：插件 DEFAULTS 的 `tls: false` 是阿里云 1883 明文时代的遗产，config 缺省该键会明文连 8883 被服务器掐断；config 双份（工作区根 + `Thinktime_DshPlugin`，内容一致，逐条热加载）；**禁止带 BOM**（PS 5.1 `Set-Content -Encoding UTF8` 写 BOM 会令 JSON.parse 失败）
  - 真实会话自动驱动：DSH host 每次模型调用经 `llm/stream` 钩子发布 Estimate/Thinking/Generating/Done（fire-and-forget 不阻塞对话）；多步工具调用中间步骤无 done，最后一步收尾
  - **常驻连接低延迟化**：插件改保持一条 MQTT 长连接（PINGREQ 保活、断线自动重连），每条消息 ~35ms（原先逐条新建 TLS 连接 ~0.5-1s）；并发发布在连接层串行化（单领导者建连，后来者等待复用）——Serverless 共享前端**不接受同 clientId 会话接管**（重复即 CONNACK 2），并发竞态与残留会话均靠 clientId 按进程唯一（`-p<进程号>` 后缀）根治
  - **Thinking 提前到流开始**（原为首 个 reasoning-delta）：与 DSH 界面同步，大上下文预填充期间设备不再落后；流异常补发 Done 防设备悬挂在呼吸态
  - **clientId 命名约定**：`thinktime-<来源工具>[-p<进程号>]`——当前 `thinktime-dsh`，为后续 Codex / Claude Code / Cursor 接入预留命名空间（同话题多发布方互不冲突，换 clientId 即可接入）
- 新增 `Thinktime_DshPlugin/send-to-device.cjs`：`thinking|generating|done|estimate` 手动发布验证
- 编译：Sketch 698,584 bytes (66%) / RAM 44,112 (53%)

---

## 2026-08-25 · V1.2 健康提醒页（When AI thinks, you move 第一落地）

- 长任务健康提醒：AI 预估时长 > 45s 时，在工作时段（9:00–18:30）内随机提醒一项微休息——喝水 / 上厕所 / 舒缓眼睛 / 舒缓肩颈
- `state=Estimate` 单独成帧：只记账不驱动状态机（防无 done 悬挂呼吸）；触发被拦时串口打印原因（时段外/间隔不足/配额满）；串口 `0x05` 强制演练提醒页（忽略时段/间隔，done 即整屏还原），`0x05 1~4` 可定向演练指定活动（纯显示测试，不计数）
- **整屏接管**：触发后整个屏幕（时钟/天气/城市）淡出，只剩纯黑底 + 提醒标题；done 后整屏还原旧页面。全屏淡出/淡入用背光 PWM 压暗/回升实现，避开 240×240 大画布的堆风险
- 提醒页五阶段编排：背光压暗(500ms) → 暗场画图标+文案+背光回升(400ms) → 驻留（静态海报，无逐帧动画）→ done 后背光压暗(500ms) → 暗场还原旧页面+背光回升(400ms)
- 接管期间时钟/天气绘制挂起（数据照常刷新缓存），收场时暗场还原时钟+天气+日期
- 每日配额：喝水 6 次、上厕所 4 次、眼睛 2 次、肩颈 2 次；全局最小间隔 40 分钟（14 次 / 9.5h 的自然节律）；工作时段 9:00–18:30；单任务最多提醒一次（闩锁）
- 提醒页版式：活动彩色图标（**88×88** 黑底 JPEG，Lucide/Material Symbols 免费许可，屏上 x76~164）居中偏上 + **两行提醒文案**在下方（`FreeSansBold12pt7b` 粗体，~78% 亮度防刺眼）；页面为静态海报，无逐帧动画
- 两行文案：`Have a Glass / of Water`、`Take a / Bathroom Break`、`Look Far Away / Rest Your Eyes`、`Unwind / Neck & Shoulders`
- 收场还原补画温度/湿度底部贴片（fillScreen 擦除后的静态元素逐一还原）
- 代码卫生：全部注释改为英文，源码（.ino 与全部 .h）零非 ASCII 字符，杜绝编码工具链事故
- 今日计数 EEPROM 持久化（地址 140–145：日期键 + 四项计数），断电不重置，跨天自动清零
- 健壮性：跨任务 estSec 残留清零；NTP 未同步时不做跨天判定；done 在任意阶段到达都能从当前亮度平滑收场

---

## 2026-08-25 · V1.1 AI 呼吸灯（消息带重构 + 配色体系）

### AI 状态呼吸灯

- 消息带重构为**四态状态机**：`日期 ⇄ 开场字幕 ⇄ 呼吸光团 ⇄ 绿盘收尾`；纯文本 MQTT 消息按约定忽略不上屏
- **开场字幕**：模型名居中渐入(400ms)→驻留(900ms)→渐出(500ms)；颜色跟随相位目标——Thinking=奶油 / Generating=冰川蓝，作为随后光团的色彩预告；将来可替换为各家 LLM logo（JPEG 无 alpha，建议预生成多档亮度帧）
- **呼吸光团**：纯光团无文字。「清亮核心＋紧致光晕」双组件渲染——核心 6–9px 提供锐利焦点，峰值中心透出一点白热；光晕幂曲线快速衰减，整体直径约 42px。16 位色专属画布（72×60 懒分配常驻、回日期释放，内存不足自动降级全宽画布）
- **呼吸节律**：非对称双段余弦——Thinking 吸气 40%/呼气 60%（2.6s 慢深呼吸），Generating 对称快呼吸（1.4s 快亮）；相位累加器驱动节拍，变速换挡不跳拍、长阻塞钳制步长防突跳；两态切换 600ms 渐变过渡
- **done 收尾**：三段式 2.6s——转绿 0.5s → 鲜绿 `(88,228,128)` 满亮定格 1.45s → 熄灭 0.65s
- **日期渐现接场**：收尾熄灭后日期以 600ms 黑底淡入（字幕被取消时同样生效），与字幕/光团共用同一 240×60 画布与中心点（屏上 120,180），状态切换零跳动
- estSec 解析保留（预留健康提醒阈值判定，暂不上屏）
- 移除：FIFO 消息队列、两行文字卡、飞入飞出动画、"..." 点动画、千分位格式化、`<queue>/<deque>` 依赖
- 新增 `[BAND]` 串口调试日志：打印每条 MQTT 报文到达时刻与全部状态迁移，便于链路排障

### 配色体系

- 新增**集中调色板**：`COL_BG / COL_INK_1~3 / COL_MODEL / COL_TEXT_HI / COL_BRIDGE / COL_GEN / COL_DONE`，全局颜色一处定义（ThinkTime.ino 顶部）
- 层级体系：纯黑底 → 中性墨阶（城市名/日期/温湿度/模型名依次递减）→ 冷暖对撞信号色（奶油 Thinking `(255,216,168)` 与冒号同源 / 冰川蓝 Generating `(150,195,240)`）→ 鲜绿仅完成定格使用
- 背景**维持纯黑 `(0,0,0)`**：实测近黑 `(10,10,14)` 屏显发灰，弃用（含启动画面与所有清屏处统一引用 `COL_BG`）
- 墨阶全部采用**中性灰**：城市名 `(198,198,198)` / 日期 `(168,168,168)` / 温湿度 `(146,146,146)` / 模型名 `(128,128,128)`——原暖灰配方在此面板屏显偏红，弃用；各级亮度与原设计一致
- **未改动**：时/分数字、天气/温湿度图标等预渲染 JPEG 资源的颜色

---

## 2026-08-16 · V1.0 后续优化

- 日期字号调大：`FreeSans9pt7b` → `FreeSans12pt7b`（细体）
- 温度/湿度字号调整：Font2 `size=3` 曾出现文字不可见，改用 `FreeSans9pt7b` 细体绘制；温度为 数字+手绘度数圆+C，湿度直接绘制 `"60%"`
- 温湿度改为底端对齐、细体小字号显示
- 拆分 README：更新历史移入本 changelog

---

## 2026-08-15 · V1.0 欧美化改造

### 界面显示

- 日期由 `YYYY-MM-DD` 改为欧美格式 `"Wed, Aug 14"`，并水平居中
- 时钟改为标准"时:分"效果：删除秒钟，时分之间加入居中闪烁冒号
- 时钟数字间距加大 8px，避免 `09` 等宽数字粘连
- 冒号颜色由纯白改为优雅过渡奶油色 `(255,220,170)`
- 时钟整体下移 6px，与上方天气图标拉开间距

### 温度 / 湿度

- 去除温度/湿度进度条（`tempWin` / `humidityWin` 及关联变量）
- 温度、湿度合并为一行显示于屏幕底部，图标+数值居中排列
- 摄氏度符号由于内置字体不含 `℃`/`°` 字形，改为手绘小圆 + `C` 显示

### 天气图标

- 天气图标由原 23 张自定义位图更换为 **Erik Flowers Weather Icons**（经典线条风格）
- 新增 `tools/generate_weather_icons.py` 一键生成
- 图标尺寸 60x60 → **48x48**
- 精简天气图标：删除 13 个未使用图标，保留 10 个：
  `t0` 晴 / `t1` 基本晴 / `t2` 多云 / `t3` 毛毛雨 / `t4` 雷暴 / `t7` 雨 / `t9` 阴 / `t14` 雪 / `t18` 雾 / `t99` 未知

### 移除功能

- 移除 **DHT11 温湿度传感器**全部代码（GPIO12、`DHT.h`、`IndoorTem`、Web/配网 DHT 开关、EEPROM `DHT_addr`）
- 移除 **太空人动画**（`img/pangzi i0~i9`、`imgAnim`）
- 移除 **天气更新间隔设定**（串口 0x04 命令、Web 字段、WiFiManager 参数、EEPROM `UpWeT_addr`），天气固定每 10 分钟刷新
- 移除 **中文字体库** `ZdyLwFont_20`，改用 TFT_eSPI 内置字体 / FreeSans 系列英文字体
- 移除 **WiFi 休眠/唤醒逻辑**（`WiFi.forceSleep*`、`Wifi_en`），Web 服务器常开、WiFi 始终在线
- 移除 `WebSever_EN` 使能宏（Web 服务无条件编译启用）
- 串口指令顺移：重置 WiFi 由 `0x05` 改为 `0x04`

### 其他

- 屏幕亮度默认值统一调整为 **30**（全局变量 / Web 页面 / 配网页）
- 城市名：靠屏幕左边缘显示，`FreeSansBold12pt7b` 粗体
- 日期：`FreeSans12pt7b` 普通细体，屏幕居中
- 清理无关过时备注（flash 探测、旧 JPEG 注释等）

---

## 2021-07-19 · V1.0 初始版本（原作者 Misaka）

- 创建 ThinkTime 项目，基础时钟/天气/配网功能
- 后续由 微车游、熊工智能 修改迭代
