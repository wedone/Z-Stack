# CCLoader OTA 分体固件烧录功能需求

> 文档位置：`D:\vc\Z-Stack\Documents\CCLoader_OTA分体固件烧录_需求.md`
> 目标仓库：`D:\vc\CCLoader\`（独立 git 仓库，需由有工作区权限的会话实施）
> 前置文档：`D:\vc\CCLoader\CCLoader_WebUI_改造需求.md`、`D:\vc\CCLoader\README.md`
> 目标读者：负责实施改造的 AI 或开发者

---

## 1. 背景与目标

### 1.1 现状

CCLoader WebUI 已完成改造，支持单文件（BIN 或 HEX）烧录。但 CC2530 的 **OTA Client 固件是分体结构**：

| 文件 | 烧录地址 | 说明 |
|------|----------|------|
| `Boot.hex`（OTA Bootloader） | 0x0000~0x07FF | 2KB，启动入口，校验并跳转到用户固件 |
| `RouterEB-OTAClient.hex`（应用固件） | 0x0800 起 | Z-Stack + 应用 + OTA Client，约 200KB |

当前 WebUI 的"烧录"页只支持选**一个**文件。如果用户先烧 Boot.hex、再烧 App.hex，每次都会触发**整片擦除**，第二次烧录会把第一次的 Bootloader 擦掉，导致设备无法启动。

### 1.2 目标

在 WebUI 烧录页新增"**OTA 分体固件**"模式，用户一次性选 Boot.hex + App.hex 两个文件，点一次"开始烧录"即完成整个 OTA 固件烧录，无需分两次操作。

### 1.3 设计原则

- **KISS 原则**：优先前端改造，后端零改动
- **复用现有烧录链路**：不新增 API 端点，不修改 `burnFromLittleFS`、`write_flash_memory_block` 等底层函数
- **不影响单文件模式**：原有的单 BIN/HEX 烧录流程保持不变，OTA 分体模式作为新增选项

---

## 2. 现有代码分析

实施前请先阅读以下源码，理解现有逻辑：

### 2.1 源码关键位置

| 文件 | 函数/区域 | 行号（约） | 作用 |
|------|----------|------------|------|
| `data/app.js` | `hex2bin(file)` | 搜索 `async function hex2bin` | 前端 Intel HEX → BIN 转换 |
| `data/app.js` | 上传逻辑 | 搜索 `isHex` / `hex2bin` | 判断 .hex 后缀，调用 hex2bin 后上传 |
| `data/app.js` | 烧录请求 | 搜索 `startBurn` / `/api/burn` | fetch POST 烧录 |
| `data/index.html` | 烧录页 | 搜索 `烧录` / `upload` | 上传区 + 文件列表 + 进度条 |
| `src/CCLoader.ino` | `handleUpload()` | ~986 行 | 接收上传 BIN 写入 LittleFS |
| `src/CCLoader.ino` | `handleBurn()` | ~1029 行 | 异步烧录入队 |
| `src/CCLoader.ino` | `burnFromLittleFS()` | ~727 行 | 核心烧录：擦除整片 → 从 addr=0 连续写 512 字节块 |

### 2.2 现有 hex2bin 逻辑（关键）

`data/app.js` 中的 `hex2bin(file)` 已实现完整的 Intel HEX 解析：

```javascript
// 解析每行，按 physAddr = baseAddr + bAddr 映射到 dataMap
const dataMap = new Map();  // 字节地址 -> Uint8Array(该行数据)
// ...
const physAddr = baseAddr + bAddr;
dataMap.set(physAddr, new Uint8Array(data));
// ...

// 生成 256KB BIN，0xFF 填充，按地址写入数据
const padTo = 0x40000;  // 256KB
const bin = new Uint8Array(binSize);
bin.fill(0xFF);
for (const [addr, data] of dataMap) {
  bin.set(data, addr);  // 关键：按字节地址写入
}
```

**关键洞察**：`hex2bin` 已按字节地址映射数据。所以：
- `Boot.hex` 转换后：BIN 的 `0x0000~0x07FF` 有数据，其余 `0xFF`
- `RouterEB-OTAClient.hex` 转换后：BIN 的 `0x0800` 起有数据，其余 `0xFF`

### 2.3 现有烧录逻辑（关键）

`burnFromLittleFS()` 的核心循环：

```cpp
uint32_t addr = 0;        // 从地址 0 开始
uint8_t buf[512];
while (f.available()) {
  f.read(buf, 512);                        // 从 BIN 读 512 字节
  write_flash_memory_block(buf, addr, 512); // 写入 Flash
  addr += 128;                              // 下一块
}
```

烧录逻辑从 `addr=0` 连续写整个 256KB BIN 到 Flash。**只要合并后的 BIN 在正确地址有数据，Flash 对应区域就会写入正确内容。**

### 2.4 结论

**两个 hex 合并成一个 256KB BIN 后，现有烧录链路完全可用，后端零改动。**

---

## 3. 技术方案

### 3.1 方案选择

| 方案 | 改动范围 | 复杂度 | 推荐度 |
|------|----------|--------|--------|
| **A：前端合并** | 仅 `data/app.js` + `data/index.html` | 低 | ★★★★★（推荐） |
| B：后端分体 API | `src/CCLoader.ino` 新增端点 + 前端 | 中 | 备选 |

**采用方案 A**：前端把 Boot.hex 和 App.hex 分别转成 BIN 数据，按地址合并成一个 256KB BIN，上传后复用现有 `/api/upload` + `/api/burn`。

### 3.2 方案 A 详细设计

#### 3.2.1 前端 UI 改动

在烧录页"上传固件"区域上方，新增模式切换：

```
┌─────────────────────────────────────────────────────┐
│  烧录模式:  ○ 单文件   ● OTA 分体固件                │
├─────────────────────────────────────────────────────┤
│                                                     │
│  〔单文件模式 - 原有 UI 不变〕                       │
│  固件文件: [选择文件...]                             │
│  [上传]                                              │
│                                                     │
│  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    │
│                                                     │
│  〔OTA 分体模式 - 新增〕                             │
│  ① OTA Bootloader:  [选择 Boot.hex]    2KB         │
│  ② 应用固件:       [选择 App.hex]       200KB       │
│  合并预览:                                          │
│    Bootloader  0x0000 ~ 0x07FF  (2 KB)              │
│    应用固件    0x0800 ~ 0x3xxxx  (xxx KB)           │
│    合并后 BIN  256 KB                                │
│  [合并并上传]                                        │
│                                                     │
└─────────────────────────────────────────────────────┘
```

**模式切换实现**：两个 radio button，切换时显示/隐藏对应区域。默认选中"单文件"。

#### 3.2.2 前端 JS 逻辑

新增 `mergeOtaHex(bootFile, appFile)` 函数：

```javascript
// 合并两个 HEX 为一个 256KB BIN
// 复用现有 hex2bin 的解析逻辑，但不生成两个 BIN 再合并，
// 而是共用一个 dataMap，直接合并地址空间
async function mergeOtaHex(bootFile, appFile) {
  const log = [];
  const dataMap = new Map();  // 共用地址映射
  let minAddr = null, maxAddr = null;

  // 解析第一个 hex（Bootloader）
  parseHexToMap(bootFile, dataMap, log, ref minAddr, ref maxAddr);
  // 解析第二个 hex（应用固件）
  parseHexToMap(appFile, dataMap, log, ref minAddr, ref maxAddr);

  // 生成 256KB BIN（与现有 hex2bin 逻辑一致）
  const padTo = 0x40000;
  const binSize = Math.max(maxAddr + 1, padTo);
  const bin = new Uint8Array(binSize);
  bin.fill(0xFF);
  for (const [addr, data] of dataMap) {
    bin.set(data, addr);
  }

  return { bin, log, name: 'OTA_merged.bin' };
}
```

> **实现建议**：把现有 `hex2bin(file)` 重构为 `parseHexToMap(file, dataMap, log)`（解析到共享 map）+ `mapToBin(dataMap, minAddr, maxAddr)`（map 转 BIN）。`hex2bin` 调用这两个函数，`mergeOtaHex` 也调用这两个函数（传同一个 dataMap）。避免代码重复。

**地址冲突检测**：如果两个 hex 的数据地址重叠（理论上不应该，Bootloader 在 0x0000-0x07FF，应用在 0x0800 起），报错提示。

**上传**：合并后得到 256KB `Uint8Array`，转为 `Blob`，复用现有上传逻辑（`fetch('/api/upload', { method: 'POST', body: formData })`）。

**烧录**：上传成功后，文件名如 `OTA_merged.bin` 出现在文件列表，用户选中后点"开始烧录"，复用现有 `/api/burn`。

#### 3.2.3 后端改动

**零改动**。现有 `handleUpload` + `handleBurn` + `burnFromLittleFS` 完全适用。

#### 3.2.4 进度条

合并后的 BIN 是 256KB，烧录进度按现有逻辑（`totalBlocks = 256KB / 512 = 512 块`）正常显示。

可选增强：在烧录日志区标注当前写入的地址段（"正在写入 Bootloader 区域 0x0000" / "正在写入应用固件区域 0x0800"），通过 `current_block * 512` 估算当前字节地址。此为可选优化，不阻塞核心功能。

---

## 4. 详细需求

### 4.1 前端 UI（index.html）

在烧录页"上传固件"卡片顶部，新增模式切换 radio：

```html
<div class="burn-mode-switch">
  <label><input type="radio" name="burn-mode" value="single" checked> 单文件</label>
  <label><input type="radio" name="burn-mode" value="ota"> OTA 分体固件</label>
</div>

<!-- 单文件模式（原有，id 包裹便于显隐） -->
<div id="single-mode" class="burn-mode-panel">
  <!-- 原有的上传区保持不变 -->
</div>

<!-- OTA 分体模式（新增） -->
<div id="ota-mode" class="burn-mode-panel" style="display:none;">
  <div class="ota-file-slot">
    <label>① OTA Bootloader (.hex)</label>
    <input type="file" id="ota-boot-file" accept=".hex">
    <span id="ota-boot-info"></span>
  </div>
  <div class="ota-file-slot">
    <label>② 应用固件 (.hex)</label>
    <input type="file" id="ota-app-file" accept=".hex">
    <span id="ota-app-info"></span>
  </div>
  <div id="ota-merge-preview"></div>
  <button id="ota-merge-upload-btn" disabled>合并并上传</button>
</div>
```

模式切换 JS：
```javascript
document.querySelectorAll('input[name="burn-mode"]').forEach(r => {
  r.addEventListener('change', e => {
    document.getElementById('single-mode').style.display =
      e.target.value === 'single' ? 'block' : 'none';
    document.getElementById('ota-mode').style.display =
      e.target.value === 'ota' ? 'block' : 'none';
  });
});
```

### 4.2 前端 JS（app.js）

#### 4.2.1 重构 hex2bin（可选，建议）

把现有 `hex2bin(file)` 拆分：

```javascript
// 解析 HEX 文件到共享 dataMap
function parseHexToMap(file, dataMap, log, addrRange) {
  // 返回 { log, minAddr, maxAddr }
  // addrRange 是 { min, max } 对象，函数内更新
  // 逻辑与现有 hex2bin 的解析部分完全一致
}

// dataMap 转 256KB BIN
function mapToBin(dataMap, maxAddr) {
  const padTo = 0x40000;
  const binSize = Math.max(maxAddr + 1, padTo);
  const bin = new Uint8Array(binSize);
  bin.fill(0xFF);
  for (const [addr, data] of dataMap) {
    bin.set(data, addr);
  }
  return bin;
}

// 单文件（原有，重构后调用上面两个函数）
async function hex2bin(file) {
  const dataMap = new Map();
  const log = [];
  const addrRange = { min: null, max: null };
  await parseHexToMap(file, dataMap, log, addrRange);
  // ...日志输出...
  const bin = mapToBin(dataMap, addrRange.max);
  return { bin, log, name: file.name.replace(/\.hex$/i, '.bin') };
}
```

#### 4.2.2 新增 mergeOtaHex

```javascript
async function mergeOtaHex(bootFile, appFile) {
  const dataMap = new Map();
  const log = [];
  const addrRange = { min: null, max: null };

  // 解析 Bootloader
  log.push('=== OTA Bootloader ===');
  await parseHexToMap(bootFile, dataMap, log, addrRange);
  const bootMin = addrRange.min, bootMax = addrRange.max;

  // 解析应用固件
  log.push('=== 应用固件 ===');
  await parseHexToMap(appFile, dataMap, log, addrRange);
  const appMin = addrRange.min === bootMin ? addrRange.min : addrRange.min;

  // 地址冲突检测
  // 理论上 Bootloader 在 0x0000-0x07FF，应用在 0x0800+，不重叠
  // dataMap.set 同一地址会覆盖，这里不做严格检测，依赖地址不重叠的约定

  const bin = mapToBin(dataMap, addrRange.max);
  log.push('=== 合并结果 ===');
  log.push('Bootloader: 0x' + bootMin.toString(16) + ' ~ 0x' + bootMax.toString(16));
  log.push('应用固件: 0x' + (addrRange.min > bootMax ? addrRange.min : bootMax + 1).toString(16) + ' ~ 0x' + addrRange.max.toString(16));
  log.push('合并后 BIN: ' + bin.length + ' 字节 (' + (bin.length / 1024).toFixed(1) + ' KB)');

  return { bin, log, name: 'OTA_merged.bin' };
}
```

#### 4.2.3 文件选择交互

```javascript
// 两个文件都选好后，启用"合并并上传"按钮，显示预览
async function updateOtaPreview() {
  const bootFile = document.getElementById('ota-boot-file').files[0];
  const appFile = document.getElementById('ota-app-file').files[0];
  const btn = document.getElementById('ota-merge-upload-btn');

  if (!bootFile || !appFile) {
    btn.disabled = true;
    return;
  }

  // 快速解析显示地址范围（不生成 BIN，只解析 dataMap）
  try {
    // 可复用 parseHexToMap 的轻量版（只算地址范围）
    const bootInfo = await quickHexInfo(bootFile);
    const appInfo = await quickHexInfo(appFile);
    document.getElementById('ota-merge-preview').innerHTML =
      `Bootloader  0x${bootInfo.min.toString(16)} ~ 0x${bootInfo.max.toString(16)}  (${bootInfo.span} 字节)<br>` +
      `应用固件    0x${appInfo.min.toString(16)} ~ 0x${appInfo.max.toString(16)}  (${appInfo.span} 字节)<br>` +
      `合并后 BIN  256 KB`;
    btn.disabled = false;
  } catch (e) {
    document.getElementById('ota-merge-preview').textContent = '预览失败: ' + e.message;
    btn.disabled = true;
  }
}

// 合并并上传
async function otaMergeAndUpload() {
  const bootFile = document.getElementById('ota-boot-file').files[0];
  const appFile = document.getElementById('ota-app-file').files[0];
  const result = await mergeOtaHex(bootFile, appFile);

  // 显示转换日志
  // result.log 逐行显示到上传区

  // 转 Blob 上传（复用现有上传逻辑）
  const blob = new Blob([result.bin], { type: 'application/octet-stream' });
  const formData = new FormData();
  formData.append('file', blob, result.name);

  const resp = await fetch('/api/upload', { method: 'POST', body: formData });
  const data = await resp.json();
  if (data.success) {
    // 刷新文件列表，自动选中 OTA_merged.bin
    refreshFileList();
  }
}
```

### 4.3 后端（CCLoader.ino）

**不修改**。现有 `handleUpload` 接收任意 BIN 文件写入 LittleFS，`handleBurn` + `burnFromLittleFS` 从 addr=0 烧录整个 256KB，完全适用。

---

## 5. 测试固件

用于验证的固件文件路径（Z-Stack 工程编译产物）：

| 文件 | 路径 | 预期地址范围 | 大小 |
|------|------|-------------|------|
| `Boot.hex` | `D:\vc\Z-Stack\Projects\zstack\OTA\Boot\CC2530DB\OTA-Boot\Exe\Boot.hex` | 0x0000~0x0773 | ~1.9KB |
| `RouterEB-OTAClient.hex` | `D:\vc\Z-Stack\Projects\zstack\HomeAutomation\SampleSwitch\CC2530DB\RouterEB - OTAClient\Exe\RouterEB-OTAClient.hex` | 0x0800~0x3xxxx | ~200KB |

测试步骤：
1. 切换到"OTA 分体固件"模式
2. 选择 Boot.hex 和 RouterEB-OTAClient.hex
3. 确认预览显示正确地址范围（Bootloader 0x0000~0x07xx，应用 0x0800~0x3xxxx）
4. 点"合并并上传"，确认 `OTA_merged.bin` 出现在文件列表
5. 选中 `OTA_merged.bin`，点"开始烧录"
6. 烧录完成后，CC2530 应能正常启动（LED 亮，串口有启动日志）
7. 切到监控页，按复位，能看到从 `main()` 开始的启动日志

---

## 6. 验收标准

### 6.1 功能验收

1. **模式切换**：单文件 / OTA 分体 两个 radio 可正常切换，切换时显隐对应区域
2. **文件选择**：OTA 模式下必须选两个 .hex 文件才能启用"合并并上传"按钮
3. **地址预览**：选择文件后显示 Bootloader 和应用固件的地址范围，与预期一致
4. **合并上传**：点击"合并并上传"后，生成 `OTA_merged.bin` 并上传到 LittleFS，文件列表可见
5. **烧录**：选中 `OTA_merged.bin` 点"开始烧录"，进度条正常推进，烧录完成后设备可启动
6. **单文件模式不受影响**：切回单文件模式，原有 BIN/HEX 上传烧录流程完全正常

### 6.2 边界情况

1. **地址不重叠**：Bootloader 数据在 0x0000-0x07FF，应用在 0x0800+，合并后无覆盖
2. **HEX 格式兼容**：支持 Intel HEX 的 type 00/01/02/04/05 记录（复用现有 hex2bin 解析）
3. **文件顺序无关**：先选 Boot 还是先选 App 不影响合并结果
4. **错误提示**：HEX 校验和错误、格式错误时显示行号
5. **只选一个文件**：按钮禁用，提示"请选择两个文件"

### 6.3 非目标（本期不做）

- 后端新增 `/api/burn_ota` 端点（如有需要可作为后续优化）
- 烧录日志区分 Bootloader / 应用区域（可选增强，不阻塞）
- 自动从 IAR 工程目录拉取固件（超出 CCLoader 职责）

---

## 7. 实施指引

### 7.1 实施环境

- 工作区：`D:\vc\CCLoader\`（需有该目录读写权限的会话）
- 开发工具：VSCode + PlatformIO（编译 ESP8266 固件）
- 改动文件：
  - `data/index.html`（新增 OTA 模式 UI）
  - `data/app.js`（新增 mergeOtaHex + 重构 hex2bin）
  - `data/style.css`（新增 OTA 模式样式，可选）

### 7.2 实施步骤

1. **重构 hex2bin**：拆分为 `parseHexToMap` + `mapToBin`，`hex2bin` 调用两者
2. **新增 mergeOtaHex**：调用 `parseHexToMap` 两次（共用 dataMap），再 `mapToBin`
3. **新增 UI**：index.html 烧录页加模式切换 + OTA 文件选择区
4. **新增交互**：app.js 加模式切换、文件选择预览、合并上传逻辑
5. **编译测试**：`python tools/gen_web_assets.py && python -m platformio run`
6. **OTA 升级**：`curl -F "image=@.pio/build/nodemcuv2/firmware.bin" http://<IP>/update`
7. **浏览器测试**：选 Boot.hex + App.hex → 合并上传 → 烧录 → 验证设备启动

### 7.3 注意事项

- **不要修改 `src/CCLoader.ino`**：后端零改动
- **保留单文件模式**：原有逻辑用 `id="single-mode"` 包裹，切换时显隐，不要删除
- **重构 hex2bin 要兼容**：单文件模式仍调用 `hex2bin(file)`，重构后行为不变
- **LittleFS 空间**：合并后 BIN 是 256KB，加上其他文件，LittleFS 1MB 分区够用
- **文件名约定**：合并后文件名固定 `OTA_merged.bin`，方便用户识别

---

## 8. 参考资料

| 资源 | 链接 |
|------|------|
| CCLoader WebUI 改造需求 | `D:\vc\CCLoader\CCLoader_WebUI_改造需求.md` |
| CCLoader README | `D:\vc\CCLoader\README.md` |
| 4路智能开关固件方案（OTA 章节） | `D:\vc\Z-Stack\Documents\4路智能开关_Zigbee固件开发方案.md` 第12章 |
| Intel HEX 格式说明 | https://en.wikipedia.org/wiki/Intel_HEX |
| CC2530 Flash 分区（ota.xcl） | `D:\vc\Z-Stack\Projects\zstack\Tools\CC2530DB\ota.xcl` |

---

*文档创建时间: 2026-07-22*
*版本: v1.0*
