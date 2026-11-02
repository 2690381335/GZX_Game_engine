# GZX Engine —— Node.js 绑定

基于 [koffi](https://koffi.dev/) 的 FFI 封装，把引擎的 69 个 C 函数包装成 JavaScript 类。无需编译，装上就能用。

```js
const { Engine, World, Font, SpriteBatch, Key, Mouse, Space, Sort, BodyType, Shape, WHITE, FULL_UV } = require('./gzx');
```

---

## 安装

```bash
npm install koffi
```

引擎动态库（`bin/engine.dll` 及其依赖）由 `gzx.js` 自动查找，顺序为：环境变量 `GZX_LIBRARY` → 同目录 → `../../bin/` → `../bin/` → 系统路径。通常不用干预。

---

## 核心概念

**坐标空间** — 每个绘制调用都要指定空间：

| 常量 | 含义 |
|---|---|
| `Space.WORLD` | 世界坐标，受相机影响（默认） |
| `Space.SCREEN` | 屏幕坐标，不受相机影响，做 UI / HUD 用 |

**主循环** — 两种写法，推荐 `run()`：

```js
// 写法一（推荐）：run() 退出时自动销毁引擎
engine.run((dt) => {
  // dt 是本帧时间增量（秒），上限 0.25
});

// 写法二：手动控制
while (engine.beginFrame()) {
  const dt = engine.deltaTime;
  engine.endFrame();
}
engine.close();
```

**颜色** — `{ r, g, b, a }` 各分量 `0` 到 `1`，`a` 可省略（默认 1）。内置 `WHITE`。

**纹理 ID** — `loadTexture()` 返回的数字句柄。同一路径重复加载返回同一 ID，内置 `WHITE_TEXTURE`（=0）是 1×1 白图，配 `drawRect` 或纯色精灵用。

---

## Engine

引擎实例，一个实例对应一个窗口，**必须在主线程使用**。

```js
const engine = new Engine({
  title: '我的游戏',      // 默认 'GZX Engine'，UTF-8
  width: 1280,            // 默认 1280
  height: 720,            // 默认 720
  resizable: true,        // 默认 true
  pauseWhenMinimized: true, // 最小化时是否阻塞等待，默认 true
});
```

### 生命周期与主循环

| 成员 | 说明 |
|---|---|
| `run(frame)` | 运行主循环直到窗口关闭，退出后自动销毁。`frame(dt)` 每帧回调 |
| `beginFrame()` | 开始新的一帧，返回 `false` 表示应退出 |
| `endFrame()` | 渲染本帧提交的内容 |
| `close()` | 销毁引擎和窗口 |
| `deltaTime` | 本帧时间增量（秒，上限 0.25） |
| `requestQuit()` | 请求退出 |
| `viewport` | 视口尺寸 `[宽, 高]` |

> **注意**：`deltaTime`、`viewport`、`mousePosition`、`mouseWheel` 读取的是**当前帧**的状态，必须在 `beginFrame()` **之后**读取才有意义。在 `beginFrame()` 之前读取会得到 `0` / `[0, 0]`（例如刚 `new Engine()` 后立刻读 `viewport` 是 `[0, 0]`）。
>
> 这些是**属性而非方法**，直接取值即可，不要加括号：`engine.deltaTime` ✅、`engine.deltaTime()` ❌。同理 `spriteCount`、`audioAvailable`、`world.bodyCount`、`font.lineHeight` 都是属性。

### 输入

| 成员 | 说明 |
|---|---|
| `keyDown(key)` / `keyPressed(key)` / `keyReleased(key)` | 键盘：当前按下 / 本帧按下 / 本帧释放 |
| `mouseDown(btn)` / `mousePressed(btn)` / `mouseReleased(btn)` | 鼠标，按键取 `Mouse.LEFT` 等 |
| `mousePosition` | 鼠标窗口位置 `[x, y]` |
| `mouseWheel` | 本帧滚轮量 `[水平, 垂直]` |

按键常量：`Key.LEFT` `Key.RIGHT` `Key.UP` `Key.DOWN` `Key.SPACE` `Key.ESCAPE` `Key.ENTER` `Key.TAB` `Key.A`~`Key.Z` `Key.NUM0`~`Key.NUM9` `Key.F1`~`Key.F12`，以及 `LEFT_SHIFT` / `LEFT_CTRL` / `LEFT_ALT` 等修饰键。
鼠标常量：`Mouse.LEFT` `Mouse.MIDDLE` `Mouse.RIGHT` `Mouse.X1` `Mouse.X2`。

### 纹理

| 方法 | 说明 |
|---|---|
| `loadTexture(file)` | 加载图片（PNG/JPG/WEBP/TIFF/AVIF 等）。失败抛 `GzxError` |
| `createTexture(width, height, rgba)` | 从 RGBA8 像素创建，`rgba` 为 `Uint8Array`，长度须为 `width*height*4` |
| `textureSize(texture)` | 返回 `[宽, 高]` |
| `textureRegion(texture, x, y, w, h)` | 取纹理内像素矩形对应的 `Uv` |
| `Engine.sheetUv(columns, rows, frame)` | **静态方法**。网格精灵表第 `frame` 帧的 `Uv`，帧按行优先编号 |

### 绘制

| 方法 | 说明 |
|---|---|
| `setBackground(color)` | 设置本帧背景色 |
| `setCamera(x, y, zoom)` | 设置相机，`(x, y)` 为视野**左上角**的世界坐标 |
| `setSort(mode)` | 精灵排序模式，见下表 |
| `drawSprite(texture, x, y, w, h, options?)` | 画单个精灵，少量精灵用 |
| `drawSprites(batch)` | 一次提交整个 `SpriteBatch`，**大量精灵必须用这个** |
| `drawRect(x, y, w, h, color?, space?, layer?)` | 画纯色矩形 |
| `spriteCount` | 本帧已提交的精灵数量 |
| `pick(x, y)` | 屏幕坐标处最上层精灵的提交序号，无则返回 `null` |
| `screenToWorld(x, y)` / `worldToScreen(x, y)` | 坐标互转，返回 `[x, y]` |

`setSort` 模式：

| 常量 | 行为 |
|---|---|
| `Sort.NONE` | 不排序，严格按提交顺序 |
| `Sort.LAYER` | 按层级稳定排序 |
| `Sort.LAYER_TEXTURE` | 按层级再按纹理排序，**合批效率最高** |

`drawSprite` / `SpriteBatch.add` 的 `options`：

```js
{
  uv: FULL_UV,        // {left, top, right, bottom}，左右交换可水平翻转
  color: WHITE,       // {r, g, b, a}
  rotation: 0,        // 弧度
  pivotX: 0.5, pivotY: 0.5,  // 旋转中心，相对尺寸比例
  layer: 0,           // 越大越靠上
  space: Space.WORLD,
}
```

### 文字

| 方法 | 说明 |
|---|---|
| `defaultFont()` | 内置像素字体，只支持 ASCII，开箱可用 |
| `gridFont(texture, cellWidth, cellHeight, first?, count?)` | 从网格排布的位图字体纹理创建 |
| `ttfFont(file, pixelSize, characters?)` | 从 TTF/OTF 创建，`characters` 传入要烘焙的字符集（中文场景必填） |
| `drawText(font, text, x, y, options?)` | 绘制。`options`: `{scale, color, space, layer}` |
| `font.measure(text, scale?)` | 返回 `[宽, 高]`，用于居中排版 |
| `font.lineHeight` | 行高（像素） |

```js
const font = engine.ttfFont('C:/Windows/Fonts/msyh.ttc', 32, '中文渲染测试');
const [w, h] = font.measure('中文渲染测试', 1);
engine.drawText(font, '中文渲染测试', (1280 - w) / 2, 40, { scale: 1, color: WHITE, space: Space.SCREEN });
```

### 音频

| 成员 | 说明 |
|---|---|
| `audioAvailable` | 音频设备是否可用 |
| `loadSound(file)` | 加载 WAV / OGG / MP3 / FLAC |
| `createSound(samples, channels?, sampleRate?)` | 从 `Float32Array` 采样数据创建 |
| `play(sound, {volume?, pan?, loop?})` | 播放，返回 `BigInt` 声部句柄，失败返回 `null` |
| `stop(voice)` / `stopAll()` | 停止 |
| `playing(voice)` | 是否仍在播放 |
| `setVoiceVolume(voice, v)` / `setVoicePan(voice, p)` / `setMasterVolume(v)` | 音量与声像，`0`~`1` |

```js
const sfx = engine.loadSound('hit.wav');
const voice = engine.play(sfx, { volume: 0.8, pan: 0, loop: false });
```

---

## SpriteBatch

可复用的精灵缓冲区。**超过几十个精灵就用它**，比逐个 `drawSprite` 快一个数量级。

```js
const batch = new SpriteBatch(1024);   // 初始容量，满时自动翻倍

batch.clear();                          // 每帧开始清空，容量保留
batch.add(texture, x, y, w, h, options);
engine.drawSprites(batch);              // 一次提交
```

| 成员 | 说明 |
|---|---|
| `count` | 当前精灵数量 |
| `capacity` | 当前容量 |
| `clear()` | 清空缓冲区，保留容量 |
| `add(texture, x, y, w, h, options?)` | 追加精灵，`options` 同 `drawSprite` |

---

## World（2D 物理）

独立于引擎运行的物理世界，可单独使用（例如服务端跑逻辑）。

```js
const world = new World(0, 980, 64);   // gravityX, gravityY, cellSize(空间划分尺寸)
```

### 刚体

```js
const body = world.createBody({
  type: BodyType.DYNAMIC,   // STATIC / DYNAMIC / KINEMATIC
  shape: Shape.BOX,         // BOX / CIRCLE
  x: 640, y: 100,           // 必填
  halfWidth: 16, halfHeight: 16,   // BOX 用
  radius: 16,                       // CIRCLE 用
  velocityX: 0, velocityY: 0,
  mass: 1, restitution: 0, friction: 0.2,
  linearDamping: 0, gravityScale: 1,
  category: 1, mask: 0xFFFFFFFF,    // 碰撞过滤位
  sensor: false,
  userData: 0n,                     // BigInt
});
```

| 成员 | 说明 |
|---|---|
| `createBody(d)` | 创建刚体，返回 `BigInt` 句柄 |
| `destroyBody(body)` / `valid(body)` | 销毁 / 句柄是否有效 |
| `position(body)` / `setPosition(body, x, y)` | 位置读写 |
| `velocity(body)` / `setVelocity(body, x, y)` | 速度读写 |
| `applyImpulse(body, x, y)` | 施加冲量 |
| `userData(body)` | 读用户数据，返回 `BigInt` |
| `positions(bodies, out?)` | 批量读位置，返回 `Float32Array`（x, y 交错） |
| `bodyCount` | 刚体数量 |

### 世界与查询

| 成员 | 说明 |
|---|---|
| `step(dt, substeps?, iterations?)` | 推进模拟，默认 `substeps=1, iterations=4` |
| `setGravity(x, y)` | 修改重力 |
| `contacts()` | 本帧接触列表，元素为 `{a, b, normalX, normalY, depth, sensor}` |
| `queryPoint(x, y, mask?)` | 点查询，返回句柄数组 |
| `queryRect(l, t, r, b, mask?)` | 矩形查询，返回句柄数组 |
| `raycast(x0, y0, x1, y1, mask?)` | 射线检测，命中返回 `{body, x, y, normalX, normalY, fraction}`，否则 `null` |
| `close()` | 销毁世界 |

```js
world.step(dt);
for (const c of world.contacts()) {
  if (c.sensor) console.log('触发事件', c.a, c.b);
}
const hit = world.raycast(0, 0, 100, 100);
if (hit) console.log('命中距离比例', hit.fraction);
```

---

## 错误处理

引擎调用失败时抛出 `GzxError`，错误信息来自引擎内部：

```js
const { GzxError } = require('./gzx');

try {
  engine.loadTexture('不存在的.png');
} catch (e) {
  if (e instanceof GzxError) console.error('引擎错误:', e.message);
}
```

`loadTexture` / `loadSound` / `createBody` / `new Engine()` 在失败时都会抛 `GzxError`。

---

## 完整示例

同目录 `demo.js` 是与 C++ 版 `examples/main.cpp` 等效的全功能演示，涵盖本文档的全部 API：

| 能力 | 演示方式 |
|---|---|
| `createTexture` | 程序化生成 7 张纹理：圆盘、光晕、木箱、草地、尖刺、天空渐变、山脉剪影 |
| `createSound` | 程序化合成 6 个音效，含 8 秒琶音背景音乐（三角波主旋律 + 贝斯） |
| 精灵表动画 | 待机 10 帧 / 奔跑 16 帧 / 攻击 7 帧 / 受击 4 帧，交换 UV 实现左右翻转 |
| `World` 物理 | 地形、平台、木箱金字塔、小球、运动学移动平台、传感器触发器 |
| 接触与查询 | 金币拾取（sensor 接触）、尖刺受伤、`raycast` 着地检测、`queryRect` 攻击判定 |
| `SpriteBatch` | 所有绘制合并进单个批次，一次 `drawSprites` 提交，含纯色矩形复用白色纹理 |
| 相机 | 指数平滑跟随、滚轮缩放、受击屏幕震动 |
| 界面 | 带阴影文字、半透明面板、帧时间曲线、三列对齐的操作说明 |
| 其他 | `pick` 鼠标拾取、粒子池与 swap-pop 回收、视锥剔除、视差背景 |

运行：

```bash
node demo.js          # 或 npm run demo
```

**验证运行环境**（自动跑完 10 项断言并输出结果，无需手动操作）：

```bash
GZX_DEMO_SELFTEST=1 node demo.js
```

自检覆盖重力着地、水平移动、跳跃、粒子发射、运行期生成刚体、金币拾取、受伤状态机、粒子风暴压力测试、批次提交、音频子系统，退出码 0 表示全部通过。

---

## 注意事项

- **主线程限制**：`Engine` 必须在主线程创建和驱动，窗口与 Vulkan 上下文不支持跨线程。
- **字符串编码**：传给引擎的字符串必须是 UTF-8，源码文件保存为 UTF-8 即可。
- **句柄类型**：刚体句柄与声部句柄是 `BigInt`，比较时注意类型（`body === otherBody` 而非 `==`）。
- **每帧生命周期**：绘制指令只在当前帧有效，`beginFrame()` 后提交的才会在 `endFrame()` 渲染；相机、背景色、排序模式也需每帧设置。
- **性能要点**：精灵数量大时，把背景、世界、UI 全部塞进**同一个** `SpriteBatch`，最后只调用一次 `drawSprites()`。纯色矩形用 `batch.add(WHITE_TEXTURE, ...)` 画，比反复调用 `drawRect()` 少得多跨语言开销 —— `demo.js` 中上万个粒子正是靠这一点稳定在 60 FPS。
- **资源释放**：`engine.close()`、`world.close()`、`font.close()` 用完后调用，或依赖 `run()` 自动清理。
