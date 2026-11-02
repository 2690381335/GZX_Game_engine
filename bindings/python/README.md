# GZX Engine —— Python 绑定

基于 **ctypes**（Python 标准库）的 FFI 封装，把引擎的 69 个 C 函数包装成 Python 类。**无需安装任何第三方依赖，无需编译**，把 `gzx.py` 放进项目即可。

```python
from gzx import Engine, World, Font, SpriteBatch, Key, Mouse, Space, Sort, BodyType, Shape, Color, Uv, WHITE, WHITE_TEXTURE
```

---

## 环境要求

- Python 3.10 及以上（Windows x64）
- 引擎动态库（`bin/engine.dll` 及其依赖）由 `gzx.py` 自动查找，顺序为：环境变量 `GZX_LIBRARY` → 同目录 → `../../bin/`（SDK 布局）→ `../bin/` → 系统路径。通常不用干预。

---

## 核心概念

**坐标空间** — 每个绘制调用都要指定空间：

| 常量 | 含义 |
|---|---|
| `Space.WORLD` | 世界坐标，受相机影响（默认） |
| `Space.SCREEN` | 屏幕坐标，不受相机影响，做 UI / HUD 用 |

**主循环** — 两种写法，推荐 `frames()` 迭代器：

```python
# 写法一（推荐）：frames() 退出时自动销毁引擎
for dt in engine.frames():
    ...   # dt 是本帧时间增量（秒），上限 0.25

# 写法二：手动控制
while engine.begin_frame():
    dt = engine.delta_time
    engine.end_frame()
engine.close()
```

**颜色** — `Color(r, g, b, a)`，各分量 `0` 到 `1`，`a` 可省略（默认 1）。任何接受颜色的地方也可以直接传 4 元组。内置 `WHITE`。

**纹理 ID** — `load_texture()` 返回的整数句柄。同一路径重复加载返回同一 ID，内置 `WHITE_TEXTURE`（=0）是 1×1 白图，配纯色矩形用。

---

## Engine

引擎实例，一个实例对应一个窗口，**必须在主线程使用**。

```python
engine = Engine(
    "我的游戏",      # title，默认 "GZX Engine"，UTF-8
    1280, 720,       # width, height，默认 1280x720
    True,            # resizable，默认 True
)
```

### 生命周期与主循环

| 成员 | 说明 |
|---|---|
| `frames()` | 迭代器驱动主循环，窗口关闭后自动销毁，每轮产出 `dt`（秒） |
| `begin_frame()` | 开始新的一帧，返回 `False` 表示应退出 |
| `end_frame()` | 渲染本帧提交的内容 |
| `close()` | 销毁引擎和窗口（也可用 `with Engine(...) as engine:` 上下文管理器） |
| `delta_time` | 本帧时间增量（秒，上限 0.25） |
| `request_quit()` | 请求退出 |
| `viewport` | 视口尺寸 `(宽, 高)` |

> **注意**：`delta_time`、`viewport`、`mouse_position`、`mouse_wheel` 读取的是**当前帧**的状态，必须在 `begin_frame()` **之后**读取才有意义，之前读取得到 `0` / `(0, 0)`。
>
> 这些是 **property 而非方法**，直接取值即可：`engine.delta_time` ✅、`engine.delta_time()` ❌。同理 `sprite_count`、`audio_available`、`world.body_count`、`font.line_height` 都是属性。

### 输入

| 成员 | 说明 |
|---|---|
| `key_down(key)` / `key_pressed(key)` / `key_released(key)` | 键盘：当前按下 / 本帧按下 / 本帧释放 |
| `mouse_down(btn)` / `mouse_pressed(btn)` / `mouse_released(btn)` | 鼠标，按键取 `Mouse.LEFT` 等 |
| `mouse_position` | 鼠标窗口位置 `(x, y)` |
| `mouse_wheel` | 本帧滚轮量 `(水平, 垂直)` |

按键常量：`Key.LEFT` `Key.RIGHT` `Key.UP` `Key.DOWN` `Key.SPACE` `Key.ESCAPE` `Key.ENTER` `Key.TAB` `Key.A`~`Key.Z` `Key.NUM0`~`Key.NUM9` `Key.F1`~`Key.F12`，以及 `LEFT_SHIFT` / `LEFT_CTRL` / `LEFT_ALT` 等修饰键。
鼠标常量：`Mouse.LEFT` `Mouse.MIDDLE` `Mouse.RIGHT` `Mouse.X1` `Mouse.X2`。

### 纹理

| 方法 | 说明 |
|---|---|
| `load_texture(path)` | 加载图片（PNG/JPG/WEBP/TIFF/AVIF 等）。失败抛 `GzxError` |
| `create_texture(width, height, rgba)` | 从 RGBA8 像素创建，`rgba` 为 `bytes` / `bytearray`，长度须为 `width*height*4` |
| `texture_size(texture)` | 返回 `(宽, 高)` |
| `texture_region(texture, x, y, w, h)` | 取纹理内像素矩形对应的 `Uv` |
| `Engine.sheet_uv(columns, rows, frame)` | **静态方法**。网格精灵表第 `frame` 帧的 `Uv`，帧按行优先编号 |

### 绘制

| 方法 | 说明 |
|---|---|
| `set_background(color)` | 设置本帧背景色 |
| `set_camera(x, y, zoom)` | 设置相机，`(x, y)` 为视野**左上角**的世界坐标 |
| `set_sort(mode)` | 精灵排序模式，见下表 |
| `draw_sprite(texture, x, y, w, h, ...)` | 画单个精灵，少量精灵用 |
| `draw_sprites(batch)` | 一次提交整个 `SpriteBatch`，**大量精灵必须用这个** |
| `draw_rect(x, y, w, h, color, space, layer)` | 画纯色矩形 |
| `sprite_count` | 本帧已提交的精灵数量 |
| `pick(x, y)` | 屏幕坐标处最上层精灵的提交序号，无则返回 `None` |
| `screen_to_world(x, y)` / `world_to_screen(x, y)` | 坐标互转，返回 `(x, y)` |

`set_sort` 模式：

| 常量 | 行为 |
|---|---|
| `Sort.NONE` | 不排序，严格按提交顺序 |
| `Sort.LAYER` | 按层级稳定排序 |
| `Sort.LAYER_TEXTURE` | 按层级再按纹理排序，**合批效率最高** |

`draw_sprite` / `SpriteBatch.add` 的可选参数：

```python
uv=FULL_UV,           # Uv(left, top, right, bottom)，左右交换可水平翻转
color=WHITE,          # Color(r, g, b, a)
rotation=0.0,         # 弧度
pivot_x=0.5, pivot_y=0.5,   # 旋转中心，相对尺寸比例
layer=0,              # 越大越靠上
space=Space.WORLD,
```

### 文字

| 方法 | 说明 |
|---|---|
| `default_font()` | 内置像素字体，只支持 ASCII，开箱可用 |
| `grid_font(texture, cell_width, cell_height, first?, count?)` | 从网格排布的位图字体纹理创建 |
| `ttf_font(path, pixel_size, characters?)` | 从 TTF/OTF 创建，`characters` 传入要烘焙的字符集（中文场景必填） |
| `draw_text(font, text, x, y, scale, color, space, layer)` | 绘制，支持换行 |
| `font.measure(text, scale?)` | 返回 `(宽, 高)`，用于居中排版 |
| `font.line_height` | 行高（像素） |

```python
font = engine.ttf_font("C:/Windows/Fonts/msyh.ttc", 32, "中文渲染测试")
width, height = font.measure("中文渲染测试", 1)
engine.draw_text(font, "中文渲染测试", (1280 - width) / 2, 40, 1, WHITE, Space.SCREEN)
```

### 音频

| 成员 | 说明 |
|---|---|
| `audio_available` | 音频设备是否可用 |
| `load_sound(path)` | 加载 WAV / OGG / MP3 / FLAC |
| `create_sound(samples, channels?, sample_rate?)` | 从浮点采样序列创建 |
| `play(sound, volume?, pan?, loop?)` | 播放，返回声部句柄，失败返回 `None` |
| `stop(voice)` / `stop_all()` | 停止 |
| `playing(voice)` | 是否仍在播放 |
| `set_voice_volume(voice, v)` / `set_voice_pan(voice, p)` / `set_master_volume(v)` | 音量与声像，`0`~`1` |

```python
sfx = engine.load_sound("hit.wav")
voice = engine.play(sfx, volume=0.8, pan=0.0)
```

---

## SpriteBatch

可复用的精灵缓冲区。**超过几十个精灵就用它**，比逐个 `draw_sprite` 快一个数量级。

```python
batch = SpriteBatch(1024)     # 初始容量，满时自动翻倍

batch.clear()                 # 每帧开始清空，容量保留
batch.add(texture, x, y, w, h, layer=0)
engine.draw_sprites(batch)    # 一次提交
```

| 成员 | 说明 |
|---|---|
| `count` | 当前精灵数量 |
| `capacity` | 当前容量 |
| `clear()` | 清空缓冲区，保留容量 |
| `add(texture, x, y, w, h, uv?, color?, rotation?, pivot_x?, pivot_y?, layer?, space?)` | 追加精灵 |

---

## World（2D 物理）

独立于引擎运行的物理世界，可单独使用（例如服务端跑逻辑）。

```python
world = World(0.0, 980.0, 64.0)   # gravity_x, gravity_y, cell_size(空间划分尺寸)
```

### 刚体

```python
body = world.create_body(
    640, 100,                    # x, y（位置参数，必填）
    shape=Shape.BOX,             # BOX / CIRCLE
    type=BodyType.DYNAMIC,       # STATIC / DYNAMIC / KINEMATIC
    half_width=16, half_height=16,   # BOX 用
    radius=16,                   # CIRCLE 用
    velocity_x=0, velocity_y=0,
    mass=1.0, restitution=0.0, friction=0.2,
    linear_damping=0.0, gravity_scale=1.0,
    category=1, mask=0xFFFFFFFF,    # 碰撞过滤位
    sensor=False,
    user_data=0,                 # 整数，常用于存游戏语义
)
```

| 成员 | 说明 |
|---|---|
| `create_body(x, y, **定义)` | 创建刚体，返回整数句柄 |
| `destroy_body(body)` / `valid(body)` | 销毁 / 句柄是否有效 |
| `position(body)` / `set_position(body, x, y)` | 位置读写 |
| `velocity(body)` / `set_velocity(body, x, y)` | 速度读写 |
| `apply_impulse(body, x, y)` | 施加冲量 |
| `user_data(body)` | 读用户数据，返回整数 |
| `positions(bodies)` | 批量读位置，返回 `array`（x, y 交错） |
| `body_count` | 刚体数量 |

### 世界与查询

| 成员 | 说明 |
|---|---|
| `step(dt, substeps?, iterations?)` | 推进模拟，默认 `substeps=1, iterations=4` |
| `set_gravity(x, y)` | 修改重力 |
| `contacts()` | 本帧接触列表，元素为 `Contact(a, b, normal_x, normal_y, depth, sensor)` |
| `query_point(x, y, mask?)` | 点查询，返回句柄列表 |
| `query_rect(l, t, r, b, mask?)` | 矩形查询，返回句柄列表 |
| `raycast(x0, y0, x1, y1, mask?)` | 射线检测，命中返回 `RayHit(body, x, y, normal_x, normal_y, fraction)`，否则 `None` |
| `close()` | 销毁世界 |

```python
world.step(dt)
for contact in world.contacts():
    if contact.sensor:
        print("触发事件", contact.a, contact.b)
hit = world.raycast(0, 0, 100, 100)
if hit:
    print("命中距离比例", hit.fraction)
```

---

## 错误处理

引擎调用失败时抛出 `GzxError`，错误信息来自引擎内部：

```python
try:
    engine.load_texture("不存在的.png")
except GzxError as e:
    print("引擎错误:", e)
```

`load_texture` / `load_sound` / `create_body` / `Engine()` 在失败时都会抛 `GzxError`。

---

## 完整示例

同目录 `demo.py` 是与 C++ 版 `examples/main.cpp` 及 Node.js 版 `demo.js` 等效的全功能演示，涵盖本文档的全部 API：

| 能力 | 演示方式 |
|---|---|
| `create_texture` | 程序化生成 7 张纹理：圆盘、光晕、木箱、草地、尖刺、天空渐变、山脉剪影 |
| `create_sound` | 程序化合成 6 个音效，含 8 秒琶音背景音乐（三角波主旋律 + 贝斯） |
| 精灵表动画 | 待机 10 帧 / 奔跑 16 帧 / 攻击 7 帧 / 受击 4 帧，交换 UV 实现左右翻转 |
| `World` 物理 | 地形、平台、木箱金字塔、小球、运动学移动平台、传感器触发器 |
| 接触与查询 | 金币拾取（sensor 接触）、尖刺受伤、`raycast` 着地检测、`query_rect` 攻击判定 |
| `SpriteBatch` | 所有绘制合并进单个批次，一次 `draw_sprites` 提交，含纯色矩形复用白色纹理 |
| 相机 | 指数平滑跟随、滚轮缩放、受击屏幕震动 |
| 界面 | 带阴影文字、半透明面板、帧时间曲线、三列对齐的操作说明 |
| 其他 | `pick` 鼠标拾取、粒子池与原地压缩回收、视锥剔除、视差背景 |

运行：

```bash
python demo.py
```

**验证运行环境**（自动跑完 10 项断言并输出结果，无需手动操作）：

```bash
GZX_DEMO_SELFTEST=1 python demo.py        # Windows CMD: set GZX_DEMO_SELFTEST=1
```

自检覆盖重力着地、水平移动、跳跃、粒子发射、运行期生成刚体、金币拾取、受伤状态机、粒子风暴压力测试、批次提交、音频子系统，退出码 0 表示全部通过。

---

## 注意事项

- **主线程限制**：`Engine` 必须在主线程创建和驱动，窗口与 Vulkan 上下文不支持跨线程。
- **字符串编码**：传给引擎的字符串自动按 UTF-8 编码，源码文件保存为 UTF-8 即可。
- **句柄类型**：刚体与声部句柄是普通 `int`（引擎内部为 64 位，Python 整数天然兼容）。
- **每帧生命周期**：绘制指令只在当前帧有效，`begin_frame()` 后提交的才会在 `end_frame()` 渲染；相机、背景色、排序模式也需每帧设置。
- **性能要点**：Python 解释器比 JS/原生慢，精灵数量大时尤其要把背景、世界、UI 全部塞进**同一个** `SpriteBatch`，每帧只调一次 `draw_sprites()`；纯色矩形用 `batch.add(WHITE_TEXTURE, ...)` 而非逐个 `draw_rect()`。`demo.py` 的粒子上限默认 12000（`MAX_PARTICLES` 常量），约 8000 粒子时帧时间 30ms；需要更高吞吐请改用 Node.js 绑定或 C++。
- **资源释放**：`engine.close()`、`world.close()`、`font.close()` 用完后调用，或依赖 `frames()` / `with` 语句自动清理（对象析构时也会兜底释放）。
