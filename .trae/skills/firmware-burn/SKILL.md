---
name: "firmware-burn"
description: "烧录固件到 CC2530 (通过 CCLoader ESP8266 无线烧录器)。当用户要求'烧录''burn''刷机''下载到设备'时调用。使用 burn.py 一键脚本完成 hex→bin 转换 + 上传 + 烧录 + 进度轮询。"
---

# 烧录固件到 CC2530

## 用途

通过 CCLoader API (ESP8266 + CC2530, HTTP API) 将固件无线烧录到 CC2530F256 芯片。

## 触发词

烧录、burn、刷机、下载到设备、flash

## 前置条件

1. **已编译**: `Projects\zstack\HomeAutomation\HGZBSwitch\CC2530DB\RouterEB\Exe\HGZBSwitch.hex` 存在
   - 若不存在, 先触发 `firmware-build` skill
2. **CCLoader 在线**: ESP8266 烧录器已通电并连入网络 (默认 IP `10.0.0.147`)
3. **Python 依赖**: `pip install requests` (burn.py 依赖)

## 一键烧录 (推荐)

```powershell
python d:\vc\Z-Stack\tools\burn.py
```

**可选参数**:
```powershell
# 指定 CCLoader IP
python d:\vc\Z-Stack\tools\burn.py --ip 192.168.1.100

# 指定 hex 文件路径 (相对路径相对于项目根目录)
python d:\vc\Z-Stack\tools\burn.py --hex D:\path\to\firmware.hex

# 仅转换+上传, 不烧录
python d:\vc\Z-Stack\tools\burn.py --no-burn
```

## burn.py 执行流程

| 步骤 | 动作 | API |
|------|------|-----|
| 1 | hex → bin 转换 (填充到 256KB) | 本地 Python |
| 2 | 检查 CCLoader 就绪 | `GET /api/status` |
| 3 | 上传 .bin 到 LittleFS | `POST /api/upload` |
| 4 | 发起异步烧录 (强制校验) | `POST /api/burn` |
| 5 | 轮询进度直到完成 (约 90 秒) | `GET /api/status` |

## 执行步骤

1. **确认 hex 产物存在**, 若不存在则提示先编译
2. **运行 burn.py**:
   ```powershell
   python d:\vc\Z-Stack\tools\burn.py
   ```
3. **观察输出**: burn.py 会实时打印进度 (0% → 100%)
4. **判断结果**:
   - `[OK] 烧录成功!` → 完成, 提示用户重新上电设备
   - `[ERROR]` 或 `[FAIL]` → 报告错误信息
   - 连接超时 → 提示检查 ESP8266 是否在线 (`ping 10.0.0.147`)
5. **烧录后**: 提醒用户重新上电 CC2530 设备, 观察入网和功能

## CCLoader 设备信息

| 项 | 值 |
|----|-----|
| 硬件 | NodeMCU ESP8266 + CC2530 (CC Debug 接线) |
| 默认地址 | `http://10.0.0.147` |
| 烧录接口 | CC Debug (D1=RESET, D2=DC, D6=DD) |
| 串口监控 | ESP8266 RX ← CC2530 P0_3 (UART0 TX) |
| 固件格式 | **仅接受 .bin** (burn.py 自动从 .hex 转换) |

## 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 无法连接 CCLoader | ESP8266 未通电或不在线 | `ping 10.0.0.147`, 检查网络 |
| 设备非 idle | 上次烧录未完成 | `GET /api/status` 查看 state, 必要时重启 ESP8266 |
| 烧录失败 | 接线松动或 CC2530 供电不足 | 检查 D1/D2/D6 接线, 确认 3.3V 供电, 共地 |
| hex 文件不存在 | 未编译或路径错误 | 先触发 `firmware-build` skill |

## API 端点速查 (手动操作用)

| 方法 | 路径 | 功能 |
|------|------|------|
| GET | /api/status | 状态/烧录进度 |
| GET | /api/files | 已上传 BIN 列表 |
| POST | /api/upload | 上传 BIN (multipart, 字段 file) |
| POST | /api/burn | 发起烧录 (异步+强制校验) |
| POST | /api/reset | 复位 CC2530 |
| POST | /api/monitor | 开始串口监控 |
| GET | /api/monitor/buffer?since=N | 获取串口日志 |

> 手动分步操作时, PowerShell 中必须用 `curl.exe`, 不能用 `curl` (后者是 Invoke-WebRequest 别名)。
