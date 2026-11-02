# GZX Engine SDK

GZX 是一个基于 **Vulkan + SDL3** 的 2D 游戏引擎。本 SDK 让 **Node.js 与 Python 开发者**直接调用引擎能力，无需编写或编译任何 C++ 代码。

引擎本体是原生动态库（`engine.dll`）。Node.js 侧通过 [koffi](https://koffi.dev/) 做 FFI 调用（`gzx.js`，已把 69 个 C 函数封装成类）；Python 侧通过标准库 **ctypes** 调用（`gzx.py`，零依赖）。两者 API 一一对应，均开箱即用。

---

## 一、30 秒上手

**Node.js**（依赖已预置，不用 npm install）：

```bash
cd bindings/nodejs
node demo.js
```

**Python**（零依赖，标准库 ctypes）：

```bash
cd bindings/python
python demo.py
```

会打开一个 1280×720 的窗口，是与 C++ 版 `examples/main.cpp` 等效的**全功能演示**：平台跳跃 + 物理沙盒 + 粒子压力测试。

包含：程序化生成的 7 张纹理与 6 个音效（含 8 秒背景音乐）、角色待机/奔跑/攻击/受伤四套动画、金币拾取、陷阱受伤击退、激光推动物体、粒子爆发与风暴、视差星空与山脉、旋转风车、相机跟随与屏幕震动、帧时间曲线、操作说明面板、鼠标拾取。

**操作**：`A/D` 移动 · `W/空格` 跳跃 · `J` 攻击 · 左键生成木箱 · `Shift+左键` 生成小球 · 按住右键激光 · 滚轮缩放 · `P` 粒子风暴 · `F1` 排序模式 · `F2` 碰撞框 · `Q/E/M` 音量 · `R` 重置 · `H` 帮助 · `ESC` 退出

**验证运行环境**（无需操作，自动跑完 10 项断言并输出结果）：

```bash
GZX_DEMO_SELFTEST=1 node demo.js      # Node.js
GZX_DEMO_SELFTEST=1 python demo.py    # Python
```

---

## 二、最小示例

**Node.js**（`game.js`，放在 `bindings/nodejs/` 下，`node game.js`）：

```js
const { Engine, Key } = require('./gzx');

const engine = new Engine({ title: '我的游戏', width: 1280, height: 720 });
const texture = engine.loadTexture('Sprites/IDLE.png');

engine.run((dt) => {
  if (engine.keyPressed(Key.ESCAPE)) engine.requestQuit();
  engine.drawSprite(texture, 100, 100, 96, 96);   // 画一个精灵
});   // run() 退出时自动销毁引擎
```

**Python**（`game.py`，放在 `bindings/python/` 下，`python game.py`）：

```python
from gzx import Engine, Key

engine = Engine("我的游戏", 1280, 720)
texture = engine.load_texture("Sprites/IDLE.png")

for dt in engine.frames():
    if engine.key_pressed(Key.ESCAPE):
        engine.request_quit()
    engine.draw_sprite(texture, 100, 100, 96, 96)   # 画一个精灵
# frames() 结束时自动销毁引擎
```

---

## 三、目录结构

```
gzx-sdk/
├── bindings/
│   ├── nodejs/              ← Node.js 绑定（koffi FFI）
│   │   ├── gzx.js           绑定库（唯一需要 require 的文件）
│   │   ├── demo.js          完整功能演示
│   │   ├── package.json / package-lock.json
│   │   ├── node_modules/    已预置 koffi（Windows x64），无需 npm install
│   │   └── README.md        ← Node.js 完整 API 文档
│   └── python/              ← Python 绑定（ctypes，零依赖）
│       ├── gzx.py           绑定库（唯一需要 import 的文件）
│       ├── demo.py          完整功能演示
│       └── README.md        ← Python 完整 API 文档
├── bin/                     ← 原生引擎及依赖（Release 构建），必须随程序一起分发
│   ├── engine.dll           引擎本体
│   └── SDL3*.dll 等         图形/图片/音频/字体依赖
├── samples/assets/Sprites/  ← 示例贴图，demo 会读
├── include/gzx.h            ← C API 头文件（874 行完整中文注释，作为行为参考）
├── redist/
│   └── vc_redist.x64.exe    对方电脑缺 VC 运行库时安装
└── LICENSE
```

---

## 四、在你的项目里使用

**Node.js** — 把 `bindings/nodejs/gzx.js`、`node_modules/` 和整个 `bin/` 目录一起复制到你的项目：

```
你的项目/
├── gzx.js
├── node_modules/            ← 从 SDK 一起复制，省掉 npm install
├── bin/engine.dll + 其他 DLL
└── game.js
```

**Python** — 把 `bindings/python/gzx.py` 和整个 `bin/` 目录一起复制到你的项目：

```
你的项目/
├── gzx.py
├── bin/engine.dll + 其他 DLL
└── game.py
```

两个绑定都会按以下顺序自动查找引擎库，一般不用管：

1. 环境变量 `GZX_LIBRARY`（指定绝对路径时用）
2. 绑定文件同目录
3. `../../bin/`
4. `../bin/`
5. 系统搜索路径

**其他平台**（Linux / macOS）：Python 侧 `gzx.py` 直接可用；Node.js 侧删掉 `node_modules/` 后执行 `npm install` 会拉取对应平台的 koffi。注意引擎本体 `engine.dll` 目前只有 Windows 版本。

---

## 五、运行环境要求

| 项目 | 要求 |
|---|---|
| 操作系统 | Windows 10/11 x64 |
| Node.js（用 nodejs 绑定时） | 18 及以上，x64 |
| Python（用 python 绑定时） | 3.10 及以上，x64 |
| 显卡驱动 | 需支持 Vulkan（近几年 NVIDIA / AMD / Intel 驱动均自带） |
| VC++ 运行库 | 2015-2022，多数 Win10/11 已自带；缺失时运行 `redist/vc_redist.x64.exe` |

SDK 本身**不写入任何系统环境变量、不修改注册表**，整个目录可以随意拷贝、删掉即卸载。

> **构建类型**：`bin/` 中的 `engine.dll` 为 **Release 构建**（链接 Release 版 VC 运行库 `MSVCP140.dll`，非 Debug 的 `MSVCP140D.dll`），已开启编译优化、不含调试符号，可直接用于生产分发。SDK 内不含任何 Debug 版本组件。

---

## 六、常见问题

**窗口打不开 / 提示加载失败**
Node.js 侧先运行 `node -e "require('./gzx.js'); console.log('OK')"`，Python 侧先运行 `python -c "import gzx; print('OK')"` 看具体报错。多数是 `bin/` 没跟着一起复制。

**提示缺少 `MSVCP140.dll` / `VCRUNTIME140.dll`**
安装 `redist/vc_redist.x64.exe`。

**提示 `vulkan-1.dll` 缺失**
显卡驱动过旧，去官网更新驱动。

**中文显示为方块**
`ttfFont()` / `ttf_font()` 没找到字体。demo 里的 `FONT_CANDIDATES` 已内置微软雅黑等常见路径，可直接复用。

**中文字体乱码**
传给引擎的字符串必须是 UTF-8，源码文件保存为 UTF-8 即可（Python 侧 `gzx.py` 会自动编码）。

---

## 七、授权

见 `LICENSE`。

> `include/gzx.h` 仅作为 API 行为参考提供（函数语义、参数范围、错误约定的权威说明）。本 SDK 当前面向 Node.js 与 Python 开发者，C/C++ 接入方式（导入库与 CMake 配置）如需提供请另行说明。
