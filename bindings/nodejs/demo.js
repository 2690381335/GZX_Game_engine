'use strict';
/**
 * GZX 引擎 Node.js 绑定 —— 全功能演示
 *
 * 运行： node demo.js        （依赖已预置，无需 npm install）
 *
 * 与 C++ 版 examples/main.cpp 效果一致：平台跳跃 + 物理沙盒 + 粒子压力测试。
 * 涵盖：程序化纹理与音效生成、四套角色动画、物理碰撞与接触事件、
 *       金币拾取、陷阱受伤、激光推动、粒子爆发与风暴、视差背景、
 *       相机跟随与震动、帧时间曲线、操作说明面板、鼠标拾取。
 */

const path = require('path');
const fs = require('fs');
const { Engine, World, SpriteBatch, Key, Mouse, Space, Sort, BodyType, Shape, WHITE, WHITE_TEXTURE } = require('./gzx');

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------
const PI = Math.PI;
const GRAVITY = 1400.0;            // 世界重力加速度（像素/秒²）
const GROUND_TOP = 600.0;          // 地面顶部的世界 Y 坐标
const LEVEL_LEFT = -400.0;         // 关卡左边界
const LEVEL_RIGHT = 2600.0;        // 关卡右边界
const TILE = 32.0;                 // 地形贴图的平铺尺寸
const PLAYER_HALF_W = 14.0;        // 玩家碰撞盒半宽
const PLAYER_HALF_H = 34.0;        // 玩家碰撞盒半高
const PLAYER_SPEED = 280.0;        // 玩家水平移动速度
const JUMP_SPEED = 640.0;          // 玩家起跳速度
const SPRITE_SCALE = 2.0;          // 角色精灵表绘制缩放（单帧 96x96）
const MAX_PARTICLES = 30000;       // 粒子上限（压力测试用，可按机器性能调整）
const MAX_PROPS = 800;             // 可交互物体上限
const SAMPLE_RATE = 48000;         // 程序化音效采样率

const BIT_WORLD = 1 << 0;          // 碰撞类别：地形
const BIT_PLAYER = 1 << 1;         // 碰撞类别：玩家
const BIT_PROP = 1 << 2;           // 碰撞类别：可交互物体
const BIT_PICKUP = 1 << 3;         // 碰撞类别：拾取物
const BIT_HAZARD = 1 << 4;         // 碰撞类别：陷阱

const LAYER_SKY = -100, LAYER_STARS = -99, LAYER_FAR = -98, LAYER_NEAR = -97;
const LAYER_DECOR = -1, LAYER_WORLD = 0, LAYER_FX = 5, LAYER_LASER = 8;
const LAYER_PLAYER = 10, LAYER_DEBUG = 50, LAYER_PANEL = 100, LAYER_TEXT = 101, LAYER_CURSOR = 200;

/** 刚体的游戏语义，存放在刚体的 userData 中 */
const Kind = { None: 0, Ground: 1, Platform: 2, Mover: 3, Crate: 4, Ball: 5, Coin: 6, Spike: 7, Player: 8 };

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------

/** xorshift32 伪随机数生成器，比 Math.random 更快且可复现 */
class Random {
  constructor(seed) { this.state = seed || 1; }
  next() {
    let s = this.state;
    s ^= s << 13; s ^= s >>> 17; s ^= s << 5;
    this.state = s >>> 0;
    return (this.state >>> 8) * (1.0 / 16777216.0);
  }
  range(low, high) { return low + (high - low) * this.next(); }
}

const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);
const clamp01 = (v) => clamp(v, 0, 1);
const hyp = Math.hypot;

/** HSV 转 RGB */
function hsv(hue, saturation, value, alpha = 1) {
  const ch = (n) => {
    const k = ((n + hue * 6) % 6 + 6) % 6;
    return value - value * saturation * clamp01(Math.min(k, 4 - k));
  };
  return { r: ch(5), g: ch(3), b: ch(1), a: alpha };
}

/** 整数坐标哈希噪声，用于程序化纹理的颗粒细节 */
function hashNoise(x, y) {
  let h = (Math.imul(x, 374761393) + Math.imul(y, 668265263)) >>> 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177) >>> 0;
  return ((h ^ (h >>> 16)) & 0xFFFFFF) / 16777216.0;
}

/** MIDI 音符编号转频率，69 为 A4 */
const midi = (note) => 440.0 * Math.pow(2, (note - 69) / 12.0);

/** 线性扫频正弦波在时刻 t 的相位，对频率积分避免相位跳变 */
const sweepPhase = (t, duration, from, to) => 2 * PI * (from * t + (to - from) * t * t / (2 * duration));

// ---------------------------------------------------------------------------
// 资源路径
// ---------------------------------------------------------------------------
const ASSETS = (() => {
  for (const candidate of [
    path.join(__dirname, '..', '..', 'samples', 'assets', 'Sprites'),
    path.join(__dirname, '..', 'Sprites'),
    path.join(__dirname, 'Sprites'),
  ]) {
    if (fs.existsSync(path.join(candidate, 'IDLE.png'))) return candidate;
  }
  throw new Error('找不到贴图目录，请在 SDK 的 bindings/nodejs 下运行');
})();

const FONT_CANDIDATES = process.platform === 'win32'
  ? ['C:/Windows/Fonts/msyh.ttc', 'C:/Windows/Fonts/simhei.ttf', 'C:/Windows/Fonts/simsun.ttc', 'C:/Windows/Fonts/msyhbd.ttc']
  : ['/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc',
     '/usr/share/fonts/truetype/wqy/wqy-microhei.ttc',
     '/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf'];

// ---------------------------------------------------------------------------
// 程序化资源生成
// ---------------------------------------------------------------------------

/** 按像素着色函数生成 RGBA8 纹理，演示 createTexture 接口 */
function bake(engine, width, height, shader) {
  const rgba = new Uint8Array(width * height * 4);
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      const c = shader((x + 0.5) / width, (y + 0.5) / height);
      const o = (y * width + x) * 4;
      rgba[o] = clamp01(c.r) * 255 + 0.5;
      rgba[o + 1] = clamp01(c.g) * 255 + 0.5;
      rgba[o + 2] = clamp01(c.b) * 255 + 0.5;
      rgba[o + 3] = clamp01(c.a) * 255 + 0.5;
    }
  }
  return engine.createTexture(width, height, rgba);
}

/** 按波形函数合成单声道音效，演示 createSound 接口 */
function synth(engine, seconds, wave) {
  const count = Math.floor(seconds * SAMPLE_RATE);
  const samples = new Float32Array(count);
  for (let i = 0; i < count; ++i) samples[i] = wave(i / SAMPLE_RATE);
  return engine.createSound(samples, 1, SAMPLE_RATE);
}

// ---------------------------------------------------------------------------
// 精灵表动画
// ---------------------------------------------------------------------------

/** 网格精灵表动画播放器 */
class Animation {
  /**
   * @param {number} columns 列数
   * @param {number} rows 行数
   * @param {number} frames 有效帧数
   * @param {number} fps 播放速率
   * @param {boolean} [loop=true] 是否循环
   */
  constructor(columns, rows, frames, fps, loop = true) {
    this.columns = columns; this.rows = rows; this.frames = frames;
    this.fps = fps; this.loop = loop;
    this.frame = 0; this.time = 0;
  }
  reset() { this.frame = 0; this.time = 0; }
  /** @returns {boolean} 非循环动画是否已播放完毕 */
  finished() { return !this.loop && this.frame >= this.frames - 1 && this.time >= 1 / this.fps; }
  update(dt) {
    this.time += dt;
    const step = 1 / this.fps;
    while (this.time >= step) {
      this.time -= step;
      if (this.frame + 1 < this.frames) ++this.frame;
      else if (this.loop) this.frame = 0;
      else break;
    }
  }
  uv() { return Engine.sheetUv(this.columns, this.rows, this.frame); }
}

// ---------------------------------------------------------------------------
// 界面文本（有中文字体时显示中文，否则回退到像素字体英文）
// ---------------------------------------------------------------------------
const T = {
  title: { zh: 'GZX 引擎演示', en: 'GZX ENGINE DEMO' },
  fps: { zh: '帧率 %s   帧时间 %s 毫秒', en: 'FPS %s   FRAME %s MS' },
  stats: { zh: '精灵 %s   刚体 %s   接触 %s   粒子 %s', en: 'SPRITES %s  BODIES %s  CONTACTS %s  PARTICLES %s' },
  score: { zh: '金币 %s / %s', en: 'COINS %s / %s' },
  audioOff: { zh: '音频设备不可用', en: 'AUDIO UNAVAILABLE' },
  picked: { zh: '拾取 第 %s 个精灵  层级 %s', en: 'PICK #%s  LAYER %s' },
  helpTitle: { zh: '操作说明（按 H 隐藏）', en: 'CONTROLS (H TO HIDE)' },
  helpHidden: { zh: '按 H 显示操作说明', en: 'PRESS H FOR CONTROLS' },
  on: { zh: '开启', en: 'ON' },
  off: { zh: '关闭', en: 'OFF' },
};
const SORT_NAMES = [{ zh: '不排序', en: 'NONE' }, { zh: '按层级', en: 'LAYER' }, { zh: '层级+纹理', en: 'LAYER+TEXTURE' }];

/** 操作说明：按键、功能、状态类型 */
const CONTROLS = [
  [{ zh: 'A / D', en: 'A / D' }, { zh: '左右移动', en: 'MOVE' }, null],
  [{ zh: 'W / 空格', en: 'W / SPACE' }, { zh: '跳跃', en: 'JUMP' }, null],
  [{ zh: 'J', en: 'J' }, { zh: '攻击', en: 'ATTACK' }, null],
  [{ zh: '鼠标左键', en: 'LEFT CLICK' }, { zh: '生成木箱', en: 'SPAWN CRATE' }, null],
  [{ zh: 'Shift + 左键', en: 'SHIFT + CLICK' }, { zh: '生成小球', en: 'SPAWN BALL' }, null],
  [{ zh: '按住右键', en: 'HOLD RIGHT' }, { zh: '激光推动物体', en: 'LASER PUSH' }, null],
  [{ zh: '鼠标滚轮', en: 'WHEEL' }, { zh: '缩放镜头', en: 'ZOOM' }, null],
  [{ zh: 'P', en: 'P' }, { zh: '粒子风暴', en: 'PARTICLE STORM' }, 'storm'],
  [{ zh: 'F1', en: 'F1' }, { zh: '切换排序模式', en: 'SORT MODE' }, 'sort'],
  [{ zh: 'F2', en: 'F2' }, { zh: '显示碰撞框', en: 'COLLIDERS' }, 'debug'],
  [{ zh: 'Q / E', en: 'Q / E' }, { zh: '音量 减 / 加', en: 'VOLUME - / +' }, 'volume'],
  [{ zh: 'M', en: 'M' }, { zh: '静音', en: 'MUTE' }, 'mute'],
  [{ zh: 'R', en: 'R' }, { zh: '重置关卡', en: 'RESET LEVEL' }, null],
  [{ zh: 'Esc', en: 'ESC' }, { zh: '退出', en: 'QUIT' }, null],
];

// ---------------------------------------------------------------------------
// 游戏主体
// ---------------------------------------------------------------------------

const engine = new Engine({ title: 'GZX Engine - Node.js 全功能演示', width: 1280, height: 720, resizable: true });
const random = new Random(0x9E3779B9);
const batch = new SpriteBatch(8192);

/** 把纯色矩形塞进精灵批次，避免逐矩形的跨语言调用开销 */
function rect(x, y, w, h, color, space, layer) {
  batch.add(WHITE_TEXTURE, x, y, w, h, { color, space, layer });
}

// --- 资源 ---
const tex = {
  idle: engine.loadTexture(path.join(ASSETS, 'IDLE.png')),
  run: engine.loadTexture(path.join(ASSETS, 'RUN.png')),
  hurt: engine.loadTexture(path.join(ASSETS, 'HURT.png')),
  logo: engine.loadTexture(path.join(ASSETS, 'GZX ENGINE_LOGO.png')),
  attack: 0,   // 首次攻击时才加载，演示运行期增量上传
};

// 程序化纹理
tex.disc = bake(engine, 64, 64, (u, v) => {
  const distance = hyp(u - 0.5, v - 0.5) * 2;
  const shade = 1 - 0.35 * hyp(u - 0.35, v - 0.35);   // 左上角高光
  return { r: shade, g: shade, b: shade, a: clamp01((1 - distance) * 32) };
});
tex.glow = bake(engine, 64, 64, (u, v) => {
  const falloff = Math.max(0, 1 - hyp(u - 0.5, v - 0.5) * 2);
  return { r: 1, g: 1, b: 1, a: falloff * falloff };
});
tex.crate = bake(engine, 32, 32, (u, v) => {
  const px = u * 32, py = v * 32;
  const border = px < 3 || px > 29 || py < 3 || py > 29;
  const brace = Math.abs(px - py) < 2.5 || Math.abs(px - (32 - py)) < 2.5;
  const grain = 0.9 + 0.1 * hashNoise(px | 0, (py / 4) | 0);
  return border || brace
    ? { r: 0.45 * grain, g: 0.27 * grain, b: 0.12 * grain, a: 1 }
    : { r: 0.78 * grain, g: 0.55 * grain, b: 0.30 * grain, a: 1 };
});
tex.ground = bake(engine, 32, 32, (u, v) => {
  const x = (u * 32) | 0, y = (v * 32) | 0;
  const grassEdge = 6 + 3 * hashNoise(x, 7);           // 草地边缘参差不齐
  const noise = hashNoise(x, y);
  if (v * 32 < grassEdge) return { r: 0.30 + 0.1 * noise, g: 0.72 + 0.1 * noise, b: 0.32, a: 1 };
  const speck = noise > 0.92 ? 0.15 : 0;
  return { r: 0.40 + speck, g: 0.26 + speck, b: 0.18 + speck, a: 1 };
});
tex.spike = bake(engine, 32, 32, (u, v) => {
  const local = (u * 2) % 1;                            // 每块贴图两根尖刺
  const edge = Math.abs(local - 0.5) * 2;
  if (v < edge) return { r: 0, g: 0, b: 0, a: 0 };
  const metal = 0.55 + 0.4 * (1 - local);
  return { r: metal, g: metal, b: metal + 0.08, a: 1 };
});
tex.sky = bake(engine, 1, 256, (u, v) => {
  const top = [0.04, 0.05, 0.14], middle = [0.20, 0.12, 0.32], bottom = [0.62, 0.32, 0.38];
  const mix = (a, b, t) => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];
  const c = v < 0.6 ? mix(top, middle, v / 0.6) : mix(middle, bottom, (v - 0.6) / 0.4);
  return { r: c[0], g: c[1], b: c[2], a: 1 };
});
tex.hills = bake(engine, 512, 128, (u, v) => {
  const angle = u * 2 * PI;                             // 整数倍频率保证左右无缝平铺
  const height = 0.55 + 0.18 * Math.sin(angle * 2) + 0.10 * Math.sin(angle * 5 + 1.3) + 0.05 * Math.sin(angle * 11 + 0.7);
  return { r: 1, g: 1, b: 1, a: clamp01((v - (1 - height)) * 128) };
});

// --- 字体 ---
const pixelFont = engine.defaultFont();
let ttfFont = null;
let chinese = false;

/** 界面用到的全部中文字符，用于烘焙字体图集 */
const CHARS = (() => {
  const all = [T.title, T.fps, T.stats, T.score, T.audioOff, T.picked, T.helpTitle, T.helpHidden, T.on, T.off]
    .concat(SORT_NAMES).map((t) => t.zh).join('');
  return all + CONTROLS.map((c) => c[0].zh + c[1].zh).join('');
})();

for (const candidate of FONT_CANDIDATES) {
  if (!fs.existsSync(candidate)) continue;
  try {
    ttfFont = engine.ttfFont(candidate, 18, CHARS);
    chinese = true;
    console.log(`中文字体: ${candidate}`);
    break;
  } catch (error) {
    console.warn(`字体 ${candidate} 不可用: ${error.message}`);
  }
}
const font = () => ttfFont || pixelFont;
const fontScale = () => (ttfFont ? 1.0 : 2.0);
const tr = (text) => (ttfFont ? text.zh : text.en);

// --- 音效 ---
const audio = { jump: 0, hit: 0, coin: 0, hurt: 0, spawn: 0, music: 0 };
if (engine.audioAvailable) {
  audio.jump = synth(engine, 0.16, (t) => {
    const env = 1 - t / 0.16;
    return 0.35 * env * env * Math.sin(sweepPhase(t, 0.16, 320, 760));
  });
  audio.hit = synth(engine, 0.14, ((noise) => (t) => {
    const env = 1 - t / 0.14;
    return 0.45 * env * env * env * noise.range(-1, 1) + 0.5 * env * Math.sin(2 * PI * 90 * t);
  })(new Random(1234)));
  audio.coin = synth(engine, 0.22, (t) => {
    const freq = t < 0.07 ? 988 : 1319;                 // B5 -> E6 经典拾取音
    const phase = 2 * PI * freq * t;
    return 0.25 * (1 - t / 0.22) * (Math.sin(phase) + Math.sin(phase * 3) / 3);
  });
  audio.hurt = synth(engine, 0.3, (t) => {
    const cycles = sweepPhase(t, 0.3, 520, 110) / (2 * PI);
    return 0.25 * (1 - t / 0.3) * (2 * (cycles - Math.floor(cycles)) - 1);   // 下行锯齿波
  });
  audio.spawn = synth(engine, 0.06, (t) => 0.3 * (1 - t / 0.06) * Math.sin(2 * PI * 220 * t));

  const MELODY = [57, 60, 64, 67, 69, 67, 64, 60, 53, 57, 60, 64, 65, 64, 60, 57,
                  48, 52, 55, 60, 64, 60, 55, 52, 55, 59, 62, 67, 71, 67, 62, 59];  // Am-F-C-G 琶音
  const BASS = [45, 41, 36, 43];
  audio.music = synth(engine, 8.0, (t) => {
    const step = Math.floor(t / 0.25) % 32;
    const local = t - step * 0.25;
    const lead = 2 / PI * Math.asin(Math.sin(2 * PI * midi(MELODY[step]) * local));   // 三角波
    const chord = Math.floor(t / 2) % 4;
    const bassTime = t - chord * 2;
    return 0.16 * lead * Math.exp(-local * 5) + 0.22 * Math.sin(2 * PI * midi(BASS[chord]) * bassTime) * Math.exp(-bassTime * 1.2);
  });
  engine.play(audio.music, { volume: 0.3, pan: 0, loop: true });
}

let volume = 0.8, muted = false;
engine.setMasterVolume(volume);

// --- 星空 ---
const stars = Array.from({ length: 180 }, () => ({
  x: random.next(), y: random.next() * 0.65,
  size: random.range(1, 3), phase: random.range(0, 2 * PI), speed: random.range(1, 4),
}));

// --- 物理世界与关卡 ---
let world, bodies = [], player = 0n, mover = 0n;
let propCount = 0, coins = 0, coinsTotal = 0;

const anim = {
  idle: new Animation(10, 1, 10, 10),
  run: new Animation(16, 1, 16, 18),
  attack: new Animation(7, 1, 7, 20, false),
  hurt: new Animation(4, 1, 4, 10, false),
};
let facingLeft = false, running = false, grounded = false;
let attacking = false, attackHitDone = false, hurting = false;
let attackTimer = 0, hurtTimer = 0;

let particles = [];
let sortMode = 1, debug = false, storm = false, showHelp = true;

// 相机
let cameraX = 0, cameraY = 0, cameraZoom = 1.5, targetZoom = 1.5, shake = 0;
let centerX = 0, centerY = GROUND_TOP - 120;
let viewW = 1280, viewH = 720;
let viewLeft = 0, viewTop = 0, viewRight = 0, viewBottom = 0;
let mouseScreen = [0, 0], mouseWorld = [0, 0];
let laserActive = false, laserHit = false, laserFrom = [0, 0], laserTo = [0, 0];
let pickedInfo = null;

let time = 0, frameMs = 16.7;
const HISTORY = 120;
const history = new Float32Array(HISTORY);
let historyIndex = 0;
let lastDraw = process.hrtime.bigint();

function addBody(kind, definition, tint) {
  definition.userData = BigInt(kind);
  const handle = world.createBody(definition);
  const round = definition.shape === Shape.CIRCLE;
  bodies.push({
    handle, kind,
    halfWidth: round ? definition.radius : definition.halfWidth,
    halfHeight: round ? definition.radius : definition.halfHeight,
    tint,
  });
  return handle;
}

function addSolid(kind, x, y, halfWidth, halfHeight, tint) {
  addBody(kind, {
    type: BodyType.STATIC, shape: Shape.BOX, x, y, halfWidth, halfHeight,
    category: BIT_WORLD, friction: 0.6,
  }, tint);
}

function spawnProp(ball, x, y) {
  if (propCount >= MAX_PROPS) return;
  const definition = ball
    ? { type: BodyType.DYNAMIC, shape: Shape.CIRCLE, x, y, radius: 14, mass: 0.6, restitution: 0.6, friction: 0.2 }
    : { type: BodyType.DYNAMIC, shape: Shape.BOX, x, y, halfWidth: 20, halfHeight: 20, mass: 1, restitution: 0.05, friction: 0.5 };
  definition.category = BIT_PROP;
  definition.mask = BIT_WORLD | BIT_PROP | BIT_PLAYER;
  addBody(ball ? Kind.Ball : Kind.Crate, definition, ball ? hsv(random.next(), 0.65, 1) : { r: 1, g: 1, b: 1, a: 1 });
  ++propCount;
}

function addCoin(x, y) {
  addBody(Kind.Coin, {
    type: BodyType.STATIC, shape: Shape.CIRCLE, x, y, radius: 10,
    sensor: true, category: BIT_PICKUP, mask: BIT_PLAYER,
  }, { r: 1, g: 0.82, b: 0.25, a: 1 });
  ++coinsTotal;
}

function buildLevel() {
  world = new World(0, GRAVITY, 64);
  bodies = [];
  particles = [];
  propCount = 0;
  coins = coinsTotal = 0;

  const grass = { r: 1, g: 1, b: 1, a: 1 };
  addSolid(Kind.Ground, (LEVEL_LEFT + LEVEL_RIGHT) * 0.5, GROUND_TOP + 200, (LEVEL_RIGHT - LEVEL_LEFT) * 0.5, 200, grass);
  addSolid(Kind.Ground, LEVEL_LEFT - 20, GROUND_TOP - 400, 20, 600, grass);
  addSolid(Kind.Ground, LEVEL_RIGHT + 20, GROUND_TOP - 400, 20, 600, grass);

  const platforms = [[520, 470, 192], [840, 370, 160], [1180, 470, 192], [1560, 350, 224], [2100, 300, 160]];
  for (const [x, top, width] of platforms) {
    addSolid(Kind.Platform, x, top + 12, width * 0.5, 12, grass);
    for (let c = -1; c <= 1; ++c) addCoin(x + c * 40, top - 30);
  }
  for (let x = -200; x < 400; x += 60) addCoin(x, GROUND_TOP - 30);

  mover = addBody(Kind.Mover, {
    type: BodyType.KINEMATIC, shape: Shape.BOX, x: 1840, y: 440, halfWidth: 64, halfHeight: 10,
    category: BIT_WORLD, friction: 0.8,
  }, { r: 0.55, g: 0.8, b: 1, a: 1 });

  addBody(Kind.Spike, {
    type: BodyType.STATIC, shape: Shape.BOX, x: 1350, y: GROUND_TOP - 12, halfWidth: 64, halfHeight: 12,
    sensor: true, category: BIT_HAZARD, mask: BIT_PLAYER,
  }, { r: 1, g: 1, b: 1, a: 1 });

  for (let row = 0; row < 4; ++row) {                   // 木箱金字塔
    for (let col = 0; col < 4 - row; ++col) spawnProp(false, 620 + row * 21 + col * 42, GROUND_TOP - 20.5 - row * 40.5);
  }
  for (let b = 0; b < 5; ++b) spawnProp(true, 950 + b * 34, GROUND_TOP - 200);

  player = addBody(Kind.Player, {
    type: BodyType.DYNAMIC, shape: Shape.BOX, x: 0, y: GROUND_TOP - PLAYER_HALF_H - 1,
    halfWidth: PLAYER_HALF_W, halfHeight: PLAYER_HALF_H,
    friction: 0, category: BIT_PLAYER, mask: BIT_WORLD | BIT_PROP | BIT_PICKUP | BIT_HAZARD,
  }, { r: 1, g: 1, b: 1, a: 1 });

  attacking = hurting = false;
  attackTimer = hurtTimer = 0;
  centerX = 0; centerY = GROUND_TOP - 120;
}
buildLevel();

// ---------------------------------------------------------------------------
// 逻辑
// ---------------------------------------------------------------------------

/** 根据世界 X 坐标计算立体声声像 */
const panAt = (x) => clamp((x - centerX) / (viewW * 0.5 / cameraZoom), -1, 1);

function startAttack() {
  if (!tex.attack) tex.attack = engine.loadTexture(path.join(ASSETS, 'ATTACK 1.png'));
  attacking = true; attackHitDone = false; attackTimer = 0;
  anim.attack.reset();
}

function attackHit() {
  attackHitDone = true;
  const pos = world.position(player);
  const dir = facingLeft ? -1 : 1;
  const nearX = pos[0], farX = pos[0] + dir * 90;
  const hits = world.queryRect(Math.min(nearX, farX), pos[1] - 40, Math.max(nearX, farX), pos[1] + 30, BIT_PROP);
  for (const body of hits) {
    world.applyImpulse(body, dir * 420, -320);
    const p = world.position(body);
    emitBurst(p[0], p[1], 40, 360, { r: 1, g: 0.75, b: 0.35, a: 1 }, 0.6);
  }
  if (hits.length) {
    if (engine.audioAvailable) engine.play(audio.hit, { volume: 1, pan: panAt(pos[0]) });
    shake = 0.35;
  }
}

function hurt() {
  if (hurtTimer > 0) return;
  hurtTimer = 1.2; hurting = true; attacking = false;
  anim.hurt.reset();
  world.setVelocity(player, facingLeft ? 260 : -260, -480);
  const pos = world.position(player);
  emitBurst(pos[0], pos[1], 40, 300, { r: 1, g: 0.25, b: 0.25, a: 1 }, 0.6);
  if (engine.audioAvailable) engine.play(audio.hurt, { volume: 0.9, pan: panAt(pos[0]) });
  shake = 0.6;
}

function emitBurst(x, y, count, speed, color, life) {
  for (let i = 0; i < count && particles.length < MAX_PARTICLES; ++i) {
    const angle = random.range(0, 2 * PI), magnitude = random.range(0.2, 1) * speed;
    const span = life * random.range(0.6, 1);
    particles.push({ x, y, vx: Math.cos(angle) * magnitude, vy: Math.sin(angle) * magnitude, life: span, span, size: random.range(4, 9), color });
  }
}

/** @param {boolean} [manual=false] 为 true 时不接管水平速度，供自检脚本直接操控刚体 */
function updatePlayer(dt, manual = false) {
  const pos = world.position(player);
  const vel = world.velocity(player);
  const feet = pos[1] + PLAYER_HALF_H;

  grounded = false;
  for (const offset of [-PLAYER_HALF_W + 2, PLAYER_HALF_W - 2]) {
    if (world.raycast(pos[0] + offset, feet - 2, pos[0] + offset, feet + 4, BIT_WORLD | BIT_PROP)) grounded = true;
  }   // 两侧脚底向下射线检测着地

  hurtTimer = Math.max(0, hurtTimer - dt);
  if (hurting) { anim.hurt.update(dt); hurting = !anim.hurt.finished(); }

  if (!hurting && !manual) {
    const left = engine.keyDown(Key.A) || engine.keyDown(Key.LEFT);
    const right = engine.keyDown(Key.D) || engine.keyDown(Key.RIGHT);
    const dir = (right ? 1 : 0) - (left ? 1 : 0);
    vel[0] = dir * PLAYER_SPEED;
    if (dir !== 0) facingLeft = dir < 0;

    if ((dir !== 0) !== running) { running = dir !== 0; (running ? anim.run : anim.idle).reset(); }

    if (grounded && (engine.keyPressed(Key.W) || engine.keyPressed(Key.UP) || engine.keyPressed(Key.SPACE))) {
      vel[1] = -JUMP_SPEED;
      if (engine.audioAvailable) engine.play(audio.jump, { volume: 0.8, pan: panAt(pos[0]) });
    }
    world.setVelocity(player, vel[0], vel[1]);

    if (!attacking && engine.keyPressed(Key.J)) startAttack();
  }

  if (attacking) {
    anim.attack.update(dt);
    attackTimer += dt;
    if (!attackHitDone && attackTimer >= 0.12) attackHit();   // 挥刀到第 3 帧左右时判定命中
    if (anim.attack.finished()) attacking = false;
  }
  (running ? anim.run : anim.idle).update(dt);

  if (pos[1] > GROUND_TOP + 1000) {                            // 掉出世界后复活
    world.setPosition(player, 0, GROUND_TOP - PLAYER_HALF_H - 1);
    world.setVelocity(player, 0, 0);
  }
}

function updateSandbox() {
  if (engine.mousePressed(Mouse.LEFT)) {
    const ball = engine.keyDown(Key.LEFT_SHIFT) || engine.keyDown(Key.RIGHT_SHIFT);
    spawnProp(ball, mouseWorld[0], mouseWorld[1]);
    if (engine.audioAvailable) engine.play(audio.spawn, { volume: 0.6, pan: panAt(mouseWorld[0]) });
  }

  laserActive = engine.mouseDown(Mouse.RIGHT);
  if (laserActive) {
    const origin = world.position(player);
    laserFrom = [origin[0], origin[1] - 10];
    const dx = mouseWorld[0] - laserFrom[0], dy = mouseWorld[1] - laserFrom[1];
    const length = Math.max(hyp(dx, dy), 1e-3);
    const end = [laserFrom[0] + dx / length * 900, laserFrom[1] + dy / length * 900];
    const hit = world.raycast(laserFrom[0], laserFrom[1], end[0], end[1], BIT_WORLD | BIT_PROP);
    laserTo = hit ? [hit.x, hit.y] : end;
    laserHit = !!hit;
    if (hit) {
      const kind = Number(world.userData(hit.body));
      if (kind === Kind.Crate || kind === Kind.Ball) world.applyImpulse(hit.body, dx / length * 30, dy / length * 30);
      emitBurst(hit.x, hit.y, 3, 220, { r: 1, g: 0.3, b: 0.25, a: 1 }, 0.35);
    }
  }

  if (storm) {   // 每帧补充粒子，稳定在数万个精灵，用于压力测试
    const width = viewW / cameraZoom;
    for (let i = 0; i < 400 && particles.length < MAX_PARTICLES; ++i) {
      const span = random.range(2, 3);
      particles.push({
        x: cameraX + random.next() * width, y: cameraY - 20,
        vx: random.range(-30, 30), vy: random.range(100, 300),
        life: span, span, size: random.range(3, 7),
        color: hsv(random.range(0.5, 0.65), 0.6, 1, 0.8),
      });
    }
  }
}

function handleContacts() {
  const pending = [];
  for (const contact of world.contacts()) {
    if (!world.valid(contact.a) || !world.valid(contact.b)) continue;
    const ka = Number(world.userData(contact.a)), kb = Number(world.userData(contact.b));
    if (ka !== Kind.Player && kb !== Kind.Player) continue;
    const other = ka === Kind.Player ? contact.b : contact.a;
    const kind = ka === Kind.Player ? kb : ka;
    if (kind === Kind.Coin) pending.push(other);
    if (kind === Kind.Spike) hurt();
  }
  for (const coin of pending) {
    if (!world.valid(coin)) continue;                          // 同一金币可能在多个子步中重复上报
    const p = world.position(coin);
    world.destroyBody(coin);
    ++coins;
    emitBurst(p[0], p[1], 30, 260, { r: 1, g: 0.85, b: 0.3, a: 1 }, 0.7);
    if (engine.audioAvailable) engine.play(audio.coin, { volume: 0.8, pan: panAt(p[0]) });
  }
  if (pending.length) bodies = bodies.filter((b) => world.valid(b.handle));
}

function updateParticles(dt) {
  for (let i = 0; i < particles.length;) {
    const p = particles[i];
    p.life -= dt;
    if (p.life <= 0) { particles[i] = particles[particles.length - 1]; particles.pop(); continue; }
    p.vy += 900 * dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if (p.y > GROUND_TOP && p.vy > 0) { p.y = GROUND_TOP; p.vy *= -0.3; }   // 落地轻微反弹
    ++i;
  }
}

function updateCamera(dt) {
  const blend = 1 - Math.exp(-dt * 6);                         // 与帧率无关的指数平滑
  const target = world.position(player);
  centerX += (target[0] - centerX) * blend;
  centerY += (Math.min(target[1] - 60, GROUND_TOP - 120) - centerY) * blend;
  cameraZoom += (targetZoom - cameraZoom) * blend;
  cameraX = centerX - viewW * 0.5 / cameraZoom;
  cameraY = centerY - viewH * 0.5 / cameraZoom;
  shake = Math.max(0, shake - dt);
}

function handleToggles() {
  if (engine.keyPressed(Key.R)) buildLevel();
  if (engine.keyPressed(Key.F1)) sortMode = (sortMode + 1) % 3;
  if (engine.keyPressed(Key.F2)) debug = !debug;
  if (engine.keyPressed(Key.P)) storm = !storm;
  if (engine.keyPressed(Key.H)) showHelp = !showHelp;

  const previous = volume;
  if (engine.keyPressed(Key.Q)) volume = Math.max(0, volume - 0.1);
  if (engine.keyPressed(Key.E)) volume = Math.min(1, volume + 0.1);
  if (engine.keyPressed(Key.M)) muted = !muted;
  if (previous !== volume || engine.keyPressed(Key.M)) engine.setMasterVolume(muted ? 0 : volume);
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

const visible = (l, t, r, b) => r >= viewLeft && l <= viewRight && b >= viewTop && t <= viewBottom;

/** 在世界空间水平平铺纹理，只提交可见的贴图块 */
function drawTiled(texture, x, y, width, height, tint, layer) {
  const right = x + width;
  let left = x;
  if (viewLeft > left) left += Math.floor((viewLeft - left) / TILE) * TILE;   // 跳过左侧不可见的贴图块
  for (; left < right && left <= viewRight; left += TILE) {
    const tile = Math.min(TILE, right - left);
    batch.add(texture, left, y, tile, height, { uv: { left: 0, top: 0, right: tile / TILE, bottom: 1 }, color: tint, layer });
  }
}

function drawBackground() {
  batch.add(tex.sky, 0, 0, viewW, viewH, { space: Space.SCREEN, layer: LAYER_SKY });

  for (const star of stars) {
    const alpha = 0.725 + 0.275 * Math.sin(time * star.speed + star.phase);
    batch.add(tex.disc, star.x * viewW, star.y * viewH, star.size, star.size,
      { color: { r: 1, g: 0.95, b: 0.85, a: alpha }, space: Space.SCREEN, layer: LAYER_STARS });
  }

  const hills = (parallax, heightRatio, tint, layer) => {     // 视差系数越小越远
    const width = 640, height = viewH * heightRatio;
    let offset = -((cameraX * cameraZoom * parallax) % width);
    if (offset > 0) offset -= width;
    for (let x = offset; x < viewW; x += width) {
      batch.add(tex.hills, x, viewH - height, width + 1, height, { color: tint, space: Space.SCREEN, layer });
    }
  };
  hills(0.15, 0.55, { r: 0.22, g: 0.16, b: 0.36, a: 1 }, LAYER_FAR);
  hills(0.4, 0.4, { r: 0.13, g: 0.10, b: 0.24, a: 1 }, LAYER_NEAR);
}

function drawWorld() {
  const HUB = [1000, 420];
  if (visible(HUB[0] - 120, HUB[1] - 120, HUB[0] + 120, GROUND_TOP)) {
    rect(HUB[0] - 8, HUB[1], 16, GROUND_TOP - HUB[1], { r: 0.25, g: 0.2, b: 0.3, a: 1 }, Space.WORLD, LAYER_DECOR);
    for (let blade = 0; blade < 4; ++blade) {                  // 绕叶片根部旋转
      batch.add(WHITE_TEXTURE, HUB[0], HUB[1] - 7, 110, 14, {
        color: { r: 0.85, g: 0.8, b: 0.9, a: 1 }, rotation: time * 1.2 + blade * PI * 0.5,
        pivotX: 0, pivotY: 0.5, layer: LAYER_DECOR,
      });
    }
    batch.add(tex.disc, HUB[0] - 10, HUB[1] - 10, 20, 20, { color: { r: 0.4, g: 0.35, b: 0.45, a: 1 }, layer: LAYER_DECOR });
  }

  for (const body of bodies) {
    if (body.kind === Kind.Player) continue;
    const c = world.position(body.handle);
    const left = c[0] - body.halfWidth, top = c[1] - body.halfHeight;
    const width = body.halfWidth * 2, height = body.halfHeight * 2;
    if (!visible(left, top, left + width, top + height)) continue;   // 视锥剔除

    switch (body.kind) {
      case Kind.Ground:
      case Kind.Platform:
      case Kind.Mover: {
        const grass = Math.min(TILE, height);
        drawTiled(tex.ground, left, top, width, grass, body.tint, LAYER_WORLD);
        if (height > grass) rect(left, top + grass, width, height - grass, { r: 0.30, g: 0.19, b: 0.13, a: 1 }, Space.WORLD, LAYER_WORLD);
        break;
      }
      case Kind.Spike:
        drawTiled(tex.spike, left, top, width, height, body.tint, LAYER_WORLD);
        break;
      case Kind.Crate:
        batch.add(tex.crate, left, top, width, height, { color: body.tint, layer: LAYER_WORLD });
        break;
      case Kind.Ball:
        batch.add(tex.disc, left, top, width, height, { color: body.tint, layer: LAYER_WORLD });
        break;
      case Kind.Coin: {
        const bob = Math.sin(time * 3 + c[0] * 0.05) * 3;
        const spin = Math.max(0.15, Math.abs(Math.cos(time * 4 + c[0])));   // 压缩宽度模拟金币翻转
        batch.add(tex.glow, c[0] - 20, c[1] - 20 + bob, 40, 40, { color: { r: 1, g: 0.8, b: 0.3, a: 0.35 }, layer: LAYER_WORLD });
        batch.add(tex.disc, c[0] - body.halfWidth * spin, top + bob, width * spin, height, { color: body.tint, layer: LAYER_WORLD });
        break;
      }
    }
  }
}

function drawParticles() {
  for (const p of particles) {
    const half = p.size * 0.5;
    if (!visible(p.x - half, p.y - half, p.x + half, p.y + half)) continue;
    const color = { ...p.color, a: p.color.a * (p.life / p.span) };   // 透明度随剩余寿命衰减
    batch.add(tex.glow, p.x - half, p.y - half, p.size, p.size, { color, layer: LAYER_FX });
  }
}

function drawLaser() {
  if (!laserActive) return;
  const dx = laserTo[0] - laserFrom[0], dy = laserTo[1] - laserFrom[1];
  const length = hyp(dx, dy), angle = Math.atan2(dy, dx);
  for (const [thickness, color] of [[10, { r: 1, g: 0.2, b: 0.2, a: 0.3 }], [2, { r: 1, g: 0.9, b: 0.9, a: 1 }]]) {
    batch.add(WHITE_TEXTURE, laserFrom[0], laserFrom[1] - thickness * 0.5, length, thickness, {
      color, rotation: angle, pivotX: 0, pivotY: 0.5, layer: LAYER_LASER,
    });
  }
  if (laserHit) batch.add(tex.glow, laserTo[0] - 16, laserTo[1] - 16, 32, 32, { color: { r: 1, g: 0.4, b: 0.3, a: 0.9 }, layer: LAYER_LASER });
}

function drawPlayer() {
  const pos = world.position(player);
  let animation = anim.idle, texture = tex.idle;
  if (hurting) { animation = anim.hurt; texture = tex.hurt; }
  else if (attacking) { animation = anim.attack; texture = tex.attack; }
  else if (running && grounded) { animation = anim.run; texture = tex.run; }

  const uv = animation.uv();
  if (facingLeft) { const t = uv.left; uv.left = uv.right; uv.right = t; }   // 交换 UV 实现水平翻转
  const blink = hurtTimer > 0 && (time % 0.15) < 0.075;                      // 无敌时间内闪烁
  const size = 96 * SPRITE_SCALE;

  batch.add(tex.glow, pos[0] - 28, pos[1] + PLAYER_HALF_H - 6, 56, 12,
    { color: { r: 0, g: 0, b: 0, a: 0.45 }, layer: LAYER_PLAYER });          // 脚底阴影
  batch.add(texture, pos[0] - size * 0.5, pos[1] + PLAYER_HALF_H - 80 * SPRITE_SCALE, size, size,
    { uv, color: { r: 1, g: 1, b: 1, a: blink ? 0.35 : 1 }, layer: LAYER_PLAYER });   // 精灵帧内脚底位于第 80 行
}

function drawColliders() {
  for (const body of bodies) {
    const c = world.position(body.handle);
    const color = body.kind === Kind.Player ? { r: 0.2, g: 1, b: 0.4, a: 0.35 }
      : body.kind === Kind.Coin || body.kind === Kind.Spike ? { r: 1, g: 0.9, b: 0.2, a: 0.3 }
        : { r: 0.3, g: 0.7, b: 1, a: 0.25 };
    rect(c[0] - body.halfWidth, c[1] - body.halfHeight, body.halfWidth * 2, body.halfHeight * 2, color, Space.WORLD, LAYER_DEBUG);
  }
}

/** 带阴影的屏幕空间文字 */
function drawText(text, x, y, color) {
  engine.drawText(font(), text, x + 1, y + 1, { scale: fontScale(), color: { r: 0, g: 0, b: 0, a: 0.7 }, space: Space.SCREEN, layer: LAYER_TEXT });
  engine.drawText(font(), text, x, y, { scale: fontScale(), color, space: Space.SCREEN, layer: LAYER_TEXT });
}

/** 带半透明底板的文字面板 */
function drawPanel(lines, x, y, highlight) {
  const lineHeight = font().lineHeight * fontScale() + 4;
  let width = 0;
  for (const line of lines) width = Math.max(width, font().measure(line, fontScale())[0]);
  const height = lineHeight * lines.length;
  if (y < 0) y = viewH - height + y;

  rect(x - 10, y - 8, width + 20, height + 12, { r: 0, g: 0, b: 0, a: 0.45 }, Space.SCREEN, LAYER_PANEL);
  for (let i = 0; i < lines.length; ++i) {
    drawText(lines[i], x, y + i * lineHeight, highlight && i === 0 ? { r: 1, g: 0.85, b: 0.4, a: 1 } : { r: 0.92, g: 0.94, b: 1, a: 1 });
  }
}

function statusText(status) {
  switch (status) {
    case 'storm': return { text: tr(storm ? T.on : T.off), active: storm };
    case 'sort': return { text: tr(SORT_NAMES[sortMode]), active: true };
    case 'debug': return { text: tr(debug ? T.on : T.off), active: debug };
    case 'volume': return { text: `${Math.round(volume * 100)}%`, active: !muted };
    case 'mute': return { text: tr(muted ? T.on : T.off), active: muted };
    default: return { text: '', active: false };
  }
}

function drawHud() {
  const lines = [
    tr(T.title),
    tr(T.fps).replace('%s', frameMs > 0 ? (1000 / frameMs).toFixed(0) : '0').replace('%s', frameMs.toFixed(2)),
    tr(T.stats).replace('%s', batch.count).replace('%s', world.bodyCount).replace('%s', world.contacts().length).replace('%s', particles.length),
    tr(T.score).replace('%s', coins).replace('%s', coinsTotal),
  ];
  if (!engine.audioAvailable) lines.push(tr(T.audioOff));
  if (pickedInfo) lines.push(tr(T.picked).replace('%s', pickedInfo.index).replace('%s', pickedInfo.layer));
  drawPanel(lines, 20, 18, true);

  // 帧时间曲线
  const graphRight = viewW - 20, graphBottom = 90;
  rect(graphRight - HISTORY * 2 - 6, 14, HISTORY * 2 + 12, graphBottom - 8, { r: 0, g: 0, b: 0, a: 0.45 }, Space.SCREEN, LAYER_PANEL);
  for (let i = 0; i < HISTORY; ++i) {
    const ms = history[(historyIndex + i) % HISTORY];         // 从最旧到最新
    const height = Math.min(ms * 2, 66);
    const color = ms < 17 ? { r: 0.3, g: 0.9, b: 0.4, a: 0.9 } : ms < 34 ? { r: 1, g: 0.8, b: 0.2, a: 0.9 } : { r: 1, g: 0.3, b: 0.3, a: 0.9 };
    rect(graphRight - (HISTORY - i) * 2, graphBottom - height, 2, height, color, Space.SCREEN, LAYER_TEXT);
  }
}

function drawControls() {
  const scale = fontScale();
  const lineHeight = font().lineHeight * scale + 6;
  const panel = { r: 0, g: 0, b: 0, a: 0.55 }, title = { r: 1, g: 0.85, b: 0.4, a: 1 };
  const top = 110, gap = 18;                                  // 位于帧时间曲线下方

  if (!showHelp) {
    const hint = tr(T.helpHidden);
    const width = font().measure(hint, scale)[0];
    rect(viewW - width - 40, top - 8, width + 20, lineHeight + 10, panel, Space.SCREEN, LAYER_PANEL);
    drawText(hint, viewW - width - 30, top, title);
    return;
  }

  const states = CONTROLS.map((c) => statusText(c[2]));
  let keyWidth = 0, actionWidth = 0, stateWidth = 0;
  CONTROLS.forEach((c, i) => {
    keyWidth = Math.max(keyWidth, font().measure(tr(c[0]), scale)[0]);
    actionWidth = Math.max(actionWidth, font().measure(tr(c[1]), scale)[0]);
    stateWidth = Math.max(stateWidth, font().measure(states[i].text, scale)[0]);
  });

  const width = Math.max(keyWidth + gap + actionWidth + gap + stateWidth, font().measure(tr(T.helpTitle), scale)[0]);
  const height = lineHeight * (CONTROLS.length + 1);
  const left = viewW - width - 30;
  rect(left - 10, top - 8, width + 20, height + 10, panel, Space.SCREEN, LAYER_PANEL);
  drawText(tr(T.helpTitle), left, top, title);

  CONTROLS.forEach((c, i) => {
    const y = top + lineHeight * (i + 1);
    const keyX = left + keyWidth - font().measure(tr(c[0]), scale)[0];   // 按键列右对齐
    rect(keyX - 4, y - 2, left + keyWidth - keyX + 8, lineHeight - 4, { r: 1, g: 1, b: 1, a: 0.12 }, Space.SCREEN, LAYER_PANEL);
    drawText(tr(c[0]), keyX, y, { r: 0.55, g: 0.85, b: 1, a: 1 });
    drawText(tr(c[1]), left + keyWidth + gap, y, { r: 0.92, g: 0.94, b: 1, a: 1 });
    if (states[i].text) {
      drawText(states[i].text, left + keyWidth + gap + actionWidth + gap, y,
        states[i].active ? { r: 0.4, g: 1, b: 0.5, a: 1 } : { r: 0.6, g: 0.6, b: 0.65, a: 1 });
    }
  });
}

function drawCursor() {
  const color = { r: 1, g: 1, b: 1, a: 0.85 };
  rect(mouseScreen[0] - 8, mouseScreen[1] - 1, 16, 2, color, Space.SCREEN, LAYER_CURSOR);
  rect(mouseScreen[0] - 1, mouseScreen[1] - 8, 2, 16, color, Space.SCREEN, LAYER_CURSOR);
}

/** 开场 LOGO（C API 未提供 splash 接口，这里自行绘制） */
function drawIntro(progress) {
  const [w, h] = engine.textureSize(tex.logo);
  const scale = Math.min(viewW * 0.6 / w, viewH * 0.6 / h);
  const alpha = progress < 0.6 ? 1 : 1 - (progress - 0.6) / 0.4;
  batch.add(tex.sky, 0, 0, viewW, viewH, { space: Space.SCREEN, layer: LAYER_SKY });
  for (const star of stars) {
    batch.add(tex.disc, star.x * viewW, star.y * viewH, star.size, star.size,
      { color: { r: 1, g: 0.95, b: 0.85, a: 0.725 + 0.275 * Math.sin(time * star.speed + star.phase) }, space: Space.SCREEN, layer: LAYER_STARS });
  }
  batch.add(tex.logo, (viewW - w * scale) * 0.5, (viewH - h * scale) * 0.5, w * scale, h * scale,
    { color: { r: 1, g: 1, b: 1, a: clamp01(alpha) }, space: Space.SCREEN, layer: LAYER_CURSOR });
  if (progress > 0.75) {
    const hint = ttfFont ? 'GZX 引擎 Node.js 绑定演示' : 'GZX ENGINE NODE.JS BINDING';
    engine.drawText(font(), hint, (viewW - font().measure(hint, fontScale())[0]) * 0.5, viewH * 0.78,
      { scale: fontScale(), color: { r: 1, g: 1, b: 1, a: clamp01(alpha) }, space: Space.SCREEN, layer: LAYER_CURSOR });
  }
}

// ---------------------------------------------------------------------------
// 自检模式
// ---------------------------------------------------------------------------

/**
 * 绕过窗口输入、按时间片自动驱动游戏逻辑并断言结果，用于验证运行环境是否正常。
 * 用法：GZX_DEMO_SELFTEST=1 node demo.js
 */
const SELFTEST = process.env.GZX_DEMO_SELFTEST === '1';
const testReport = [];
let testPhase = 0, testTimer = 0, testFailures = 0;
let testStart = [0, 0], testBodies = 0, testCoins = 0, testParticles = 0, testY = 0;

function testCheck(name, ok, detail = '') {
  if (!ok) ++testFailures;
  testReport.push(`  ${ok ? '[通过]' : '[失败]'} ${name}${detail ? `  ${detail}` : ''}`);
}

function runTestStep(dt) {
  testTimer += dt;
  if (testTimer < 0.6) return;
  testTimer = 0;

  const pos = world.position(player);
  switch (testPhase) {
    case 0:
      testCheck('重力与着地检测', grounded, `玩家 Y=${pos[1].toFixed(0)}`);
      testStart = pos.slice();
      world.setVelocity(player, PLAYER_SPEED, 0);
      break;
    case 1:
      testCheck('水平移动', pos[0] - testStart[0] > 100, `位移 ${(pos[0] - testStart[0]).toFixed(0)} 像素`);
      testY = pos[1];
      world.setVelocity(player, 0, -JUMP_SPEED);
      break;
    case 2:
      testCheck('跳跃上升', pos[1] < testY, `Y ${testY.toFixed(0)} -> ${pos[1].toFixed(0)}`);
      testParticles = particles.length;
      emitBurst(pos[0], pos[1], 60, 300, { r: 1, g: 0.8, b: 0.3, a: 1 }, 1.0);
      break;
    case 3:
      testCheck('粒子发射', particles.length > testParticles, `新增 ${particles.length - testParticles} 个`);
      testBodies = world.bodyCount;
      spawnProp(false, pos[0] + 60, pos[1] - 60);
      break;
    case 4:
      testCheck('运行期生成刚体', world.bodyCount > testBodies, `刚体 ${testBodies} -> ${world.bodyCount}`);
      testCoins = coins;
      {   // 把玩家瞬移到第一枚金币处，验证接触事件与拾取
        const coin = bodies.find((b) => b.kind === Kind.Coin && world.valid(b.handle));
        if (coin) { const c = world.position(coin.handle); world.setPosition(player, c[0], c[1]); }
      }
      break;
    case 5:
      testCheck('金币拾取接触事件', coins > testCoins, `金币 ${testCoins} -> ${coins}`);
      hurt();
      testCheck('受伤状态机', hurting, '');
      break;
    case 6:
      storm = true;
      break;
    case 7:
      testCheck('粒子风暴压力测试', particles.length > 3000, `当前粒子 ${particles.length}  帧时间 ${frameMs.toFixed(2)} 毫秒`);
      testCheck('渲染批次提交', batch.count > 1000, `本帧精灵 ${batch.count}`);
      testCheck('音频子系统', !engine.audioAvailable || audio.music !== 0, engine.audioAvailable ? '已合成 6 个音效' : '无音频设备（已跳过）');
      engine.requestQuit();
      break;
  }
  ++testPhase;
}

// ---------------------------------------------------------------------------
// 主循环
// ---------------------------------------------------------------------------
const INTRO_SECONDS = 1.4;
let introTime = 0;

console.log(`贴图目录: ${ASSETS}`);
if (SELFTEST) {
  console.log('自检模式：自动验证物理、拾取、粒子与音频\n');
} else {
  console.log('ESC 退出 / WASD 移动 / J 攻击 / H 帮助 / P 粒子风暴 / F1 排序 / F2 碰撞框');
}

engine.run((dt) => {
  // --- 帧时间统计 ---
  const now = process.hrtime.bigint();
  const ms = Number(now - lastDraw) / 1e6;
  lastDraw = now;
  if (ms > 0 && ms < 1000) {
    frameMs += (ms - frameMs) * 0.05;
    history[historyIndex] = ms;
    historyIndex = (historyIndex + 1) % HISTORY;
  }

  const [vw, vh] = engine.viewport;
  viewW = vw; viewH = vh;

  // --- 开场 ---
  if (introTime < INTRO_SECONDS && !SELFTEST) {
    introTime += dt;
    time += dt;
    batch.clear();
    drawIntro(introTime / INTRO_SECONDS);
    engine.drawSprites(batch);
    if (engine.keyPressed(Key.ESCAPE) || engine.keyPressed(Key.SPACE)) introTime = INTRO_SECONDS;
    return;
  }

  time += dt;
  world.setVelocity(mover, Math.cos(time * 0.8) * 140, 0);   // 运动学平台按速度往返移动

  if (SELFTEST) {
    runTestStep(dt);          // 自检模式：脚本驱动，不读取窗口输入
    updatePlayer(dt, true);   // 仍更新着地检测与动画，但不接管速度以便脚本操控
    updateSandbox();          // 自检后期会开启粒子风暴，需要走这里生成粒子
    if (engine.keyPressed(Key.ESCAPE)) { engine.requestQuit(); return; }
  } else {
    if (engine.keyPressed(Key.ESCAPE)) { engine.requestQuit(); return; }
    handleToggles();

    mouseScreen = engine.mousePosition;
    const wheel = engine.mouseWheel;
    if (wheel[1] !== 0) targetZoom = clamp(targetZoom * Math.pow(1.1, clamp(wheel[1], -10, 10)), 0.5, 3);
    mouseWorld = engine.screenToWorld(mouseScreen[0], mouseScreen[1]);

    updatePlayer(dt);
    updateSandbox();
  }

  world.step(dt, 2, 6);                                      // 两个子步提高堆叠稳定性
  handleContacts();
  updateParticles(dt);
  updateCamera(dt);

  // --- 绘制 ---
  engine.setBackground({ r: 0.05, g: 0.06, b: 0.15, a: 1 });
  engine.setSort(sortMode);
  let camX = cameraX, camY = cameraY;
  if (shake > 0) {                                           // 屏幕震动只影响渲染相机，不影响鼠标换算
    camX += random.range(-1, 1) * shake * 10;
    camY += random.range(-1, 1) * shake * 10;
  }
  engine.setCamera(camX, camY, cameraZoom);

  viewLeft = camX - 64; viewTop = camY - 64;
  viewRight = camX + viewW / cameraZoom + 64; viewBottom = camY + viewH / cameraZoom + 64;

  batch.clear();
  drawBackground();
  drawWorld();
  drawParticles();
  drawLaser();
  drawPlayer();
  if (debug) drawColliders();
  drawHud();
  drawControls();
  drawCursor();
  engine.drawSprites(batch);

  // 鼠标拾取：高亮命中的精灵（C API 无法改写已提交精灵，改用外框指示）
  const index = engine.pick(mouseScreen[0], mouseScreen[1]);
  pickedInfo = index === null ? null : { index, layer: '?' };
});

if (SELFTEST) {
  console.log('=== 自检结果 ===');
  console.log(testReport.join('\n'));
  console.log(testFailures === 0 ? `\n全部 ${testReport.length} 项通过，运行环境正常` : `\n${testFailures} / ${testReport.length} 项失败`);
  process.exitCode = testFailures === 0 ? 0 : 1;
}
