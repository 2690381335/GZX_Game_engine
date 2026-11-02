"""GZX 引擎 Python 绑定 —— 全功能演示

运行： python demo.py

与 C++ 版 examples/main.cpp 及 Node.js 版 demo.js 效果一致：
平台跳跃 + 物理沙盒 + 粒子压力测试。涵盖：程序化纹理与音效生成、
四套角色动画、物理碰撞与接触事件、金币拾取、陷阱受伤、激光推动、
粒子爆发与风暴、视差背景、相机跟随与震动、帧时间曲线、操作说明面板。

自检模式（无人工操作，自动验证环境）： GZX_DEMO_SELFTEST=1 python demo.py
"""

from __future__ import annotations

import math
import os
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from gzx import (BodyType, Color, Engine, Font, GzxError, Key, Mouse, Shape, Sort,
                 Space, SpriteBatch, Uv, WHITE, WHITE_TEXTURE, World)

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------
PI = math.pi
GRAVITY = 1400.0            # 世界重力加速度（像素/秒²）
GROUND_TOP = 600.0          # 地面顶部的世界 Y 坐标
LEVEL_LEFT = -400.0         # 关卡左边界
LEVEL_RIGHT = 2600.0        # 关卡右边界
TILE = 32.0                 # 地形贴图的平铺尺寸
PLAYER_HALF_W = 14.0        # 玩家碰撞盒半宽
PLAYER_HALF_H = 34.0        # 玩家碰撞盒半高
PLAYER_SPEED = 280.0        # 玩家水平移动速度
JUMP_SPEED = 640.0          # 玩家起跳速度
SPRITE_SCALE = 2.0          # 角色精灵表绘制缩放（单帧 96x96）
MAX_PARTICLES = 12000       # 粒子上限（Python 循环比 JS 慢，按性能调整）
MAX_PROPS = 800             # 可交互物体上限
SAMPLE_RATE = 48000         # 程序化音效采样率

BIT_WORLD = 1 << 0          # 碰撞类别：地形
BIT_PLAYER = 1 << 1         # 碰撞类别：玩家
BIT_PROP = 1 << 2           # 碰撞类别：可交互物体
BIT_PICKUP = 1 << 3         # 碰撞类别：拾取物
BIT_HAZARD = 1 << 4         # 碰撞类别：陷阱

LAYER_SKY, LAYER_STARS, LAYER_FAR, LAYER_NEAR = -100, -99, -98, -97
LAYER_DECOR, LAYER_WORLD, LAYER_FX, LAYER_LASER = -1, 0, 5, 8
LAYER_PLAYER, LAYER_DEBUG, LAYER_PANEL, LAYER_TEXT, LAYER_CURSOR = 10, 50, 100, 101, 200

"""刚体的游戏语义，存放在刚体的 user_data 中"""


class Kind:
    NONE, GROUND, PLATFORM, MOVER, CRATE, BALL, COIN, SPIKE, PLAYER = range(9)


# ---------------------------------------------------------------------------
# 工具
# ---------------------------------------------------------------------------
_MASK32 = 0xFFFFFFFF


class Random:
    """xorshift32 伪随机数生成器，比 random 模块更快且可复现。"""

    __slots__ = ("state",)

    def __init__(self, seed: int) -> None:
        self.state = seed & _MASK32 or 1

    def next_float(self) -> float:
        s = self.state
        s ^= (s << 13) & _MASK32
        s ^= s >> 17
        s ^= (s << 5) & _MASK32
        self.state = s & _MASK32
        return ((s >> 8) & 0xFFFFFF) / 16777216.0

    def range(self, low: float, high: float) -> float:
        return low + (high - low) * self.next_float()


def clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def clamp01(v: float) -> float:
    return clamp(v, 0.0, 1.0)


def hsv(hue: float, saturation: float, value: float, alpha: float = 1.0) -> Color:
    def channel(n: float) -> float:
        k = (n + hue * 6) % 6
        return value - value * saturation * clamp01(min(k, 4 - k))

    return Color(channel(5), channel(3), channel(1), alpha)


def hash_noise(x: int, y: int) -> float:
    """整数坐标哈希噪声，用于程序化纹理的颗粒细节。"""
    h = (x * 374761393 + y * 668265263) & _MASK32
    h = ((h ^ (h >> 13)) * 1274126177) & _MASK32
    return ((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.0


def midi(note: float) -> float:
    """MIDI 音符编号转频率，69 为 A4。"""
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def sweep_phase(t: float, duration: float, low: float, high: float) -> float:
    """线性扫频正弦波在时刻 t 的相位，对频率积分避免相位跳变。"""
    return 2 * PI * (low * t + (high - low) * t * t / (2 * duration))


# ---------------------------------------------------------------------------
# 资源路径
# ---------------------------------------------------------------------------
def find_assets() -> Path:
    for candidate in [
        Path(__file__).resolve().parent.parent.parent / "samples" / "assets" / "Sprites",
        Path(__file__).resolve().parent.parent / "Sprites",
        Path(__file__).resolve().parent / "Sprites",
    ]:
        if (candidate / "IDLE.png").is_file():
            return candidate
    raise FileNotFoundError("找不到贴图目录，请在 SDK 的 bindings/python 下运行")


ASSETS = find_assets()

FONT_CANDIDATES = (
    ["C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/simhei.ttf",
     "C:/Windows/Fonts/simsun.ttc", "C:/Windows/Fonts/msyhbd.ttc"]
    if os.name == "nt" else
    ["/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
     "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
     "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf"]
)

# ---------------------------------------------------------------------------
# 程序化资源生成
# ---------------------------------------------------------------------------
Shader = Callable[[float, float], Color]


def bake(engine: Engine, width: int, height: int, shader: Shader) -> int:
    """按像素着色函数生成 RGBA8 纹理，演示 create_texture 接口。"""
    rgba = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            c = shader((x + 0.5) / width, (y + 0.5) / height)
            offset = (y * width + x) * 4
            rgba[offset] = int(clamp01(c[0]) * 255 + 0.5)
            rgba[offset + 1] = int(clamp01(c[1]) * 255 + 0.5)
            rgba[offset + 2] = int(clamp01(c[2]) * 255 + 0.5)
            rgba[offset + 3] = int(clamp01(c[3]) * 255 + 0.5)
    return engine.create_texture(width, height, rgba)


def synth(engine: Engine, seconds: float, wave: Callable[[float], float]) -> int:
    """按波形函数合成单声道音效，演示 create_sound 接口。"""
    count = int(seconds * SAMPLE_RATE)
    samples = [wave(i / SAMPLE_RATE) for i in range(count)]
    return engine.create_sound(samples, 1, SAMPLE_RATE)


# ---------------------------------------------------------------------------
# 精灵表动画
# ---------------------------------------------------------------------------
class Animation:
    """网格精灵表动画播放器。"""

    __slots__ = ("columns", "rows", "frames", "fps", "loop", "frame", "time")

    def __init__(self, columns: int, rows: int, frames: int, fps: float, loop: bool = True) -> None:
        self.columns, self.rows, self.frames = columns, rows, frames
        self.fps, self.loop = fps, loop
        self.frame, self.time = 0, 0.0

    def reset(self) -> None:
        self.frame, self.time = 0, 0.0

    def finished(self) -> bool:
        return not self.loop and self.frame >= self.frames - 1 and self.time >= 1 / self.fps

    def update(self, dt: float) -> None:
        self.time += dt
        step = 1 / self.fps
        while self.time >= step:
            self.time -= step
            if self.frame + 1 < self.frames:
                self.frame += 1
            elif self.loop:
                self.frame = 0
            else:
                break

    def uv(self) -> Uv:
        return Engine.sheet_uv(self.columns, self.rows, self.frame)


# ---------------------------------------------------------------------------
# 界面文本（有中文字体时显示中文，否则回退到像素字体英文）
# ---------------------------------------------------------------------------
T = {
    "title": ("GZX 引擎演示", "GZX ENGINE DEMO"),
    "fps": ("帧率 {fps}   帧时间 {ms} 毫秒", "FPS {fps}   FRAME {ms} MS"),
    "stats": ("精灵 {sprites}   刚体 {bodies}   接触 {contacts}   粒子 {particles}",
              "SPRITES {sprites}  BODIES {bodies}  CONTACTS {contacts}  PARTICLES {particles}"),
    "score": ("金币 {coins} / {total}", "COINS {coins} / {total}"),
    "audio_off": ("音频设备不可用", "AUDIO UNAVAILABLE"),
    "picked": ("拾取 第 {index} 个精灵", "PICK #{index}"),
    "help_title": ("操作说明（按 H 隐藏）", "CONTROLS (H TO HIDE)"),
    "help_hidden": ("按 H 显示操作说明", "PRESS H FOR CONTROLS"),
    "on": ("开启", "ON"),
    "off": ("关闭", "OFF"),
}
SORT_NAMES = [("不排序", "NONE"), ("按层级", "LAYER"), ("层级+纹理", "LAYER+TEXTURE")]

CONTROLS = [
    ("A / D", "左右移动", None),
    ("W / 空格", "跳跃", None),
    ("J", "攻击", None),
    ("鼠标左键", "生成木箱", None),
    ("Shift + 左键", "生成小球", None),
    ("按住右键", "激光推动物体", None),
    ("鼠标滚轮", "缩放镜头", None),
    ("P", "粒子风暴", "storm"),
    ("F1", "切换排序模式", "sort"),
    ("F2", "显示碰撞框", "debug"),
    ("Q / E", "音量 减 / 加", "volume"),
    ("M", "静音", "mute"),
    ("R", "重置关卡", None),
    ("Esc", "退出", None),
]

# ---------------------------------------------------------------------------
# 游戏主体
# ---------------------------------------------------------------------------
SELFTEST = os.environ.get("GZX_DEMO_SELFTEST") == "1"

engine = Engine("GZX Engine - Python 全功能演示", 1280, 720, True)
random = Random(0x9E3779B9)
batch = SpriteBatch(8192)


def rect(x: float, y: float, w: float, h: float, color: Color, space: int, layer: int) -> None:
    """把纯色矩形塞进精灵批次，避免逐矩形的跨语言调用开销。"""
    batch.add(WHITE_TEXTURE, x, y, w, h, color=color, space=space, layer=layer)


# --- 资源 ---
tex = {
    "idle": engine.load_texture(ASSETS / "IDLE.png"),
    "run": engine.load_texture(ASSETS / "RUN.png"),
    "hurt": engine.load_texture(ASSETS / "HURT.png"),
    "logo": engine.load_texture(ASSETS / "GZX ENGINE_LOGO.png"),
    "attack": 0,   # 首次攻击时才加载，演示运行期增量上传
}

# 程序化纹理
tex["disc"] = bake(engine, 64, 64, lambda u, v: Color(*[1 - 0.35 * math.hypot(u - 0.35, v - 0.35)] * 3,
                                                       clamp01((1 - math.hypot(u - 0.5, v - 0.5) * 2) * 32)))
tex["glow"] = bake(engine, 64, 64, lambda u, v: Color(1, 1, 1,
                                                      max(0.0, 1 - math.hypot(u - 0.5, v - 0.5) * 2) ** 2))


def _crate_shader(u: float, v: float) -> Color:
    px, py = u * 32, v * 32
    border = px < 3 or px > 29 or py < 3 or py > 29
    brace = abs(px - py) < 2.5 or abs(px - (32 - py)) < 2.5
    grain = 0.9 + 0.1 * hash_noise(int(px), int(py / 4))
    if border or brace:
        return Color(0.45 * grain, 0.27 * grain, 0.12 * grain, 1)
    return Color(0.78 * grain, 0.55 * grain, 0.30 * grain, 1)


tex["crate"] = bake(engine, 32, 32, _crate_shader)


def _ground_shader(u: float, v: float) -> Color:
    x, y = int(u * 32), int(v * 32)
    grass_edge = 6 + 3 * hash_noise(x, 7)
    noise = hash_noise(x, y)
    if v * 32 < grass_edge:
        return Color(0.30 + 0.1 * noise, 0.72 + 0.1 * noise, 0.32, 1)
    speck = 0.15 if noise > 0.92 else 0.0
    return Color(0.40 + speck, 0.26 + speck, 0.18 + speck, 1)


tex["ground"] = bake(engine, 32, 32, _ground_shader)


def _spike_shader(u: float, v: float) -> Color:
    local = (u * 2) % 1                       # 每块贴图两根尖刺
    edge = abs(local - 0.5) * 2
    if v < edge:
        return Color(0, 0, 0, 0)
    metal = 0.55 + 0.4 * (1 - local)
    return Color(metal, metal, metal + 0.08, 1)


tex["spike"] = bake(engine, 32, 32, _spike_shader)


def _sky_shader(u: float, v: float) -> Color:
    top, middle, bottom = (0.04, 0.05, 0.14), (0.20, 0.12, 0.32), (0.62, 0.32, 0.38)

    def mix(a, b, t):
        return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))

    c = mix(top, middle, v / 0.6) if v < 0.6 else mix(middle, bottom, (v - 0.6) / 0.4)
    return Color(*c, 1)


tex["sky"] = bake(engine, 1, 256, _sky_shader)


def _hills_shader(u: float, v: float) -> Color:
    angle = u * 2 * PI                        # 整数倍频率保证左右无缝平铺
    height = 0.55 + 0.18 * math.sin(angle * 2) + 0.10 * math.sin(angle * 5 + 1.3) + 0.05 * math.sin(angle * 11 + 0.7)
    return Color(1, 1, 1, clamp01((v - (1 - height)) * 128))


tex["hills"] = bake(engine, 512, 128, _hills_shader)

# --- 字体 ---
pixel_font = engine.default_font()
ttf_font: Optional[Font] = None


def tr(pair) -> str:
    return pair[0] if ttf_font else pair[1]


# 界面用到的全部中文字符，用于烘焙字体图集
_chars_src = "".join(pair[0] for pair in T.values()) + "".join(name[0] for name in SORT_NAMES)
CHARS = _chars_src + "".join(key + action for key, action, _ in CONTROLS)

for candidate in FONT_CANDIDATES:
    if not Path(candidate).is_file():
        continue
    try:
        ttf_font = engine.ttf_font(candidate, 18, CHARS)
        print(f"中文字体: {candidate}")
        break
    except (GzxError, OSError) as error:
        print(f"字体 {candidate} 不可用: {error}", file=sys.stderr)

# --- 音效 ---
audio = {"jump": 0, "hit": 0, "coin": 0, "hurt": 0, "spawn": 0, "music": 0}
if engine.audio_available:
    audio["jump"] = synth(engine, 0.16, lambda t: 0.35 * (1 - t / 0.16) ** 2
                          * math.sin(sweep_phase(t, 0.16, 320, 760)))

    _hit_noise = Random(1234)

    def _hit(t: float) -> float:
        env = 1 - t / 0.14
        return 0.45 * env ** 3 * _hit_noise.range(-1, 1) + 0.5 * env * math.sin(2 * PI * 90 * t)

    audio["hit"] = synth(engine, 0.14, _hit)

    def _coin(t: float) -> float:
        freq = 988 if t < 0.07 else 1319      # B5 -> E6 经典拾取音
        phase = 2 * PI * freq * t
        return 0.25 * (1 - t / 0.22) * (math.sin(phase) + math.sin(phase * 3) / 3)

    audio["coin"] = synth(engine, 0.22, _coin)

    def _hurt(t: float) -> float:
        cycles = sweep_phase(t, 0.3, 520, 110) / (2 * PI)
        return 0.25 * (1 - t / 0.3) * (2 * (cycles - math.floor(cycles)) - 1)   # 下行锯齿波

    audio["hurt"] = synth(engine, 0.3, _hurt)
    audio["spawn"] = synth(engine, 0.06, lambda t: 0.3 * (1 - t / 0.06) * math.sin(2 * PI * 220 * t))

    MELODY = [57, 60, 64, 67, 69, 67, 64, 60, 53, 57, 60, 64, 65, 64, 60, 57,
              48, 52, 55, 60, 64, 60, 55, 52, 55, 59, 62, 67, 71, 67, 62, 59]  # Am-F-C-G 琶音
    BASS = [45, 41, 36, 43]

    def _music(t: float) -> float:
        step = int(t / 0.25) % 32
        local = t - step * 0.25
        lead = 2 / PI * math.asin(math.sin(2 * PI * midi(MELODY[step]) * local))   # 三角波
        chord = int(t / 2) % 4
        bass_time = t - chord * 2
        return (0.16 * lead * math.exp(-local * 5)
                + 0.22 * math.sin(2 * PI * midi(BASS[chord]) * bass_time) * math.exp(-bass_time * 1.2))

    audio["music"] = synth(engine, 8.0, _music)
    engine.play(audio["music"], 0.3, 0.0, True)

volume, muted = 0.8, False
engine.set_master_volume(volume)

# --- 星空 ---
stars = [(random.next_float(), random.next_float() * 0.65,
          random.range(1, 3), random.range(0, 2 * PI), random.range(1, 4)) for _ in range(180)]

# --- 物理世界与关卡 ---
world: Optional[World] = None
bodies: list[dict] = []
player = 0
mover = 0
prop_count = 0
coins = coins_total = 0

anim = {
    "idle": Animation(10, 1, 10, 10),
    "run": Animation(16, 1, 16, 18),
    "attack": Animation(7, 1, 7, 20, False),
    "hurt": Animation(4, 1, 4, 10, False),
}
facing_left = False
running = False
grounded = False
attacking = False
attack_hit_done = False
hurting = False
attack_timer = 0.0
hurt_timer = 0.0

particles: list[dict] = []
sort_mode = 1
debug = False
storm = False
show_help = True

# 相机
camera_x = camera_y = 0.0
camera_zoom = target_zoom = 1.5
shake = 0.0
center_x, center_y = 0.0, GROUND_TOP - 120
view_w, view_h = 1280.0, 720.0
view_left = view_top = view_right = view_bottom = 0.0
mouse_screen = (0.0, 0.0)
mouse_world = (0.0, 0.0)
laser_active = laser_hit = False
laser_from = (0.0, 0.0)
laser_to = (0.0, 0.0)
picked_info: Optional[dict] = None

elapsed = 0.0
frame_ms = 16.7
HISTORY = 120
history = [0.0] * HISTORY
history_index = 0
_last_draw = time.perf_counter()


def add_body(kind: int, tint: Color, **definition) -> int:
    handle = world.create_body(user_data=kind, **definition)
    round_shape = definition.get("shape") == Shape.CIRCLE
    bodies.append({
        "handle": handle, "kind": kind,
        "half_width": definition.get("radius", 0) if round_shape else definition.get("half_width", 0),
        "half_height": definition.get("radius", 0) if round_shape else definition.get("half_height", 0),
        "tint": tint,
    })
    return handle


def add_solid(kind: int, x: float, y: float, half_width: float, half_height: float, tint: Color) -> None:
    add_body(kind, tint, type=BodyType.STATIC, shape=Shape.BOX, x=x, y=y,
             half_width=half_width, half_height=half_height, category=BIT_WORLD, friction=0.6)


def spawn_prop(ball: bool, x: float, y: float) -> None:
    global prop_count
    if prop_count >= MAX_PROPS:
        return
    if ball:
        add_body(Kind.BALL, hsv(random.next_float(), 0.65, 1), type=BodyType.DYNAMIC, shape=Shape.CIRCLE,
                 x=x, y=y, radius=14, mass=0.6, restitution=0.6, friction=0.2,
                 category=BIT_PROP, mask=BIT_WORLD | BIT_PROP | BIT_PLAYER)
    else:
        add_body(Kind.CRATE, Color(1, 1, 1, 1), type=BodyType.DYNAMIC, shape=Shape.BOX,
                 x=x, y=y, half_width=20, half_height=20, mass=1, restitution=0.05, friction=0.5,
                 category=BIT_PROP, mask=BIT_WORLD | BIT_PROP | BIT_PLAYER)
    prop_count += 1


def add_coin(x: float, y: float) -> None:
    global coins_total
    add_body(Kind.COIN, Color(1, 0.82, 0.25, 1), type=BodyType.STATIC, shape=Shape.CIRCLE,
             x=x, y=y, radius=10, sensor=True, category=BIT_PICKUP, mask=BIT_PLAYER)
    coins_total += 1


def build_level() -> None:
    global world, bodies, particles, prop_count, coins, coins_total
    global player, mover, attacking, hurting, attack_timer, hurt_timer, center_x, center_y
    world = World(0.0, GRAVITY, 64.0)
    bodies = []
    particles = []
    prop_count = 0
    coins = coins_total = 0

    grass = Color(1, 1, 1, 1)
    add_solid(Kind.GROUND, (LEVEL_LEFT + LEVEL_RIGHT) * 0.5, GROUND_TOP + 200,
              (LEVEL_RIGHT - LEVEL_LEFT) * 0.5, 200, grass)
    add_solid(Kind.GROUND, LEVEL_LEFT - 20, GROUND_TOP - 400, 20, 600, grass)
    add_solid(Kind.GROUND, LEVEL_RIGHT + 20, GROUND_TOP - 400, 20, 600, grass)

    for x, top, width in [(520, 470, 192), (840, 370, 160), (1180, 470, 192), (1560, 350, 224), (2100, 300, 160)]:
        add_solid(Kind.PLATFORM, x, top + 12, width * 0.5, 12, grass)
        for c in (-1, 0, 1):
            add_coin(x + c * 40, top - 30)
    for x in range(-200, 400, 60):
        add_coin(float(x), GROUND_TOP - 30)

    mover = add_body(Kind.MOVER, Color(0.55, 0.8, 1, 1), type=BodyType.KINEMATIC, shape=Shape.BOX,
                     x=1840, y=440, half_width=64, half_height=10, category=BIT_WORLD, friction=0.8)

    add_body(Kind.SPIKE, Color(1, 1, 1, 1), type=BodyType.STATIC, shape=Shape.BOX,
             x=1350, y=GROUND_TOP - 12, half_width=64, half_height=12,
             sensor=True, category=BIT_HAZARD, mask=BIT_PLAYER)

    for row in range(4):                      # 木箱金字塔
        for col in range(4 - row):
            spawn_prop(False, 620 + row * 21 + col * 42, GROUND_TOP - 20.5 - row * 40.5)
    for b in range(5):
        spawn_prop(True, 950 + b * 34, GROUND_TOP - 200)

    player = add_body(Kind.PLAYER, Color(1, 1, 1, 1), type=BodyType.DYNAMIC, shape=Shape.BOX,
                      x=0, y=GROUND_TOP - PLAYER_HALF_H - 1,
                      half_width=PLAYER_HALF_W, half_height=PLAYER_HALF_H,
                      friction=0, category=BIT_PLAYER, mask=BIT_WORLD | BIT_PROP | BIT_PICKUP | BIT_HAZARD)

    attacking = hurting = False
    attack_timer = hurt_timer = 0.0
    center_x, center_y = 0.0, GROUND_TOP - 120


build_level()

# ---------------------------------------------------------------------------
# 逻辑
# ---------------------------------------------------------------------------
def pan_at(x: float) -> float:
    """根据世界 X 坐标计算立体声声像。"""
    return clamp((x - center_x) / (view_w * 0.5 / camera_zoom), -1, 1)


def start_attack() -> None:
    global attacking, attack_hit_done, attack_timer
    if not tex["attack"]:
        tex["attack"] = engine.load_texture(ASSETS / "ATTACK 1.png")
    attacking = True
    attack_hit_done = False
    attack_timer = 0.0
    anim["attack"].reset()


def attack_hit() -> None:
    global attack_hit_done, shake
    attack_hit_done = True
    pos = world.position(player)
    direction = -1 if facing_left else 1
    near_x, far_x = pos[0], pos[0] + direction * 90
    hits = world.query_rect(min(near_x, far_x), pos[1] - 40, max(near_x, far_x), pos[1] + 30, BIT_PROP)
    for body in hits:
        world.apply_impulse(body, direction * 420, -320)
        p = world.position(body)
        emit_burst(p[0], p[1], 40, 360, Color(1, 0.75, 0.35, 1), 0.6)
    if hits:
        if engine.audio_available:
            engine.play(audio["hit"], 1.0, pan_at(pos[0]))
        shake = 0.35


def hurt() -> None:
    global hurt_timer, hurting, attacking, shake
    if hurt_timer > 0:
        return
    hurt_timer, hurting, attacking = 1.2, True, False
    anim["hurt"].reset()
    world.set_velocity(player, 260 if facing_left else -260, -480)
    pos = world.position(player)
    emit_burst(pos[0], pos[1], 40, 300, Color(1, 0.25, 0.25, 1), 0.6)
    if engine.audio_available:
        engine.play(audio["hurt"], 0.9, pan_at(pos[0]))
    shake = 0.6


def emit_burst(x: float, y: float, count: int, speed: float, color: Color, life: float) -> None:
    for _ in range(count):
        if len(particles) >= MAX_PARTICLES:
            return
        angle = random.range(0, 2 * PI)
        magnitude = random.range(0.2, 1) * speed
        span = life * random.range(0.6, 1)
        particles.append({"x": x, "y": y, "vx": math.cos(angle) * magnitude, "vy": math.sin(angle) * magnitude,
                          "life": span, "span": span, "size": random.range(4, 9), "color": color})


def update_player(dt: float, manual: bool = False) -> None:
    """manual 为 True 时不接管水平速度，供自检脚本直接操控刚体。"""
    global grounded, hurt_timer, hurting, facing_left, running, attacking, attack_hit_done, attack_timer
    pos = world.position(player)
    vel = list(world.velocity(player))
    feet = pos[1] + PLAYER_HALF_H

    grounded = False
    for offset in (-PLAYER_HALF_W + 2, PLAYER_HALF_W - 2):
        if world.raycast(pos[0] + offset, feet - 2, pos[0] + offset, feet + 4, BIT_WORLD | BIT_PROP):
            grounded = True
            break       # 两侧脚底向下射线检测着地

    hurt_timer = max(0.0, hurt_timer - dt)
    if hurting:
        anim["hurt"].update(dt)
        hurting = not anim["hurt"].finished()

    if not hurting and not manual:
        left = engine.key_down(Key.A) or engine.key_down(Key.LEFT)
        right = engine.key_down(Key.D) or engine.key_down(Key.RIGHT)
        direction = (1 if right else 0) - (1 if left else 0)
        vel[0] = direction * PLAYER_SPEED
        if direction != 0:
            facing_left = direction < 0

        if (direction != 0) != running:
            running = direction != 0
            (anim["run"] if running else anim["idle"]).reset()

        if grounded and (engine.key_pressed(Key.W) or engine.key_pressed(Key.UP) or engine.key_pressed(Key.SPACE)):
            vel[1] = -JUMP_SPEED
            if engine.audio_available:
                engine.play(audio["jump"], 0.8, pan_at(pos[0]))
        world.set_velocity(player, vel[0], vel[1])

        if not attacking and engine.key_pressed(Key.J):
            start_attack()

    if attacking:
        anim["attack"].update(dt)
        attack_timer += dt
        if not attack_hit_done and attack_timer >= 0.12:     # 挥刀到第 3 帧左右时判定命中
            attack_hit()
        if anim["attack"].finished():
            attacking = False
    (anim["run"] if running else anim["idle"]).update(dt)

    if pos[1] > GROUND_TOP + 1000:                           # 掉出世界后复活
        world.set_position(player, 0, GROUND_TOP - PLAYER_HALF_H - 1)
        world.set_velocity(player, 0, 0)


def update_sandbox() -> None:
    global laser_active, laser_hit, laser_from, laser_to
    if engine.mouse_pressed(Mouse.LEFT):
        ball = engine.key_down(Key.LEFT_SHIFT) or engine.key_down(Key.RIGHT_SHIFT)
        spawn_prop(ball, mouse_world[0], mouse_world[1])
        if engine.audio_available:
            engine.play(audio["spawn"], 0.6, pan_at(mouse_world[0]))

    laser_active = engine.mouse_down(Mouse.RIGHT)
    if laser_active:
        origin = world.position(player)
        laser_from = (origin[0], origin[1] - 10)
        dx, dy = mouse_world[0] - laser_from[0], mouse_world[1] - laser_from[1]
        length = max(math.hypot(dx, dy), 1e-3)
        end = (laser_from[0] + dx / length * 900, laser_from[1] + dy / length * 900)
        hit = world.raycast(laser_from[0], laser_from[1], end[0], end[1], BIT_WORLD | BIT_PROP)
        laser_to = (hit.x, hit.y) if hit else end
        laser_hit = hit is not None
        if hit:
            kind = world.user_data(hit.body)
            if kind in (Kind.CRATE, Kind.BALL):
                world.apply_impulse(hit.body, dx / length * 30, dy / length * 30)
            emit_burst(hit.x, hit.y, 3, 220, Color(1, 0.3, 0.25, 1), 0.35)

    if storm:   # 每帧补充粒子，稳定在数千个精灵，用于压力测试
        width = view_w / camera_zoom
        for _ in range(400):
            if len(particles) >= MAX_PARTICLES:
                break
            span = random.range(2, 3)
            particles.append({
                "x": camera_x + random.next_float() * width, "y": camera_y - 20,
                "vx": random.range(-30, 30), "vy": random.range(100, 300),
                "life": span, "span": span, "size": random.range(3, 7),
                "color": hsv(random.range(0.5, 0.65), 0.6, 1, 0.8),
            })


def handle_contacts() -> None:
    global coins, bodies
    pending = []
    for contact in world.contacts():
        if not world.valid(contact.a) or not world.valid(contact.b):
            continue
        kind_a, kind_b = world.user_data(contact.a), world.user_data(contact.b)
        if kind_a != Kind.PLAYER and kind_b != Kind.PLAYER:
            continue
        other = contact.b if kind_a == Kind.PLAYER else contact.a
        kind = kind_b if kind_a == Kind.PLAYER else kind_a
        if kind == Kind.COIN:
            pending.append(other)
        if kind == Kind.SPIKE:
            hurt()
    for coin in pending:
        if not world.valid(coin):    # 同一金币可能在多个子步中重复上报
            continue
        p = world.position(coin)
        world.destroy_body(coin)
        coins += 1
        emit_burst(p[0], p[1], 30, 260, Color(1, 0.85, 0.3, 1), 0.7)
        if engine.audio_available:
            engine.play(audio["coin"], 0.8, pan_at(p[0]))
    if pending:
        bodies = [b for b in bodies if world.valid(b["handle"])]


def update_particles(dt: float) -> None:
    write = 0
    for p in particles:                       # 原地压缩存活粒子，避免逐个 pop 的开销
        p["life"] -= dt
        if p["life"] <= 0:
            continue
        p["vy"] += 900 * dt
        p["x"] += p["vx"] * dt
        p["y"] += p["vy"] * dt
        if p["y"] > GROUND_TOP and p["vy"] > 0:
            p["y"] = GROUND_TOP
            p["vy"] *= -0.3                  # 落地轻微反弹
        particles[write] = p
        write += 1
    del particles[write:]


def update_camera(dt: float) -> None:
    global camera_x, camera_y, camera_zoom, center_x, center_y, shake
    blend = 1 - math.exp(-dt * 6)             # 与帧率无关的指数平滑
    target = world.position(player)
    center_x += (target[0] - center_x) * blend
    center_y += (min(target[1] - 60, GROUND_TOP - 120) - center_y) * blend
    camera_zoom += (target_zoom - camera_zoom) * blend
    camera_x = center_x - view_w * 0.5 / camera_zoom
    camera_y = center_y - view_h * 0.5 / camera_zoom
    shake = max(0.0, shake - dt)


def handle_toggles() -> None:
    global sort_mode, debug, storm, show_help, volume, muted
    if engine.key_pressed(Key.R):
        build_level()
    if engine.key_pressed(Key.F1):
        sort_mode = (sort_mode + 1) % 3
    if engine.key_pressed(Key.F2):
        debug = not debug
    if engine.key_pressed(Key.P):
        storm = not storm
    if engine.key_pressed(Key.H):
        show_help = not show_help

    previous = volume
    if engine.key_pressed(Key.Q):
        volume = max(0.0, volume - 0.1)
    if engine.key_pressed(Key.E):
        volume = min(1.0, volume + 0.1)
    if engine.key_pressed(Key.M):
        muted = not muted
    if previous != volume or engine.key_pressed(Key.M):
        engine.set_master_volume(0.0 if muted else volume)


# ---------------------------------------------------------------------------
# 绘制
# ---------------------------------------------------------------------------
def visible(l: float, t: float, r: float, b: float) -> bool:
    return r >= view_left and l <= view_right and b >= view_top and t <= view_bottom


def draw_tiled(texture: int, x: float, y: float, width: float, height: float, tint: Color, layer: int) -> None:
    """在世界空间水平平铺纹理，只提交可见的贴图块。"""
    right = x + width
    left = x
    if view_left > left:
        left += math.floor((view_left - left) / TILE) * TILE   # 跳过左侧不可见的贴图块
    while left < right and left <= view_right:
        tile = min(TILE, right - left)
        batch.add(texture, left, y, tile, height, uv=(0.0, 0.0, tile / TILE, 1.0),
                  color=tint, layer=layer)
        left += TILE


def draw_background() -> None:
    batch.add(tex["sky"], 0, 0, view_w, view_h, space=Space.SCREEN, layer=LAYER_SKY)

    for star_x, star_y, star_size, phase, speed in stars:
        alpha = 0.725 + 0.275 * math.sin(elapsed * speed + phase)
        batch.add(tex["disc"], star_x * view_w, star_y * view_h, star_size, star_size,
                  color=Color(1, 0.95, 0.85, alpha), space=Space.SCREEN, layer=LAYER_STARS)

    def hills(parallax: float, height_ratio: float, tint: Color, layer: int) -> None:
        width, height = 640.0, view_h * height_ratio
        offset = -((camera_x * camera_zoom * parallax) % width)     # 视差系数越小越远
        if offset > 0:
            offset -= width
        x = offset
        while x < view_w:
            batch.add(tex["hills"], x, view_h - height, width + 1, height,
                      color=tint, space=Space.SCREEN, layer=layer)
            x += width

    hills(0.15, 0.55, Color(0.22, 0.16, 0.36, 1), LAYER_FAR)
    hills(0.4, 0.4, Color(0.13, 0.10, 0.24, 1), LAYER_NEAR)


def draw_world() -> None:
    hub_x, hub_y = 1000, 420
    if visible(hub_x - 120, hub_y - 120, hub_x + 120, GROUND_TOP):
        rect(hub_x - 8, hub_y, 16, GROUND_TOP - hub_y, Color(0.25, 0.2, 0.3, 1), Space.WORLD, LAYER_DECOR)
        for blade in range(4):                 # 绕叶片根部旋转的风车
            batch.add(WHITE_TEXTURE, hub_x, hub_y - 7, 110, 14,
                      color=Color(0.85, 0.8, 0.9, 1), rotation=elapsed * 1.2 + blade * PI * 0.5,
                      pivot_x=0.0, pivot_y=0.5, layer=LAYER_DECOR)
        batch.add(tex["disc"], hub_x - 10, hub_y - 10, 20, 20,
                  color=Color(0.4, 0.35, 0.45, 1), layer=LAYER_DECOR)

    for body in bodies:
        if body["kind"] == Kind.PLAYER:
            continue
        cx, cy = world.position(body["handle"])
        left, top = cx - body["half_width"], cy - body["half_height"]
        width, height = body["half_width"] * 2, body["half_height"] * 2
        if not visible(left, top, left + width, top + height):
            continue                           # 视锥剔除

        kind = body["kind"]
        if kind in (Kind.GROUND, Kind.PLATFORM, Kind.MOVER):
            grass = min(TILE, height)
            draw_tiled(tex["ground"], left, top, width, grass, body["tint"], LAYER_WORLD)
            if height > grass:
                rect(left, top + grass, width, height - grass, Color(0.30, 0.19, 0.13, 1), Space.WORLD, LAYER_WORLD)
        elif kind == Kind.SPIKE:
            draw_tiled(tex["spike"], left, top, width, height, body["tint"], LAYER_WORLD)
        elif kind == Kind.CRATE:
            batch.add(tex["crate"], left, top, width, height, color=body["tint"], layer=LAYER_WORLD)
        elif kind == Kind.BALL:
            batch.add(tex["disc"], left, top, width, height, color=body["tint"], layer=LAYER_WORLD)
        elif kind == Kind.COIN:
            bob = math.sin(elapsed * 3 + cx * 0.05) * 3
            spin = max(0.15, abs(math.cos(elapsed * 4 + cx)))   # 压缩宽度模拟金币翻转
            batch.add(tex["glow"], cx - 20, cy - 20 + bob, 40, 40,
                      color=Color(1, 0.8, 0.3, 0.35), layer=LAYER_WORLD)
            batch.add(tex["disc"], cx - body["half_width"] * spin, top + bob, width * spin, height,
                      color=body["tint"], layer=LAYER_WORLD)


def draw_particles() -> None:
    for p in particles:
        half = p["size"] * 0.5
        if not visible(p["x"] - half, p["y"] - half, p["x"] + half, p["y"] + half):
            continue
        color = p["color"]
        alpha = color[3] * (p["life"] / p["span"])             # 透明度随剩余寿命衰减
        batch.add(tex["glow"], p["x"] - half, p["y"] - half, p["size"], p["size"],
                  color=Color(color[0], color[1], color[2], alpha), layer=LAYER_FX)


def draw_laser() -> None:
    if not laser_active:
        return
    dx, dy = laser_to[0] - laser_from[0], laser_to[1] - laser_from[1]
    length = math.hypot(dx, dy)
    angle = math.atan2(dy, dx)
    for thickness, color in ((10, Color(1, 0.2, 0.2, 0.3)), (2, Color(1, 0.9, 0.9, 1))):
        batch.add(WHITE_TEXTURE, laser_from[0], laser_from[1] - thickness * 0.5, length, thickness,
                  color=color, rotation=angle, pivot_x=0.0, pivot_y=0.5, layer=LAYER_LASER)
    if laser_hit:
        batch.add(tex["glow"], laser_to[0] - 16, laser_to[1] - 16, 32, 32,
                  color=Color(1, 0.4, 0.3, 0.9), layer=LAYER_LASER)


def draw_player() -> None:
    pos = world.position(player)
    if hurting:
        animation, texture = anim["hurt"], tex["hurt"]
    elif attacking:
        animation, texture = anim["attack"], tex["attack"]
    elif running and grounded:
        animation, texture = anim["run"], tex["run"]
    else:
        animation, texture = anim["idle"], tex["idle"]

    uv = animation.uv()
    if facing_left:                           # 交换 UV 实现水平翻转
        uv = Uv(uv.right, uv.top, uv.left, uv.bottom)
    blink = hurt_timer > 0 and (elapsed % 0.15) < 0.075         # 无敌时间内闪烁
    size = 96 * SPRITE_SCALE

    batch.add(tex["glow"], pos[0] - 28, pos[1] + PLAYER_HALF_H - 6, 56, 12,
              color=Color(0, 0, 0, 0.45), layer=LAYER_PLAYER)   # 脚底阴影
    batch.add(texture, pos[0] - size * 0.5, pos[1] + PLAYER_HALF_H - 80 * SPRITE_SCALE, size, size,
              uv=uv, color=Color(1, 1, 1, 0.35 if blink else 1), layer=LAYER_PLAYER)   # 帧内脚底位于第 80 行


def draw_colliders() -> None:
    for body in bodies:
        cx, cy = world.position(body["handle"])
        kind = body["kind"]
        color = (Color(0.2, 1, 0.4, 0.35) if kind == Kind.PLAYER else
                 Color(1, 0.9, 0.2, 0.3) if kind in (Kind.COIN, Kind.SPIKE) else
                 Color(0.3, 0.7, 1, 0.25))
        rect(cx - body["half_width"], cy - body["half_height"], body["half_width"] * 2, body["half_height"] * 2,
             color, Space.WORLD, LAYER_DEBUG)


def draw_text(text: str, x: float, y: float, color: Color) -> None:
    """带阴影的屏幕空间文字。"""
    font = ttf_font or pixel_font
    scale = 1.0 if ttf_font else 2.0
    engine.draw_text(font, text, x + 1, y + 1, scale, Color(0, 0, 0, 0.7), Space.SCREEN, LAYER_TEXT)
    engine.draw_text(font, text, x, y, scale, color, Space.SCREEN, LAYER_TEXT)


def font_scale() -> float:
    return 1.0 if ttf_font else 2.0


def measure(text: str) -> float:
    return (ttf_font or pixel_font).measure(text, font_scale())[0]


def draw_panel(lines: list[str], x: float, y: float, highlight: bool) -> None:
    """带半透明底板的文字面板。"""
    font = ttf_font or pixel_font
    line_height = font.line_height * font_scale() + 4
    width = max(measure(line) for line in lines)
    height = line_height * len(lines)
    if y < 0:
        y = view_h - height + y

    rect(x - 10, y - 8, width + 20, height + 12, Color(0, 0, 0, 0.45), Space.SCREEN, LAYER_PANEL)
    for i, line in enumerate(lines):
        draw_text(line, x, y + i * line_height,
                  Color(1, 0.85, 0.4, 1) if highlight and i == 0 else Color(0.92, 0.94, 1, 1))


def status_text(status: Optional[str]) -> tuple[str, bool]:
    if status == "storm":
        return tr(T["on"] if storm else T["off"]), storm
    if status == "sort":
        return tr(SORT_NAMES[sort_mode]), True
    if status == "debug":
        return tr(T["on"] if debug else T["off"]), debug
    if status == "volume":
        return f"{round(volume * 100)}%", not muted
    if status == "mute":
        return tr(T["on"] if muted else T["off"]), muted
    return "", False


def draw_hud() -> None:
    lines = [
        tr(T["title"]),
        tr(T["fps"]).format(fps=round(1000 / frame_ms) if frame_ms > 0 else 0, ms=f"{frame_ms:.2f}"),
        tr(T["stats"]).format(sprites=batch.count, bodies=world.body_count,
                              contacts=len(world.contacts()), particles=len(particles)),
        tr(T["score"]).format(coins=coins, total=coins_total),
    ]
    if not engine.audio_available:
        lines.append(tr(T["audio_off"]))
    if picked_info:
        lines.append(tr(T["picked"]).format(index=picked_info["index"]))
    draw_panel(lines, 20, 18, True)

    # 帧时间曲线
    graph_right, graph_bottom = view_w - 20, 90
    rect(graph_right - HISTORY * 2 - 6, 14, HISTORY * 2 + 12, graph_bottom - 8,
         Color(0, 0, 0, 0.45), Space.SCREEN, LAYER_PANEL)
    for i in range(HISTORY):
        ms = history[(history_index + i) % HISTORY]             # 从最旧到最新
        height = min(ms * 2, 66)
        color = (Color(0.3, 0.9, 0.4, 0.9) if ms < 17 else
                 Color(1, 0.8, 0.2, 0.9) if ms < 34 else Color(1, 0.3, 0.3, 0.9))
        rect(graph_right - (HISTORY - i) * 2, graph_bottom - height, 2, height,
             color, Space.SCREEN, LAYER_TEXT)


def draw_controls() -> None:
    scale = font_scale()
    font = ttf_font or pixel_font
    line_height = font.line_height * scale + 6
    panel = Color(0, 0, 0, 0.55)
    title_color = Color(1, 0.85, 0.4, 1)
    top, gap = 110, 18                      # 位于帧时间曲线下方

    if not show_help:
        hint = tr(T["help_hidden"])
        width = measure(hint)
        rect(view_w - width - 40, top - 8, width + 20, line_height + 10, panel, Space.SCREEN, LAYER_PANEL)
        draw_text(hint, view_w - width - 30, top, title_color)
        return

    states = [status_text(status) for _, _, status in CONTROLS]
    key_width = max(measure(tr(key)) for key, _, _ in CONTROLS)
    action_width = max(measure(tr(action)) for _, action, _ in CONTROLS)
    state_width = max((measure(text) for text, _ in states), default=0.0)

    width = max(key_width + gap + action_width + gap + state_width, measure(tr(T["help_title"])))
    height = line_height * (len(CONTROLS) + 1)
    left = view_w - width - 30
    rect(left - 10, top - 8, width + 20, height + 10, panel, Space.SCREEN, LAYER_PANEL)
    draw_text(tr(T["help_title"]), left, top, title_color)

    for i, (key, action, _) in enumerate(CONTROLS):
        y = top + line_height * (i + 1)
        key_x = left + key_width - measure(tr(key))            # 按键列右对齐
        rect(key_x - 4, y - 2, left + key_width - key_x + 8, line_height - 4,
             Color(1, 1, 1, 0.12), Space.SCREEN, LAYER_PANEL)
        draw_text(tr(key), key_x, y, Color(0.55, 0.85, 1, 1))
        draw_text(tr(action), left + key_width + gap, y, Color(0.92, 0.94, 1, 1))
        text, active = states[i]
        if text:
            draw_text(text, left + key_width + gap + action_width + gap, y,
                      Color(0.4, 1, 0.5, 1) if active else Color(0.6, 0.6, 0.65, 1))


def draw_cursor() -> None:
    color = Color(1, 1, 1, 0.85)
    rect(mouse_screen[0] - 8, mouse_screen[1] - 1, 16, 2, color, Space.SCREEN, LAYER_CURSOR)
    rect(mouse_screen[0] - 1, mouse_screen[1] - 8, 2, 16, color, Space.SCREEN, LAYER_CURSOR)


def draw_intro(progress: float) -> None:
    """开场 LOGO（C API 未提供 splash 接口，这里自行绘制）。"""
    w, h = engine.texture_size(tex["logo"])
    scale = min(view_w * 0.6 / w, view_h * 0.6 / h)
    alpha = 1.0 if progress < 0.6 else 1 - (progress - 0.6) / 0.4
    batch.add(tex["sky"], 0, 0, view_w, view_h, space=Space.SCREEN, layer=LAYER_SKY)
    for star_x, star_y, star_size, phase, speed in stars:
        batch.add(tex["disc"], star_x * view_w, star_y * view_h, star_size, star_size,
                  color=Color(1, 0.95, 0.85, 0.725 + 0.275 * math.sin(elapsed * speed + phase)),
                  space=Space.SCREEN, layer=LAYER_STARS)
    batch.add(tex["logo"], (view_w - w * scale) * 0.5, (view_h - h * scale) * 0.5, w * scale, h * scale,
              color=Color(1, 1, 1, clamp01(alpha)), space=Space.SCREEN, layer=LAYER_CURSOR)
    if progress > 0.75:
        hint = "GZX 引擎 Python 绑定演示" if ttf_font else "GZX ENGINE PYTHON BINDING"
        engine.draw_text(ttf_font or pixel_font, hint, (view_w - measure(hint)) * 0.5, view_h * 0.78,
                         font_scale(), Color(1, 1, 1, clamp01(alpha)), Space.SCREEN, LAYER_CURSOR)


# ---------------------------------------------------------------------------
# 自检模式
# ---------------------------------------------------------------------------
test_report: list[str] = []
test_phase = 0
test_timer = 0.0
test_failures = 0
test_start = (0.0, 0.0)
test_bodies = 0
test_coins = 0
test_particles = 0
test_y = 0.0


def test_check(name: str, ok: bool, detail: str = "") -> None:
    global test_failures
    if not ok:
        test_failures += 1
    test_report.append(f"  {'[通过]' if ok else '[失败]'} {name}  {detail}".rstrip())


def run_test_step(dt: float) -> None:
    global test_timer, test_phase, test_start, test_bodies, test_coins, test_particles, test_y, storm
    test_timer += dt
    if test_timer < 0.6:
        return
    test_timer = 0.0

    pos = world.position(player)
    if test_phase == 0:
        test_check("重力与着地检测", grounded, f"玩家 Y={pos[1]:.0f}")
        test_start = pos
        world.set_velocity(player, PLAYER_SPEED, 0)
    elif test_phase == 1:
        test_check("水平移动", pos[0] - test_start[0] > 100, f"位移 {pos[0] - test_start[0]:.0f} 像素")
        test_y = pos[1]
        world.set_velocity(player, 0, -JUMP_SPEED)
    elif test_phase == 2:
        test_check("跳跃上升", pos[1] < test_y, f"Y {test_y:.0f} -> {pos[1]:.0f}")
        test_particles = len(particles)
        emit_burst(pos[0], pos[1], 60, 300, Color(1, 0.8, 0.3, 1), 1.0)
    elif test_phase == 3:
        test_check("粒子发射", len(particles) > test_particles, f"新增 {len(particles) - test_particles} 个")
        test_bodies = world.body_count
        spawn_prop(False, pos[0] + 60, pos[1] - 60)
    elif test_phase == 4:
        test_check("运行期生成刚体", world.body_count > test_bodies, f"刚体 {test_bodies} -> {world.body_count}")
        test_coins = coins
        # 把玩家瞬移到第一枚金币处，验证接触事件与拾取
        coin = next((b for b in bodies if b["kind"] == Kind.COIN and world.valid(b["handle"])), None)
        if coin:
            c = world.position(coin["handle"])
            world.set_position(player, c[0], c[1])
    elif test_phase == 5:
        test_check("金币拾取接触事件", coins > test_coins, f"金币 {test_coins} -> {coins}")
        hurt()
        test_check("受伤状态机", hurting)
    elif test_phase == 6:
        storm = True
    elif test_phase == 7:
        test_check("粒子风暴压力测试", len(particles) > 3000,
                   f"当前粒子 {len(particles)}  帧时间 {frame_ms:.2f} 毫秒")
        test_check("渲染批次提交", batch.count > 1000, f"本帧精灵 {batch.count}")
        test_check("音频子系统", not engine.audio_available or audio["music"] != 0,
                   "已合成 6 个音效" if engine.audio_available else "无音频设备（已跳过）")
        engine.request_quit()
    test_phase += 1


# ---------------------------------------------------------------------------
# 主循环
# ---------------------------------------------------------------------------
INTRO_SECONDS = 1.4
intro_time = 0.0

print(f"贴图目录: {ASSETS}")
if SELFTEST:
    print("自检模式：自动验证物理、拾取、粒子与音频\n")
else:
    print("ESC 退出 / WASD 移动 / J 攻击 / H 帮助 / P 粒子风暴 / F1 排序 / F2 碰撞框")


def frame(dt: float) -> None:
    global elapsed, frame_ms, history_index, intro_time, view_w, view_h
    global mouse_screen, mouse_world, target_zoom, view_left, view_top, view_right, view_bottom
    global _last_draw, picked_info

    # --- 帧时间统计 ---
    now = time.perf_counter()
    ms = (now - _last_draw) * 1000.0
    _last_draw = now
    if 0 < ms < 1000:
        frame_ms += (ms - frame_ms) * 0.05
        history[history_index] = ms
        history_index = (history_index + 1) % HISTORY

    view_w, view_h = engine.viewport

    # --- 开场 ---
    if intro_time < INTRO_SECONDS and not SELFTEST:
        intro_time += dt
        elapsed += dt
        batch.clear()
        draw_intro(intro_time / INTRO_SECONDS)
        engine.draw_sprites(batch)
        if engine.key_pressed(Key.ESCAPE) or engine.key_pressed(Key.SPACE):
            intro_time = INTRO_SECONDS
        return

    elapsed += dt
    world.set_velocity(mover, math.cos(elapsed * 0.8) * 140, 0)   # 运动学平台按速度往返移动

    if SELFTEST:
        run_test_step(dt)         # 自检模式：脚本驱动，不读取窗口输入
        update_player(dt, True)   # 仍更新着地检测与动画，但不接管速度以便脚本操控
        update_sandbox()          # 自检后期会开启粒子风暴，需要走这里生成粒子
        if engine.key_pressed(Key.ESCAPE):
            engine.request_quit()
            return
    else:
        if engine.key_pressed(Key.ESCAPE):
            engine.request_quit()
            return
        handle_toggles()

        mouse_screen = engine.mouse_position
        wheel = engine.mouse_wheel
        if wheel[1] != 0:
            target_zoom = clamp(target_zoom * 1.1 ** clamp(wheel[1], -10, 10), 0.5, 3)
        mouse_world = engine.screen_to_world(mouse_screen[0], mouse_screen[1])

        update_player(dt)
        update_sandbox()

    world.step(dt, 2, 6)          # 两个子步提高堆叠稳定性
    handle_contacts()
    update_particles(dt)
    update_camera(dt)

    # --- 绘制 ---
    engine.set_background(Color(0.05, 0.06, 0.15, 1))
    engine.set_sort(sort_mode)
    cam_x, cam_y = camera_x, camera_y
    if shake > 0:                 # 屏幕震动只影响渲染相机，不影响鼠标换算
        cam_x += random.range(-1, 1) * shake * 10
        cam_y += random.range(-1, 1) * shake * 10
    engine.set_camera(cam_x, cam_y, camera_zoom)

    view_left, view_top = cam_x - 64, cam_y - 64
    view_right, view_bottom = cam_x + view_w / camera_zoom + 64, cam_y + view_h / camera_zoom + 64

    batch.clear()
    draw_background()
    draw_world()
    draw_particles()
    draw_laser()
    draw_player()
    if debug:
        draw_colliders()
    draw_hud()
    draw_controls()
    draw_cursor()
    engine.draw_sprites(batch)

    # 鼠标拾取：高亮命中的精灵（C API 无法改写已提交精灵，改用信息指示）
    index = engine.pick(mouse_screen[0], mouse_screen[1])
    picked_info = None if index is None else {"index": index}


for dt in engine.frames():
    frame(dt)

if SELFTEST:
    print("=== 自检结果 ===")
    print("\n".join(test_report))
    print(f"\n全部 {len(test_report)} 项通过，运行环境正常" if test_failures == 0
          else f"\n{test_failures} / {len(test_report)} 项失败")
    sys.exit(0 if test_failures == 0 else 1)
