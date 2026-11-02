#include "../../include/gzx.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../../include/engine/engine.hpp"
#include "../platform/window.hpp"
#include "../render/memory/gpu_allocator.hpp"
#include "../render/renderer.hpp"
#include "../render/vulkan/device_selection.hpp"
#include "../render/vulkan/vulkan_context.hpp"
#include "../render/vulkan/vulkan_device.hpp"
#include "SDL3/SDL.h"

static_assert(std::is_trivially_copyable_v<engine::Sprite>, "Sprite must be trivially copyable");
static_assert(sizeof(GzxSprite) == sizeof(engine::Sprite), "GzxSprite must match engine::Sprite");  // 布局一致时批量提交可直接整块拷贝
static_assert(offsetof(GzxSprite, x) == offsetof(engine::Sprite, rectangle));
static_assert(offsetof(GzxSprite, r) == offsetof(engine::Sprite, rectangle) + offsetof(engine::Rectangle, color));
static_assert(offsetof(GzxSprite, u0) == offsetof(engine::Sprite, uv));
static_assert(offsetof(GzxSprite, space) == offsetof(engine::Sprite, space));
static_assert(offsetof(GzxSprite, rotation) == offsetof(engine::Sprite, rotation));
static_assert(offsetof(GzxSprite, pivot_x) == offsetof(engine::Sprite, pivot_x));
static_assert(offsetof(GzxSprite, layer) == offsetof(engine::Sprite, layer));
static_assert(GZX_KEY_A == static_cast<int>(engine::Key::A) && GZX_KEY_0 == static_cast<int>(engine::Key::Num0) && GZX_KEY_F1 == static_cast<int>(engine::Key::F1));
static_assert(GZX_KEY_COUNT == static_cast<int>(engine::Key::Count));
static_assert(GZX_SPACE_SCREEN == static_cast<int>(engine::CoordinateSpace::Screen));
static_assert(GZX_SORT_LAYER_TEXTURE == static_cast<int>(engine::SortMode::LayerTexture));
static_assert(GZX_BODY_KINEMATIC == static_cast<int>(engine::BodyType::Kinematic));
static_assert(GZX_SHAPE_CIRCLE == static_cast<int>(engine::ShapeType::Circle));

/**
 * @brief 引擎实例，成员按依赖顺序声明，析构时逆序释放。
 *
 */
struct GzxEngine {
  /**
   * @brief 按与 Engine::run 相同的顺序创建窗口、音频和渲染器。
   *
   * @param options 窗口选项。
   */
  explicit GzxEngine(const engine::WindowOptions& options)
      : pause_when_minimized(options.pause_when_minimized),
        window(options),
        context(window.nativeHandle()),
        device(engine::render::selectDevice(context.instance(), context.surface())),
        allocator(context.instance(), device),
        renderer(device, allocator, context.surface(), window.nativeHandle()) {}

  bool pause_when_minimized;                                                // 窗口最小化时是否阻塞等待
  engine::platform::Window window;                                          // 窗口，必须最先创建、最后销毁
  engine::Audio audio;                                                      // 音频混音器，依赖窗口初始化的 SDL
  engine::render::VulkanContext context;                                    // Vulkan 实例和表面
  engine::render::VulkanDevice device;                                      // Vulkan 设备
  engine::render::GpuAllocator allocator;                                   // GPU 内存分配器
  engine::Assets assets;                                                    // 资源管理器
  engine::render::Renderer renderer;                                        // 渲染器
  engine::Input input;                                                      // 当前输入状态
  engine::RenderFrame frame;                                                // 当前帧的绘制内容
  std::size_t uploaded = 0;                                                 // 已上传到 GPU 的纹理数量
  std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();  // 上一帧开始时间
  float delta = 0.0f;                                                       // 本帧时间增量
  bool quit = false;                                                        // 是否已请求退出
};

/**
 * @brief 字体实例。
 *
 */
struct GzxFont {
  engine::BitmapFont font;  // 引擎字体
};

/**
 * @brief 物理世界实例，附带复用的查询缓冲区。
 *
 */
struct GzxWorld {
  engine::PhysicsWorld world;               // 引擎物理世界
  std::vector<engine::BodyHandle> scratch;  // 查询结果缓冲区，跨调用复用容量
};

namespace {
thread_local std::string last_error;  // 当前线程最近一次失败调用的错误信息

/**
 * @brief 执行函数体并把 C++ 异常转换为错误信息，保证异常不会越过 C 边界。
 *
 * @tparam Result 返回值类型。
 * @tparam Body 函数体类型。
 * @param fallback 发生异常时的返回值。
 * @param body 函数体。
 * @return Result 函数体的返回值或 fallback。
 */
template <class Result, class Body>
Result guard(Result fallback, Body&& body) noexcept {
  try {
    return body();
  } catch (const std::exception& error) {
    last_error = error.what();
  } catch (...) {
    last_error = "Unknown error";
  }
  return fallback;
}

/**
 * @brief 无返回值版本的 guard。
 *
 * @tparam Body 函数体类型。
 * @param body 函数体。
 */
template <class Body>
void guard(Body&& body) noexcept {
  guard(0, [&] {
    body();
    return 0;
  });
}

/**
 * @brief 检查指针非空，为空时抛出异常。
 *
 * @tparam T 指针指向的类型。
 * @param pointer 指针。
 * @param name 参数名，用于错误信息。
 * @return T& 指针指向的对象。
 */
template <class T>
T& require(T* pointer, const char* name) {
  if (!pointer) throw std::invalid_argument(std::string(name) + " must not be NULL");
  return *pointer;
}

/**
 * @brief 检查数组或字符串指针非空，为空时抛出异常。
 *
 * @tparam T 元素类型。
 * @param pointer 指针。
 * @param name 参数名，用于错误信息。
 * @return T* 原样返回的指针。
 */
template <class T>
T* nonNull(T* pointer, const char* name) {
  if (!pointer) throw std::invalid_argument(std::string(name) + " must not be NULL");
  return pointer;
}

/**
 * @brief 将 UTF-8 C 字符串转换为文件系统路径。
 *
 * @param path UTF-8 路径。
 * @return std::filesystem::path 路径。
 */
std::filesystem::path toPath(const char* path) {
  const std::string_view text(nonNull(path, "path"));
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

/**
 * @brief 将带代数的句柄打包为 64 位整数（高 32 位为代数，低 32 位为索引）。
 *
 * @param index 句柄索引。
 * @param generation 句柄代数。
 * @return uint64_t 打包后的句柄。
 */
constexpr uint64_t pack(std::uint32_t index, std::uint32_t generation) noexcept {
  return (static_cast<uint64_t>(generation) << 32) | index;
}

/**
 * @brief 将 64 位句柄解包为刚体句柄。
 *
 * @param handle 打包后的句柄。
 * @return engine::BodyHandle 刚体句柄。
 */
constexpr engine::BodyHandle toBody(uint64_t handle) noexcept {
  return {static_cast<std::uint32_t>(handle), static_cast<std::uint32_t>(handle >> 32)};
}

/**
 * @brief 将 64 位句柄解包为声部句柄。
 *
 * @param handle 打包后的句柄。
 * @return engine::VoiceHandle 声部句柄。
 */
constexpr engine::VoiceHandle toVoice(uint64_t handle) noexcept {
  return {static_cast<std::uint32_t>(handle), static_cast<std::uint32_t>(handle >> 32)};
}

/**
 * @brief 将坐标空间整数转换为枚举，越界时抛出异常。
 *
 * @param space 坐标空间。
 * @return engine::CoordinateSpace 枚举值。
 */
engine::CoordinateSpace toSpace(int32_t space) {
  if (space != GZX_SPACE_WORLD && space != GZX_SPACE_SCREEN) throw std::invalid_argument("Invalid coordinate space");
  return static_cast<engine::CoordinateSpace>(space);
}

/**
 * @brief 将 C 颜色转换为引擎颜色。
 *
 * @param color C 颜色。
 * @return engine::Color 引擎颜色。
 */
constexpr engine::Color toColor(GzxColor color) noexcept { return {color.r, color.g, color.b, color.a}; }

/**
 * @brief 将引擎 UV 转换为 C UV。
 *
 * @param uv 引擎 UV。
 * @return GzxUv C UV。
 */
constexpr GzxUv toUv(engine::UvRect uv) noexcept { return {uv.left, uv.top, uv.right, uv.bottom}; }

/**
 * @brief 写出可选的二维输出参数。
 *
 * @param x 横坐标输出指针，可以为空。
 * @param y 纵坐标输出指针，可以为空。
 * @param point 要写出的点。
 */
void writePoint(float* x, float* y, engine::Point2D point) noexcept {
  if (x) *x = point.x;
  if (y) *y = point.y;
}

/**
 * @brief 将查询结果写入调用方缓冲区。
 *
 * @param bodies 查询结果。
 * @param out 输出缓冲区，可以为空。
 * @param capacity 输出缓冲区容量。
 * @return size_t 结果总数。
 */
size_t copyBodies(const std::vector<engine::BodyHandle>& bodies, uint64_t* out, size_t capacity) noexcept {
  if (out) {
    const std::size_t count = std::min(bodies.size(), capacity);
    for (std::size_t index = 0; index < count; ++index) out[index] = pack(bodies[index].index, bodies[index].generation);
  }
  return bodies.size();
}
}  // namespace

extern "C" {

// ---------------------------------------------------------------------------
// 错误
// ---------------------------------------------------------------------------

const char* gzx_last_error(void) { return last_error.c_str(); }

// ---------------------------------------------------------------------------
// 引擎与主循环
// ---------------------------------------------------------------------------

void gzx_config_init(GzxConfig* config) {
  if (!config) return;
  const engine::WindowOptions defaults;
  *config = {"GZX Engine", defaults.width, defaults.height, defaults.resizable, defaults.pause_when_minimized};
}

GzxEngine* gzx_create(const GzxConfig* config) {
  return guard<GzxEngine*>(nullptr, [&] {
    engine::WindowOptions options;
    if (config) {
      if (config->title) options.title = config->title;
      options.width = config->width;
      options.height = config->height;
      options.resizable = config->resizable != 0;
      options.pause_when_minimized = config->pause_when_minimized != 0;
    }
    return new GzxEngine(options);
  });
}

void gzx_destroy(GzxEngine* engine) { delete engine; }

int gzx_begin_frame(GzxEngine* engine) {
  return guard(0, [&] {
    auto& self = require(engine, "engine");
    if (self.quit) return 0;

    self.input.clearEdges();  // 清除上一帧的按下、释放和滚轮事件
    for (;;) {
      if (!self.window.pollEvents(self.input)) return 0;
      auto& frame = self.frame;
      if (!SDL_GetWindowSizeInPixels(self.window.nativeHandle(), &frame.viewport_width, &frame.viewport_height)) {
        throw std::runtime_error("Failed to get window size in pixels");
      }
      const bool minimized = (SDL_GetWindowFlags(self.window.nativeHandle()) & SDL_WINDOW_MINIMIZED) != 0;
      if ((self.pause_when_minimized && minimized) || frame.viewport_width <= 0 || frame.viewport_height <= 0) {
        self.input.releaseAll();
        self.input.clearEdges();
        SDL_Delay(20);  // 最小化期间降低 CPU 占用
        self.previous = std::chrono::steady_clock::now();
        continue;
      }
      break;
    }

    const auto now = std::chrono::steady_clock::now();
    self.delta = std::min(std::chrono::duration<float>(now - self.previous).count(), 0.25f);  // 限制最大步长避免卡顿后时间跳变
    self.previous = now;
    self.frame.reset();  // 只清空精灵和相机，视口尺寸保留
    return 1;
  });
}

void gzx_end_frame(GzxEngine* engine) {
  guard([&] {
    auto& self = require(engine, "engine");
    if (self.assets.images().size() != self.uploaded) {
      self.renderer.uploadTextures(self.assets.images());
      self.assets.releasePixels(self.uploaded);  // 像素已上传到 GPU，释放 CPU 副本
      self.uploaded = self.assets.images().size();
    }
    if (!self.renderer.render(self.frame)) SDL_Delay(20);  // 交换链重建期间跳过本帧
  });
}

float gzx_delta_time(const GzxEngine* engine) { return engine ? engine->delta : 0.0f; }

void gzx_request_quit(GzxEngine* engine) {
  if (engine) engine->quit = true;
}

void gzx_viewport(const GzxEngine* engine, int32_t* width, int32_t* height) {
  if (width) *width = engine ? engine->frame.viewport_width : 0;
  if (height) *height = engine ? engine->frame.viewport_height : 0;
}

// ---------------------------------------------------------------------------
// 输入
// ---------------------------------------------------------------------------

int gzx_key_down(const GzxEngine* engine, int32_t key) { return engine && key >= 0 && engine->input.down(static_cast<engine::Key>(key)); }

int gzx_key_pressed(const GzxEngine* engine, int32_t key) { return engine && key >= 0 && engine->input.pressed(static_cast<engine::Key>(key)); }

int gzx_key_released(const GzxEngine* engine, int32_t key) { return engine && key >= 0 && engine->input.released(static_cast<engine::Key>(key)); }

int gzx_mouse_down(const GzxEngine* engine, int32_t button) { return engine && button >= 0 && engine->input.down(static_cast<engine::MouseButton>(button)); }

int gzx_mouse_pressed(const GzxEngine* engine, int32_t button) { return engine && button >= 0 && engine->input.pressed(static_cast<engine::MouseButton>(button)); }

int gzx_mouse_released(const GzxEngine* engine, int32_t button) { return engine && button >= 0 && engine->input.released(static_cast<engine::MouseButton>(button)); }

void gzx_mouse_position(const GzxEngine* engine, float* x, float* y) {
  writePoint(x, y, engine ? engine::Point2D{engine->input.mouse_x, engine->input.mouse_y} : engine::Point2D{});
}

void gzx_mouse_wheel(const GzxEngine* engine, float* x, float* y) {
  writePoint(x, y, engine ? engine::Point2D{engine->input.wheel_x, engine->input.wheel_y} : engine::Point2D{});
}

// ---------------------------------------------------------------------------
// 纹理
// ---------------------------------------------------------------------------

uint32_t gzx_load_texture(GzxEngine* engine, const char* path) {
  return guard<uint32_t>(GZX_INVALID_ID, [&] { return require(engine, "engine").assets.loadTexture(toPath(path)).index; });
}

uint32_t gzx_create_texture(GzxEngine* engine, uint32_t width, uint32_t height, const uint8_t* rgba) {
  return guard<uint32_t>(GZX_INVALID_ID, [&] {
    const auto bytes = std::span(reinterpret_cast<const std::byte*>(nonNull(rgba, "rgba")), std::size_t{width} * height * 4);
    return require(engine, "engine").assets.createTexture(width, height, bytes).index;
  });
}

int gzx_texture_size(const GzxEngine* engine, uint32_t texture, uint32_t* width, uint32_t* height) {
  return guard(0, [&] {
    const auto size = require(engine, "engine").assets.textureSize({texture});
    if (width) *width = size.width;
    if (height) *height = size.height;
    return 1;
  });
}

GzxUv gzx_texture_region(const GzxEngine* engine, uint32_t texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
  return guard(GzxUv{0.0f, 0.0f, 1.0f, 1.0f}, [&] { return toUv(require(engine, "engine").assets.textureRegion({texture}, x, y, width, height)); });
}

GzxUv gzx_sheet_uv(uint32_t columns, uint32_t rows, uint32_t frame) {
  if (columns == 0 || rows == 0) return {0.0f, 0.0f, 1.0f, 1.0f};
  frame %= columns * rows;
  const float column_scale = 1.0f / static_cast<float>(columns), row_scale = 1.0f / static_cast<float>(rows);
  const std::uint32_t column = frame % columns, row = frame / columns;
  return {static_cast<float>(column) * column_scale, static_cast<float>(row) * row_scale,
          static_cast<float>(column + 1) * column_scale, static_cast<float>(row + 1) * row_scale};
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void gzx_set_background(GzxEngine* engine, GzxColor color) {
  if (engine) engine->frame.background = toColor(color);
}

void gzx_set_camera(GzxEngine* engine, float x, float y, float zoom) {
  if (engine) engine->frame.camera = {x, y, zoom};  // 非法缩放值由渲染器在 gzx_end_frame 中报告
}

void gzx_set_sort(GzxEngine* engine, int32_t mode) {
  guard([&] {
    if (mode < GZX_SORT_NONE || mode > GZX_SORT_LAYER_TEXTURE) throw std::invalid_argument("Invalid sort mode");
    require(engine, "engine").frame.sort = static_cast<engine::SortMode>(mode);
  });
}

void gzx_draw_sprite(GzxEngine* engine, const GzxSprite* sprite) { gzx_draw_sprites(engine, sprite, 1); }

void gzx_draw_sprites(GzxEngine* engine, const GzxSprite* sprites, size_t count) {
  guard([&] {
    if (count == 0) return;
    auto& list = require(engine, "engine").frame.sprites;
    const std::size_t first = list.size();
    list.resize(first + count);
    std::memcpy(list.data() + first, nonNull(sprites, "sprites"), count * sizeof(GzxSprite));  // 布局已由 static_assert 保证一致
  });
}

void gzx_draw_rect(GzxEngine* engine, float x, float y, float width, float height, GzxColor color, int32_t space, int32_t layer) {
  guard([&] { require(engine, "engine").frame.drawRectangle({x, y, width, height, toColor(color)}, toSpace(space), layer); });
}

size_t gzx_sprite_count(const GzxEngine* engine) { return engine ? engine->frame.sprites.size() : 0; }

int64_t gzx_pick(const GzxEngine* engine, float x, float y) {
  if (!engine) return -1;
  const auto picked = engine->frame.pick({x, y});
  return picked ? static_cast<int64_t>(*picked) : -1;
}

void gzx_screen_to_world(const GzxEngine* engine, float x, float y, float* world_x, float* world_y) {
  writePoint(world_x, world_y, engine ? engine->frame.camera.screenToWorld({x, y}) : engine::Point2D{x, y});
}

void gzx_world_to_screen(const GzxEngine* engine, float x, float y, float* screen_x, float* screen_y) {
  writePoint(screen_x, screen_y, engine ? engine->frame.camera.worldToScreen({x, y}) : engine::Point2D{x, y});
}

// ---------------------------------------------------------------------------
// 字体与文本
// ---------------------------------------------------------------------------

GzxFont* gzx_font_default(GzxEngine* engine) {
  return guard<GzxFont*>(nullptr, [&] { return new GzxFont{engine::BitmapFont::createDefault(require(engine, "engine").assets)}; });
}

GzxFont* gzx_font_grid(GzxEngine* engine, uint32_t texture, uint32_t cell_width, uint32_t cell_height, uint32_t first, uint32_t count) {
  return guard<GzxFont*>(nullptr, [&] {
    return new GzxFont{engine::BitmapFont::fromGrid(require(engine, "engine").assets, {texture}, cell_width, cell_height, static_cast<char32_t>(first), count)};
  });
}

GzxFont* gzx_font_ttf(GzxEngine* engine, const char* path, float pixel_size, const char* characters) {
  return guard<GzxFont*>(nullptr, [&] {
    return new GzxFont{engine::BitmapFont::fromTtf(require(engine, "engine").assets, toPath(path), pixel_size, characters ? std::string_view(characters) : std::string_view{})};
  });
}

void gzx_font_destroy(GzxFont* font) { delete font; }

void gzx_draw_text(GzxEngine* engine, const GzxFont* font, const char* text, float x, float y, float scale, GzxColor color, int32_t space, int32_t layer) {
  guard([&] {
    require(font, "font").font.draw(require(engine, "engine").frame, nonNull(text, "text"), x, y, scale, toColor(color), toSpace(space), layer);
  });
}

void gzx_measure_text(const GzxFont* font, const char* text, float scale, float* width, float* height) {
  writePoint(width, height, font && text ? font->font.measure(text, scale) : engine::Point2D{});
}

float gzx_font_line_height(const GzxFont* font) { return font ? font->font.glyphHeight() : 0.0f; }

// ---------------------------------------------------------------------------
// 音频
// ---------------------------------------------------------------------------

int gzx_audio_available(const GzxEngine* engine) { return engine && engine->audio.available(); }

uint32_t gzx_load_sound(GzxEngine* engine, const char* path) {
  return guard<uint32_t>(GZX_INVALID_ID, [&] { return require(engine, "engine").audio.loadSound(toPath(path)).index; });
}

uint32_t gzx_create_sound(GzxEngine* engine, const float* samples, size_t count, uint32_t channels, uint32_t sample_rate) {
  return guard<uint32_t>(GZX_INVALID_ID, [&] {
    return require(engine, "engine").audio.createSound(std::span(nonNull(samples, "samples"), count), channels, sample_rate).index;
  });
}

uint64_t gzx_play(GzxEngine* engine, uint32_t sound, float volume, float pan, int loop) {
  return guard<uint64_t>(GZX_INVALID_HANDLE, [&] {
    const auto voice = require(engine, "engine").audio.play({sound}, volume, pan, loop != 0);
    return voice.index == engine::VoiceHandle{}.index ? GZX_INVALID_HANDLE : pack(voice.index, voice.generation);
  });
}

void gzx_stop(GzxEngine* engine, uint64_t voice) {
  if (engine) engine->audio.stop(toVoice(voice));
}

void gzx_stop_all(GzxEngine* engine) {
  if (engine) engine->audio.stopAll();
}

int gzx_playing(const GzxEngine* engine, uint64_t voice) { return engine && engine->audio.playing(toVoice(voice)); }

void gzx_set_voice_volume(GzxEngine* engine, uint64_t voice, float volume) {
  if (engine) engine->audio.setVolume(toVoice(voice), volume);
}

void gzx_set_voice_pan(GzxEngine* engine, uint64_t voice, float pan) {
  if (engine) engine->audio.setPan(toVoice(voice), pan);
}

void gzx_set_master_volume(GzxEngine* engine, float volume) {
  if (engine) engine->audio.setMasterVolume(volume);
}

// ---------------------------------------------------------------------------
// 物理
// ---------------------------------------------------------------------------

GzxWorld* gzx_world_create(float gravity_x, float gravity_y, float cell_size) {
  return guard<GzxWorld*>(nullptr, [&] { return new GzxWorld{engine::PhysicsWorld(gravity_x, gravity_y, cell_size), {}}; });
}

void gzx_world_destroy(GzxWorld* world) { delete world; }

void gzx_body_def_init(GzxBodyDef* definition) {
  if (!definition) return;
  const engine::BodyDefinition d;
  *definition = {static_cast<int32_t>(d.type), static_cast<int32_t>(d.shape), d.x, d.y, d.half_width, d.half_height, d.radius,
                 d.velocity_x, d.velocity_y, d.mass, d.restitution, d.friction, d.linear_damping, d.gravity_scale,
                 d.category, d.mask, d.sensor, d.user_data};
}

uint64_t gzx_world_create_body(GzxWorld* world, const GzxBodyDef* definition) {
  return guard<uint64_t>(GZX_INVALID_HANDLE, [&] {
    const auto& source = require(definition, "definition");
    if (source.type < GZX_BODY_STATIC || source.type > GZX_BODY_KINEMATIC) throw std::invalid_argument("Invalid body type");
    if (source.shape != GZX_SHAPE_BOX && source.shape != GZX_SHAPE_CIRCLE) throw std::invalid_argument("Invalid body shape");
    engine::BodyDefinition target;
    target.type = static_cast<engine::BodyType>(source.type);
    target.shape = static_cast<engine::ShapeType>(source.shape);
    target.x = source.x;
    target.y = source.y;
    target.half_width = source.half_width;
    target.half_height = source.half_height;
    target.radius = source.radius;
    target.velocity_x = source.velocity_x;
    target.velocity_y = source.velocity_y;
    target.mass = source.mass;
    target.restitution = source.restitution;
    target.friction = source.friction;
    target.linear_damping = source.linear_damping;
    target.gravity_scale = source.gravity_scale;
    target.category = source.category;
    target.mask = source.mask;
    target.sensor = source.sensor != 0;
    target.user_data = source.user_data;
    const auto body = require(world, "world").world.createBody(target);
    return pack(body.index, body.generation);
  });
}

void gzx_world_destroy_body(GzxWorld* world, uint64_t body) {
  if (world) world->world.destroyBody(toBody(body));
}

int gzx_world_valid(const GzxWorld* world, uint64_t body) { return world && world->world.valid(toBody(body)); }

void gzx_world_position(const GzxWorld* world, uint64_t body, float* x, float* y) {
  writePoint(x, y, guard(engine::Point2D{}, [&] { return require(world, "world").world.position(toBody(body)); }));
}

void gzx_world_set_position(GzxWorld* world, uint64_t body, float x, float y) {
  guard([&] { require(world, "world").world.setPosition(toBody(body), {x, y}); });
}

void gzx_world_velocity(const GzxWorld* world, uint64_t body, float* x, float* y) {
  writePoint(x, y, guard(engine::Point2D{}, [&] { return require(world, "world").world.velocity(toBody(body)); }));
}

void gzx_world_set_velocity(GzxWorld* world, uint64_t body, float x, float y) {
  guard([&] { require(world, "world").world.setVelocity(toBody(body), {x, y}); });
}

void gzx_world_apply_impulse(GzxWorld* world, uint64_t body, float x, float y) {
  guard([&] { require(world, "world").world.applyImpulse(toBody(body), x, y); });
}

uint64_t gzx_world_user_data(const GzxWorld* world, uint64_t body) {
  return guard<uint64_t>(0, [&] {
    const auto& self = require(world, "world").world;
    return self.valid(toBody(body)) ? self.userData(toBody(body)) : 0;
  });
}

void gzx_world_set_gravity(GzxWorld* world, float gravity_x, float gravity_y) {
  guard([&] { require(world, "world").world.setGravity(gravity_x, gravity_y); });
}

void gzx_world_step(GzxWorld* world, float delta_time, int32_t substeps, int32_t iterations) {
  guard([&] { require(world, "world").world.step(delta_time, substeps, iterations); });
}

size_t gzx_world_contacts(const GzxWorld* world, GzxContact* out, size_t capacity) {
  if (!world) return 0;
  const auto contacts = world->world.contacts();
  if (out) {
    const std::size_t count = std::min(contacts.size(), capacity);
    for (std::size_t index = 0; index < count; ++index) {
      const auto& contact = contacts[index];
      out[index] = {pack(contact.a.index, contact.a.generation), pack(contact.b.index, contact.b.generation),
                    contact.normal_x, contact.normal_y, contact.depth, contact.sensor};
    }
  }
  return contacts.size();
}

size_t gzx_world_query_point(GzxWorld* world, float x, float y, uint32_t mask, uint64_t* out, size_t capacity) {
  return guard<size_t>(0, [&] {
    auto& self = require(world, "world");
    self.scratch.clear();
    self.world.queryPoint({x, y}, self.scratch, mask);
    return copyBodies(self.scratch, out, capacity);
  });
}

size_t gzx_world_query_rect(GzxWorld* world, float left, float top, float right, float bottom, uint32_t mask, uint64_t* out, size_t capacity) {
  return guard<size_t>(0, [&] {
    auto& self = require(world, "world");
    self.scratch.clear();
    self.world.queryRect(left, top, right, bottom, self.scratch, mask);
    return copyBodies(self.scratch, out, capacity);
  });
}

int gzx_world_raycast(const GzxWorld* world, float x0, float y0, float x1, float y1, uint32_t mask, GzxRayHit* hit) {
  return guard(0, [&] {
    const auto result = require(world, "world").world.raycast({x0, y0}, {x1, y1}, mask);
    if (!result) return 0;
    if (hit) *hit = {pack(result->body.index, result->body.generation), result->x, result->y, result->normal_x, result->normal_y, result->fraction};
    return 1;
  });
}

void gzx_world_positions(const GzxWorld* world, const uint64_t* bodies, size_t count, float* out_xy) {
  guard([&] {
    const auto& self = require(world, "world").world;
    const auto* source = nonNull(bodies, "bodies");
    auto* target = nonNull(out_xy, "out_xy");
    for (std::size_t index = 0; index < count; ++index) {
      const auto position = self.position(toBody(source[index]));
      target[index * 2] = position.x;
      target[index * 2 + 1] = position.y;
    }
  });
}

size_t gzx_world_body_count(const GzxWorld* world) { return world ? world->world.bodyCount() : 0; }

}  // extern "C"
