'use strict';
/**
 * GZX 2D 游戏引擎的 Node.js 接口，基于 koffi 调用引擎动态库。
 * Windows 加载 engine.dll，Linux 加载 libengine.so。
 * 安装依赖：npm install koffi
 * 可用环境变量 GZX_LIBRARY 指定动态库绝对路径。
 */
const koffi = require('koffi');
const path = require('path');

const LIBRARY_NAME = process.platform === 'win32' ? 'engine.dll' : 'libengine.so';

// 按SDK布局逐级回退查找动态库：环境变量 -> 同目录 -> SDK bin/ -> 上层 bin/ -> 系统路径
function findLibrary() {
  if (process.env.GZX_LIBRARY) return process.env.GZX_LIBRARY;
  const candidates = [
    path.join(__dirname, LIBRARY_NAME),
    path.join(__dirname, '..', '..', 'bin', LIBRARY_NAME),
    path.join(__dirname, '..', 'bin', LIBRARY_NAME),
    path.join(__dirname, '..', '..', 'build-release', LIBRARY_NAME),
    path.join(__dirname, '..', 'build', LIBRARY_NAME),
  ];
  for (const candidate of candidates) {
    if (require('fs').existsSync(candidate)) return candidate;
  }
  return LIBRARY_NAME;  // 最后交给系统搜索路径
}

const lib = koffi.load(findLibrary());

const INVALID_ID = 0xFFFFFFFF;
const INVALID_HANDLE = 0xFFFFFFFFFFFFFFFFn;
const WHITE_TEXTURE = 0;
const SPRITE_BYTES = 72;   // 与 C 端 GzxSprite 布局一致
const SPRITE_SLOTS = 18;   // 每个精灵 18 个 4 字节字段

/** 键盘按键 */
const Key = Object.freeze({
  LEFT: 0, RIGHT: 1, UP: 2, DOWN: 3, SPACE: 4, ESCAPE: 5, ENTER: 6, TAB: 7, BACKSPACE: 8,
  LEFT_SHIFT: 9, RIGHT_SHIFT: 10, LEFT_CTRL: 11, RIGHT_CTRL: 12, LEFT_ALT: 13, RIGHT_ALT: 14,
  ...Object.fromEntries([...'ABCDEFGHIJKLMNOPQRSTUVWXYZ'].map((c, i) => [c, 15 + i])),
  ...Object.fromEntries(Array.from({ length: 10 }, (_, i) => [`NUM${i}`, 41 + i])),
  ...Object.fromEntries(Array.from({ length: 12 }, (_, i) => [`F${i + 1}`, 51 + i])),
});
/** 鼠标按键 */
const Mouse = Object.freeze({ LEFT: 0, MIDDLE: 1, RIGHT: 2, X1: 3, X2: 4 });
/** 坐标空间 */
const Space = Object.freeze({ WORLD: 0, SCREEN: 1 });
/** 精灵排序模式 */
const Sort = Object.freeze({ NONE: 0, LAYER: 1, LAYER_TEXTURE: 2 });
/** 刚体运动类型 */
const BodyType = Object.freeze({ STATIC: 0, DYNAMIC: 1, KINEMATIC: 2 });
/** 刚体碰撞形状 */
const Shape = Object.freeze({ BOX: 0, CIRCLE: 1 });

/**
 * @typedef {{r: number, g: number, b: number, a?: number}} Color RGBA 颜色，各分量 0 到 1
 * @typedef {{left: number, top: number, right: number, bottom: number}} Uv 纹理 UV 区域
 * @typedef {{a: bigint, b: bigint, normalX: number, normalY: number, depth: number, sensor: boolean}} Contact 物理接触
 * @typedef {{body: bigint, x: number, y: number, normalX: number, normalY: number, fraction: number}} RayHit 射线命中信息
 */

const WHITE = Object.freeze({ r: 1, g: 1, b: 1, a: 1 });
const FULL_UV = Object.freeze({ left: 0, top: 0, right: 1, bottom: 1 });

// ---------------------------------------------------------------------------
// C 类型与函数
// ---------------------------------------------------------------------------

koffi.opaque('GzxEngine');
koffi.opaque('GzxFont');
koffi.opaque('GzxWorld');
koffi.struct('GzxColor', { r: 'float', g: 'float', b: 'float', a: 'float' });
koffi.struct('GzxUv', { left: 'float', top: 'float', right: 'float', bottom: 'float' });
koffi.struct('GzxConfig', { title: 'const char *', width: 'int32_t', height: 'int32_t', resizable: 'int32_t', pause_when_minimized: 'int32_t' });
koffi.struct('GzxBodyDef', {
  type: 'int32_t', shape: 'int32_t', x: 'float', y: 'float', half_width: 'float', half_height: 'float', radius: 'float',
  velocity_x: 'float', velocity_y: 'float', mass: 'float', restitution: 'float', friction: 'float',
  linear_damping: 'float', gravity_scale: 'float', category: 'uint32_t', mask: 'uint32_t', sensor: 'int32_t', user_data: 'uint64_t',
});
koffi.struct('GzxRayHit', { body: 'uint64_t', x: 'float', y: 'float', normal_x: 'float', normal_y: 'float', fraction: 'float' });

const c = {
  lastError: lib.func('const char *gzx_last_error(void)'),
  create: lib.func('GzxEngine *gzx_create(const GzxConfig *config)'),
  destroy: lib.func('void gzx_destroy(GzxEngine *engine)'),
  beginFrame: lib.func('int gzx_begin_frame(GzxEngine *engine)'),
  endFrame: lib.func('void gzx_end_frame(GzxEngine *engine)'),
  deltaTime: lib.func('float gzx_delta_time(GzxEngine *engine)'),
  requestQuit: lib.func('void gzx_request_quit(GzxEngine *engine)'),
  viewport: lib.func('void gzx_viewport(GzxEngine *engine, _Out_ int32_t *width, _Out_ int32_t *height)'),
  keyDown: lib.func('int gzx_key_down(GzxEngine *engine, int32_t key)'),
  keyPressed: lib.func('int gzx_key_pressed(GzxEngine *engine, int32_t key)'),
  keyReleased: lib.func('int gzx_key_released(GzxEngine *engine, int32_t key)'),
  mouseDown: lib.func('int gzx_mouse_down(GzxEngine *engine, int32_t button)'),
  mousePressed: lib.func('int gzx_mouse_pressed(GzxEngine *engine, int32_t button)'),
  mouseReleased: lib.func('int gzx_mouse_released(GzxEngine *engine, int32_t button)'),
  mousePosition: lib.func('void gzx_mouse_position(GzxEngine *engine, _Out_ float *x, _Out_ float *y)'),
  mouseWheel: lib.func('void gzx_mouse_wheel(GzxEngine *engine, _Out_ float *x, _Out_ float *y)'),
  loadTexture: lib.func('uint32_t gzx_load_texture(GzxEngine *engine, const char *path)'),
  createTexture: lib.func('uint32_t gzx_create_texture(GzxEngine *engine, uint32_t width, uint32_t height, const void *rgba)'),
  textureSize: lib.func('int gzx_texture_size(GzxEngine *engine, uint32_t texture, _Out_ uint32_t *width, _Out_ uint32_t *height)'),
  textureRegion: lib.func('GzxUv gzx_texture_region(GzxEngine *engine, uint32_t texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height)'),
  sheetUv: lib.func('GzxUv gzx_sheet_uv(uint32_t columns, uint32_t rows, uint32_t frame)'),
  setBackground: lib.func('void gzx_set_background(GzxEngine *engine, GzxColor color)'),
  setCamera: lib.func('void gzx_set_camera(GzxEngine *engine, float x, float y, float zoom)'),
  setSort: lib.func('void gzx_set_sort(GzxEngine *engine, int32_t mode)'),
  drawSprites: lib.func('void gzx_draw_sprites(GzxEngine *engine, const void *sprites, size_t count)'),
  drawRect: lib.func('void gzx_draw_rect(GzxEngine *engine, float x, float y, float width, float height, GzxColor color, int32_t space, int32_t layer)'),
  spriteCount: lib.func('size_t gzx_sprite_count(GzxEngine *engine)'),
  pick: lib.func('int64_t gzx_pick(GzxEngine *engine, float x, float y)'),
  screenToWorld: lib.func('void gzx_screen_to_world(GzxEngine *engine, float x, float y, _Out_ float *wx, _Out_ float *wy)'),
  worldToScreen: lib.func('void gzx_world_to_screen(GzxEngine *engine, float x, float y, _Out_ float *sx, _Out_ float *sy)'),
  fontDefault: lib.func('GzxFont *gzx_font_default(GzxEngine *engine)'),
  fontGrid: lib.func('GzxFont *gzx_font_grid(GzxEngine *engine, uint32_t texture, uint32_t cw, uint32_t ch, uint32_t first, uint32_t count)'),
  fontTtf: lib.func('GzxFont *gzx_font_ttf(GzxEngine *engine, const char *path, float size, const char *characters)'),
  fontDestroy: lib.func('void gzx_font_destroy(GzxFont *font)'),
  drawText: lib.func('void gzx_draw_text(GzxEngine *engine, GzxFont *font, const char *text, float x, float y, float scale, GzxColor color, int32_t space, int32_t layer)'),
  measureText: lib.func('void gzx_measure_text(GzxFont *font, const char *text, float scale, _Out_ float *w, _Out_ float *h)'),
  fontLineHeight: lib.func('float gzx_font_line_height(GzxFont *font)'),
  audioAvailable: lib.func('int gzx_audio_available(GzxEngine *engine)'),
  loadSound: lib.func('uint32_t gzx_load_sound(GzxEngine *engine, const char *path)'),
  createSound: lib.func('uint32_t gzx_create_sound(GzxEngine *engine, const void *samples, size_t count, uint32_t channels, uint32_t rate)'),
  play: lib.func('uint64_t gzx_play(GzxEngine *engine, uint32_t sound, float volume, float pan, int loop)'),
  stop: lib.func('void gzx_stop(GzxEngine *engine, uint64_t voice)'),
  stopAll: lib.func('void gzx_stop_all(GzxEngine *engine)'),
  playing: lib.func('int gzx_playing(GzxEngine *engine, uint64_t voice)'),
  setVoiceVolume: lib.func('void gzx_set_voice_volume(GzxEngine *engine, uint64_t voice, float volume)'),
  setVoicePan: lib.func('void gzx_set_voice_pan(GzxEngine *engine, uint64_t voice, float pan)'),
  setMasterVolume: lib.func('void gzx_set_master_volume(GzxEngine *engine, float volume)'),
  worldCreate: lib.func('GzxWorld *gzx_world_create(float gx, float gy, float cell)'),
  worldDestroy: lib.func('void gzx_world_destroy(GzxWorld *world)'),
  createBody: lib.func('uint64_t gzx_world_create_body(GzxWorld *world, const GzxBodyDef *definition)'),
  destroyBody: lib.func('void gzx_world_destroy_body(GzxWorld *world, uint64_t body)'),
  valid: lib.func('int gzx_world_valid(GzxWorld *world, uint64_t body)'),
  position: lib.func('void gzx_world_position(GzxWorld *world, uint64_t body, _Out_ float *x, _Out_ float *y)'),
  setPosition: lib.func('void gzx_world_set_position(GzxWorld *world, uint64_t body, float x, float y)'),
  velocity: lib.func('void gzx_world_velocity(GzxWorld *world, uint64_t body, _Out_ float *x, _Out_ float *y)'),
  setVelocity: lib.func('void gzx_world_set_velocity(GzxWorld *world, uint64_t body, float x, float y)'),
  applyImpulse: lib.func('void gzx_world_apply_impulse(GzxWorld *world, uint64_t body, float x, float y)'),
  userData: lib.func('uint64_t gzx_world_user_data(GzxWorld *world, uint64_t body)'),
  setGravity: lib.func('void gzx_world_set_gravity(GzxWorld *world, float x, float y)'),
  step: lib.func('void gzx_world_step(GzxWorld *world, float dt, int32_t substeps, int32_t iterations)'),
  contacts: lib.func('size_t gzx_world_contacts(GzxWorld *world, void *out, size_t capacity)'),
  queryPoint: lib.func('size_t gzx_world_query_point(GzxWorld *world, float x, float y, uint32_t mask, void *out, size_t capacity)'),
  queryRect: lib.func('size_t gzx_world_query_rect(GzxWorld *world, float l, float t, float r, float b, uint32_t mask, void *out, size_t capacity)'),
  raycast: lib.func('int gzx_world_raycast(GzxWorld *world, float x0, float y0, float x1, float y1, uint32_t mask, _Out_ GzxRayHit *hit)'),
  positions: lib.func('void gzx_world_positions(GzxWorld *world, const void *bodies, size_t count, void *out)'),
  bodyCount: lib.func('size_t gzx_world_body_count(GzxWorld *world)'),
};

/** 引擎调用失败时抛出，信息来自 gzx_last_error */
class GzxError extends Error {
  constructor() {
    super(c.lastError());
    this.name = 'GzxError';
  }
}

const toColor = (color) => ({ r: color.r, g: color.g, b: color.b, a: color.a ?? 1 });
const out2 = (fn, ...args) => {
  const x = [0], y = [0];
  fn(...args, x, y);
  return [x[0], y[0]];
};

// ---------------------------------------------------------------------------
// 批量精灵
// ---------------------------------------------------------------------------

/** 可复用的精灵缓冲区，大量精灵时比逐个 drawSprite 快得多 */
class SpriteBatch {
  /** @param {number} [capacity=1024] 初始容量 */
  constructor(capacity = 1024) {
    this.count = 0;
    this._allocate(Math.max(1, capacity));
  }

  _allocate(capacity) {
    const buffer = new ArrayBuffer(capacity * SPRITE_BYTES);
    if (this.f32) new Uint8Array(buffer).set(new Uint8Array(this.f32.buffer));
    this.f32 = new Float32Array(buffer);
    this.u32 = new Uint32Array(buffer);
    this.i32 = new Int32Array(buffer);
    this.capacity = capacity;
  }

  /** 清空缓冲区，保留容量 */
  clear() {
    this.count = 0;
  }

  /**
   * 追加一个精灵
   * @param {number} texture 纹理 ID
   * @param {number} x 左上角横坐标
   * @param {number} y 左上角纵坐标
   * @param {number} width 宽度
   * @param {number} height 高度
   * @param {{uv?: Uv, color?: Color, rotation?: number, pivotX?: number, pivotY?: number, layer?: number, space?: number}} [options]
   */
  add(texture, x, y, width, height, options = {}) {
    if (this.count === this.capacity) this._allocate(this.capacity * 2);
    const uv = options.uv ?? FULL_UV, color = options.color ?? WHITE;
    const o = this.count++ * SPRITE_SLOTS;
    this.u32[o] = texture;
    this.f32[o + 1] = x; this.f32[o + 2] = y; this.f32[o + 3] = width; this.f32[o + 4] = height;
    this.f32[o + 5] = color.r; this.f32[o + 6] = color.g; this.f32[o + 7] = color.b; this.f32[o + 8] = color.a ?? 1;
    this.f32[o + 9] = uv.left; this.f32[o + 10] = uv.top; this.f32[o + 11] = uv.right; this.f32[o + 12] = uv.bottom;
    this.i32[o + 13] = options.space ?? Space.WORLD;
    this.f32[o + 14] = options.rotation ?? 0;
    this.f32[o + 15] = options.pivotX ?? 0.5; this.f32[o + 16] = options.pivotY ?? 0.5;
    this.i32[o + 17] = options.layer ?? 0;
  }
}

// ---------------------------------------------------------------------------
// 字体
// ---------------------------------------------------------------------------

/** 位图字体，通过 Engine.defaultFont / gridFont / ttfFont 创建 */
class Font {
  constructor(handle) {
    this._handle = handle;
  }

  /** 释放字体 */
  close() {
    if (this._handle) c.fontDestroy(this._handle);
    this._handle = null;
  }

  /**
   * 测量文本绘制后的尺寸
   * @param {string} text 文本
   * @param {number} [scale=1] 缩放
   * @returns {[number, number]} [宽, 高]
   */
  measure(text, scale = 1) {
    return out2(c.measureText, this._handle, text, scale);
  }

  /** @returns {number} 行高（像素） */
  get lineHeight() {
    return c.fontLineHeight(this._handle);
  }
}

// ---------------------------------------------------------------------------
// 引擎
// ---------------------------------------------------------------------------

/** 引擎实例，一个实例对应一个窗口，必须在主线程使用 */
class Engine {
  /**
   * @param {{title?: string, width?: number, height?: number, resizable?: boolean, pauseWhenMinimized?: boolean}} [options]
   */
  constructor(options = {}) {
    this._handle = c.create({
      title: options.title ?? 'GZX Engine',
      width: options.width ?? 1280,
      height: options.height ?? 720,
      resizable: options.resizable ?? true ? 1 : 0,
      pause_when_minimized: options.pauseWhenMinimized ?? true ? 1 : 0,
    });
    if (!this._handle) throw new GzxError();
    this._single = new SpriteBatch(1);
  }

  /** 销毁引擎和窗口 */
  close() {
    if (this._handle) c.destroy(this._handle);
    this._handle = null;
  }

  /** @returns {boolean} 开始新的一帧，false 表示应退出 */
  beginFrame() {
    return c.beginFrame(this._handle) !== 0;
  }

  /** 渲染本帧提交的内容 */
  endFrame() {
    c.endFrame(this._handle);
  }

  /**
   * 运行主循环直到窗口关闭，退出后自动销毁引擎
   * @param {(dt: number) => void} frame 每帧回调，参数为时间增量（秒）
   */
  run(frame) {
    try {
      while (this.beginFrame()) {
        frame(this.deltaTime);
        this.endFrame();
      }
    } finally {
      this.close();
    }
  }

  /** @returns {number} 本帧时间增量（秒），最大 0.25 */
  get deltaTime() {
    return c.deltaTime(this._handle);
  }

  /** 请求退出 */
  requestQuit() {
    c.requestQuit(this._handle);
  }

  /** @returns {[number, number]} 视口尺寸 [宽, 高] */
  get viewport() {
    return out2(c.viewport, this._handle);
  }

  /** @param {number} key Key.* @returns {boolean} 按键当前是否按下 */
  keyDown(key) { return c.keyDown(this._handle, key) !== 0; }
  /** @param {number} key Key.* @returns {boolean} 按键是否在本帧按下 */
  keyPressed(key) { return c.keyPressed(this._handle, key) !== 0; }
  /** @param {number} key Key.* @returns {boolean} 按键是否在本帧释放 */
  keyReleased(key) { return c.keyReleased(this._handle, key) !== 0; }
  /** @param {number} button Mouse.* @returns {boolean} 鼠标按键当前是否按下 */
  mouseDown(button) { return c.mouseDown(this._handle, button) !== 0; }
  /** @param {number} button Mouse.* @returns {boolean} 鼠标按键是否在本帧按下 */
  mousePressed(button) { return c.mousePressed(this._handle, button) !== 0; }
  /** @param {number} button Mouse.* @returns {boolean} 鼠标按键是否在本帧释放 */
  mouseReleased(button) { return c.mouseReleased(this._handle, button) !== 0; }
  /** @returns {[number, number]} 鼠标在窗口中的位置 */
  get mousePosition() { return out2(c.mousePosition, this._handle); }
  /** @returns {[number, number]} 本帧滚轮滚动量 [水平, 垂直] */
  get mouseWheel() { return out2(c.mouseWheel, this._handle); }

  /**
   * 加载图片纹理，重复加载同一路径返回同一 ID
   * @param {string} file 图片路径
   * @returns {number} 纹理 ID
   */
  loadTexture(file) {
    const texture = c.loadTexture(this._handle, file);
    if (texture === INVALID_ID) throw new GzxError();
    return texture;
  }

  /**
   * 从 RGBA8 像素创建纹理
   * @param {number} width 宽度
   * @param {number} height 高度
   * @param {Uint8Array} rgba 长度为 width * height * 4 的像素数据
   * @returns {number} 纹理 ID
   */
  createTexture(width, height, rgba) {
    if (rgba.byteLength !== width * height * 4) throw new RangeError('rgba 长度必须为 width * height * 4');
    const texture = c.createTexture(this._handle, width, height, rgba);
    if (texture === INVALID_ID) throw new GzxError();
    return texture;
  }

  /** @param {number} texture @returns {[number, number]} 纹理尺寸 [宽, 高] */
  textureSize(texture) {
    const w = [0], h = [0];
    if (!c.textureSize(this._handle, texture, w, h)) throw new GzxError();
    return [w[0], h[0]];
  }

  /** @returns {Uv} 纹理中像素区域对应的 UV */
  textureRegion(texture, x, y, width, height) {
    return c.textureRegion(this._handle, texture, x, y, width, height);
  }

  /** @returns {Uv} 网格精灵表第 frame 帧的 UV，帧按行优先编号 */
  static sheetUv(columns, rows, frame) {
    return c.sheetUv(columns, rows, frame);
  }

  /** @param {Color} color 本帧背景颜色 */
  setBackground(color) { c.setBackground(this._handle, toColor(color)); }
  /** 设置本帧相机，(x, y) 为视野左上角的世界坐标 */
  setCamera(x, y, zoom = 1) { c.setCamera(this._handle, x, y, zoom); }
  /** @param {number} mode Sort.* */
  setSort(mode) { c.setSort(this._handle, mode); }

  /**
   * 绘制一个精灵，大量精灵请使用 SpriteBatch + drawSprites
   * @param {number} texture 纹理 ID
   * @param {number} x @param {number} y @param {number} width @param {number} height
   * @param {{uv?: Uv, color?: Color, rotation?: number, pivotX?: number, pivotY?: number, layer?: number, space?: number}} [options]
   */
  drawSprite(texture, x, y, width, height, options) {
    this._single.clear();
    this._single.add(texture, x, y, width, height, options);
    c.drawSprites(this._handle, this._single.f32, 1);
  }

  /** @param {SpriteBatch} batch 一次提交整个精灵缓冲区 */
  drawSprites(batch) {
    if (batch.count) c.drawSprites(this._handle, batch.f32, batch.count);
  }

  /**
   * 绘制纯色矩形
   * @param {Color} [color=WHITE] @param {number} [space=Space.WORLD] @param {number} [layer=0]
   */
  drawRect(x, y, width, height, color = WHITE, space = Space.WORLD, layer = 0) {
    c.drawRect(this._handle, x, y, width, height, toColor(color), space, layer);
  }

  /** @returns {number} 本帧已提交的精灵数量 */
  get spriteCount() { return Number(c.spriteCount(this._handle)); }

  /** @returns {number | null} 屏幕坐标处最上层精灵的提交序号 */
  pick(x, y) {
    const index = Number(c.pick(this._handle, x, y));
    return index < 0 ? null : index;
  }

  /** @returns {[number, number]} 屏幕坐标转世界坐标 */
  screenToWorld(x, y) { return out2(c.screenToWorld, this._handle, x, y); }
  /** @returns {[number, number]} 世界坐标转屏幕坐标 */
  worldToScreen(x, y) { return out2(c.worldToScreen, this._handle, x, y); }

  /** @returns {Font} 内置 8x8 ASCII 像素字体 */
  defaultFont() { return this._font(c.fontDefault(this._handle)); }
  /** @returns {Font} 从网格字体图集创建字体 */
  gridFont(texture, cellWidth, cellHeight, first = 32, count = 95) {
    return this._font(c.fontGrid(this._handle, texture, cellWidth, cellHeight, first, count));
  }
  /**
   * 将 TTF/OTF 字体烘焙为图集
   * @param {string} file 字体路径 @param {number} pixelSize 字号 @param {string} [characters] 额外字符（如中文）
   * @returns {Font}
   */
  ttfFont(file, pixelSize, characters = '') {
    return this._font(c.fontTtf(this._handle, file, pixelSize, characters));
  }
  _font(handle) {
    if (!handle) throw new GzxError();
    return new Font(handle);
  }

  /**
   * 绘制 UTF-8 文本，支持换行
   * @param {Font} font @param {string} text @param {number} x @param {number} y
   * @param {{scale?: number, color?: Color, space?: number, layer?: number}} [options]
   */
  drawText(font, text, x, y, options = {}) {
    c.drawText(this._handle, font._handle, text, x, y, options.scale ?? 1, toColor(options.color ?? WHITE),
               options.space ?? Space.SCREEN, options.layer ?? 0);
  }

  /** @returns {boolean} 音频设备是否可用 */
  get audioAvailable() { return c.audioAvailable(this._handle) !== 0; }

  /** @param {string} file 声音文件（WAV/OGG/MP3/FLAC） @returns {number} 声音 ID */
  loadSound(file) {
    const sound = c.loadSound(this._handle, file);
    if (sound === INVALID_ID) throw new GzxError();
    return sound;
  }

  /**
   * 从浮点采样创建声音
   * @param {Float32Array} samples 交错排列的采样，-1 到 1
   * @param {number} [channels=1] @param {number} [sampleRate=48000]
   * @returns {number} 声音 ID
   */
  createSound(samples, channels = 1, sampleRate = 48000) {
    const sound = c.createSound(this._handle, samples, samples.length, channels, sampleRate);
    if (sound === INVALID_ID) throw new GzxError();
    return sound;
  }

  /**
   * 播放声音
   * @param {number} sound 声音 ID
   * @param {{volume?: number, pan?: number, loop?: boolean}} [options]
   * @returns {bigint | null} 声部句柄，设备不可用或声部已满时为 null
   */
  play(sound, options = {}) {
    const voice = BigInt(c.play(this._handle, sound, options.volume ?? 1, options.pan ?? 0, options.loop ? 1 : 0));
    return voice === INVALID_HANDLE ? null : voice;
  }

  /** @param {bigint} voice */
  stop(voice) { c.stop(this._handle, voice); }
  stopAll() { c.stopAll(this._handle); }
  /** @param {bigint} voice @returns {boolean} */
  playing(voice) { return c.playing(this._handle, voice) !== 0; }
  /** @param {bigint} voice @param {number} volume */
  setVoiceVolume(voice, volume) { c.setVoiceVolume(this._handle, voice, volume); }
  /** @param {bigint} voice @param {number} pan -1 最左，1 最右 */
  setVoicePan(voice, pan) { c.setVoicePan(this._handle, voice, pan); }
  /** @param {number} volume 主音量 */
  setMasterVolume(volume) { c.setMasterVolume(this._handle, volume); }
}

// ---------------------------------------------------------------------------
// 物理
// ---------------------------------------------------------------------------

/** 二维物理世界，y 轴向下，可独立于 Engine 使用 */
class World {
  constructor(gravityX = 0, gravityY = 980, cellSize = 64) {
    this._handle = c.worldCreate(gravityX, gravityY, cellSize);
    if (!this._handle) throw new GzxError();
    this._query = new BigUint64Array(256);
  }

  /** 销毁物理世界 */
  close() {
    if (this._handle) c.worldDestroy(this._handle);
    this._handle = null;
  }

  /**
   * 创建刚体
   * @param {{x: number, y: number, shape?: number, type?: number, halfWidth?: number, halfHeight?: number, radius?: number,
   *          velocityX?: number, velocityY?: number, mass?: number, restitution?: number, friction?: number,
   *          linearDamping?: number, gravityScale?: number, category?: number, mask?: number, sensor?: boolean, userData?: bigint}} d
   * @returns {bigint} 刚体句柄
   */
  createBody(d) {
    const body = BigInt(c.createBody(this._handle, {
      type: d.type ?? BodyType.DYNAMIC, shape: d.shape ?? Shape.BOX, x: d.x, y: d.y,
      half_width: d.halfWidth ?? 16, half_height: d.halfHeight ?? 16, radius: d.radius ?? 16,
      velocity_x: d.velocityX ?? 0, velocity_y: d.velocityY ?? 0, mass: d.mass ?? 1, restitution: d.restitution ?? 0,
      friction: d.friction ?? 0.2, linear_damping: d.linearDamping ?? 0, gravity_scale: d.gravityScale ?? 1,
      category: d.category ?? 1, mask: d.mask ?? 0xFFFFFFFF, sensor: d.sensor ? 1 : 0, user_data: d.userData ?? 0n,
    }));
    if (body === INVALID_HANDLE) throw new GzxError();
    return body;
  }

  /** @param {bigint} body */
  destroyBody(body) { c.destroyBody(this._handle, body); }
  /** @param {bigint} body @returns {boolean} */
  valid(body) { return c.valid(this._handle, body) !== 0; }
  /** @param {bigint} body @returns {[number, number]} 刚体中心位置 */
  position(body) { return out2(c.position, this._handle, body); }
  /** @param {bigint} body */
  setPosition(body, x, y) { c.setPosition(this._handle, body, x, y); }
  /** @param {bigint} body @returns {[number, number]} 刚体速度 */
  velocity(body) { return out2(c.velocity, this._handle, body); }
  /** @param {bigint} body */
  setVelocity(body, x, y) { c.setVelocity(this._handle, body, x, y); }
  /** @param {bigint} body */
  applyImpulse(body, x, y) { c.applyImpulse(this._handle, body, x, y); }
  /** @param {bigint} body @returns {bigint} 用户数据 */
  userData(body) { return BigInt(c.userData(this._handle, body)); }
  setGravity(x, y) { c.setGravity(this._handle, x, y); }
  /** 推进模拟 */
  step(dt, substeps = 1, iterations = 4) { c.step(this._handle, dt, substeps, iterations); }

  /** @returns {Contact[]} 最近一次步进产生的接触 */
  contacts() {
    const total = Number(c.contacts(this._handle, null, 0));
    const view = new DataView(new ArrayBuffer(total * 32));  // GzxContact 为 32 字节
    c.contacts(this._handle, view.buffer, total);
    const result = new Array(total);
    for (let i = 0; i < total; ++i) {
      const o = i * 32;
      result[i] = {
        a: view.getBigUint64(o, true), b: view.getBigUint64(o + 8, true),
        normalX: view.getFloat32(o + 16, true), normalY: view.getFloat32(o + 20, true),
        depth: view.getFloat32(o + 24, true), sensor: view.getInt32(o + 28, true) !== 0,
      };
    }
    return result;
  }

  _collect(fn, ...args) {
    let total = Number(fn(this._handle, ...args, this._query, this._query.length));
    if (total > this._query.length) {
      this._query = new BigUint64Array(total);
      fn(this._handle, ...args, this._query, total);
    }
    return Array.from(this._query.subarray(0, total));
  }

  /** @returns {bigint[]} 包含该点的刚体 */
  queryPoint(x, y, mask = 0xFFFFFFFF) { return this._collect(c.queryPoint, x, y, mask); }
  /** @returns {bigint[]} 与矩形重叠的刚体 */
  queryRect(left, top, right, bottom, mask = 0xFFFFFFFF) { return this._collect(c.queryRect, left, top, right, bottom, mask); }

  /** @returns {RayHit | null} 最近的射线命中 */
  raycast(x0, y0, x1, y1, mask = 0xFFFFFFFF) {
    const hit = {};
    if (!c.raycast(this._handle, x0, y0, x1, y1, mask, hit)) return null;
    return { body: BigInt(hit.body), x: hit.x, y: hit.y, normalX: hit.normal_x, normalY: hit.normal_y, fraction: hit.fraction };
  }

  /**
   * 批量读取刚体位置
   * @param {BigUint64Array} bodies 刚体句柄
   * @param {Float32Array} [out] 输出，长度至少为 bodies.length * 2
   * @returns {Float32Array} 按 x, y 交错排列的位置
   */
  positions(bodies, out = new Float32Array(bodies.length * 2)) {
    if (bodies.length) c.positions(this._handle, bodies, bodies.length, out);
    return out;
  }

  /** @returns {number} 存活的刚体数量 */
  get bodyCount() { return Number(c.bodyCount(this._handle)); }
}

module.exports = {
  Engine, World, Font, SpriteBatch, GzxError,
  Key, Mouse, Space, Sort, BodyType, Shape, WHITE, FULL_UV, WHITE_TEXTURE,
};
