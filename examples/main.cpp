#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../include/engine/engine.hpp"
#include "../include/engine/sprite_animation.hpp"

static_assert(engine::Camera2D{100.0f, 50.0f, 2.0f}.screenToWorld({200.0f, 100.0f}).x == 200.0f, "Camera2D::screenToWorld must be usable at compile time");  // 验证相机坐标转换可在编译期求值
static_assert(engine::Camera2D{100.0f, 50.0f, 2.0f}.worldToScreen({200.0f, 100.0f}).x == 200.0f, "Camera2D::worldToScreen must be usable at compile time");  // 验证相机坐标转换可在编译期求值

namespace {
constexpr float kPi = std::numbers::pi_v<float>;  // 圆周率
constexpr float kGravity = 1400.0f;               // 世界重力加速度（像素/秒²）
constexpr float kGroundTop = 600.0f;              // 地面顶部的世界 Y 坐标
constexpr float kLevelLeft = -400.0f;             // 关卡左边界
constexpr float kLevelRight = 2600.0f;            // 关卡右边界
constexpr float kTile = 32.0f;                    // 地形贴图的平铺尺寸
constexpr float kPlayerHalfWidth = 14.0f;         // 玩家碰撞盒半宽
constexpr float kPlayerHalfHeight = 34.0f;        // 玩家碰撞盒半高
constexpr float kPlayerSpeed = 280.0f;            // 玩家水平移动速度
constexpr float kJumpSpeed = 640.0f;              // 玩家起跳速度
constexpr float kSpriteScale = 2.0f;              // 角色精灵表的绘制缩放（单帧 96x96）
constexpr std::size_t kMaxParticles = 60000;      // 粒子数量上限
constexpr std::size_t kMaxProps = 800;            // 可交互物体数量上限
constexpr std::uint32_t kSampleRate = 48000;      // 程序化音效的采样率

constexpr std::uint32_t kWorldBit = 1u << 0;   // 碰撞类别：地形
constexpr std::uint32_t kPlayerBit = 1u << 1;  // 碰撞类别：玩家
constexpr std::uint32_t kPropBit = 1u << 2;    // 碰撞类别：可交互物体
constexpr std::uint32_t kPickupBit = 1u << 3;  // 碰撞类别：拾取物
constexpr std::uint32_t kHazardBit = 1u << 4;  // 碰撞类别：陷阱

constexpr std::int32_t kLayerSky = -100;    // 天空层
constexpr std::int32_t kLayerStars = -99;   // 星空层
constexpr std::int32_t kLayerFar = -98;     // 远景山层
constexpr std::int32_t kLayerNear = -97;    // 近景山层
constexpr std::int32_t kLayerDecor = -1;    // 世界装饰层
constexpr std::int32_t kLayerWorld = 0;     // 地形和物体层
constexpr std::int32_t kLayerFx = 5;        // 粒子层
constexpr std::int32_t kLayerLaser = 8;     // 激光层
constexpr std::int32_t kLayerPlayer = 10;   // 玩家层
constexpr std::int32_t kLayerDebug = 50;    // 碰撞框调试层
constexpr std::int32_t kLayerPanel = 100;   // 界面底板层
constexpr std::int32_t kLayerText = 101;    // 界面文字层
constexpr std::int32_t kLayerCursor = 200;  // 鼠标准星层

/**
 * @brief 刚体的游戏语义，存放在刚体的用户数据中。
 *
 */
enum class Kind : std::uint64_t { None, Ground, Platform, Mover, Crate, Ball, Coin, Spike, Player };

/**
 * @brief 中英文文本对，有中文字体时显示中文，否则回退到内置像素字体的英文。
 *
 */
struct Text {
  const char* zh;  // 中文文本
  const char* en;  // 英文文本
};

constexpr Text kTitle{"GZX 引擎演示", "GZX ENGINE DEMO"};
constexpr Text kFps{"帧率 %.0f   帧时间 %.2f 毫秒", "FPS %.0f   FRAME %.2f MS"};
constexpr Text kStats{"精灵 %zu   刚体 %zu   接触 %zu   粒子 %zu", "SPRITES %zu  BODIES %zu  CONTACTS %zu  PARTICLES %zu"};
constexpr Text kScore{"金币 %d / %d", "COINS %d / %d"};
constexpr Text kSortNames[]{{"不排序", "NONE"}, {"按层级", "LAYER"}, {"层级+纹理", "LAYER+TEXTURE"}};
constexpr Text kAudioOff{"音频设备不可用", "AUDIO UNAVAILABLE"};
constexpr Text kPicked{"拾取 第 %zu 个精灵  层级 %d", "PICK #%zu  LAYER %d"};
constexpr Text kHelpTitle{"操作说明（按 H 隐藏）", "CONTROLS (H TO HIDE)"};
constexpr Text kHelpHidden{"按 H 显示操作说明", "PRESS H FOR CONTROLS"};
constexpr Text kOn{"开启", "ON"};
constexpr Text kOff{"关闭", "OFF"};

/**
 * @brief 操作说明中需要显示当前状态的功能。
 *
 */
enum class Status : std::uint8_t { None, Storm, Sort, Colliders, Volume, Mute };

/**
 * @brief 操作说明中的一行：按键、功能和状态类型。
 *
 */
struct Control {
  Text key;       // 按键
  Text action;    // 功能
  Status status;  // 需要显示的状态
};

constexpr Control kControls[]{
  {{"A / D", "A / D"}, {"左右移动", "MOVE"}, Status::None},
  {{"W / 空格", "W / SPACE"}, {"跳跃", "JUMP"}, Status::None},
  {{"J", "J"}, {"攻击", "ATTACK"}, Status::None},
  {{"鼠标左键", "LEFT CLICK"}, {"生成木箱", "SPAWN CRATE"}, Status::None},
  {{"Shift + 左键", "SHIFT + CLICK"}, {"生成小球", "SPAWN BALL"}, Status::None},
  {{"按住右键", "HOLD RIGHT"}, {"激光推动物体", "LASER PUSH"}, Status::None},
  {{"鼠标滚轮", "WHEEL"}, {"缩放镜头", "ZOOM"}, Status::None},
  {{"P", "P"}, {"粒子风暴", "PARTICLE STORM"}, Status::Storm},
  {{"F1", "F1"}, {"切换排序模式", "SORT MODE"}, Status::Sort},
  {{"F2", "F2"}, {"显示碰撞框", "COLLIDERS"}, Status::Colliders},
  {{"Q / E", "Q / E"}, {"音量 减 / 加", "VOLUME - / +"}, Status::Volume},
  {{"M", "M"}, {"静音", "MUTE"}, Status::Mute},
  {{"R", "R"}, {"重置关卡", "RESET LEVEL"}, Status::None},
  {{"Esc", "ESC"}, {"退出", "QUIT"}, Status::None}};

#if defined(_WIN32)
// Windows 常见中文字体：微软雅黑 / 黑体 / 宋体（含 Bold 变体），均不存在时回退到像素字体
constexpr const char* kFontCandidates[]{
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/simsun.ttc",
    "C:/Windows/Fonts/msyhbd.ttc"};
#else
constexpr const char* kFontCandidates[]{
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"};  // 常见 Linux 中文字体，均不存在时回退到像素字体
#endif

/**
 * @brief 运行时自检失败时抛出异常，Release 构建下同样生效。
 *
 * @param condition 检查条件。
 * @param message 失败信息。
 */
void check(bool condition, const char* message) {
  if (!condition) throw std::logic_error(message);
}

/**
 * @brief 引擎纯 CPU 部分的自检：相机变换、旋转命中测试和按层级拾取。
 *
 */
void runSelfTests() {
  engine::Sprite rotated;
  rotated.rectangle = {0.0f, 0.0f, 100.0f, 50.0f};
  rotated.rotation = kPi * 0.5f;
  const engine::Camera2D camera{-100.0f, -50.0f, 2.0f};
  check(engine::hitTest(rotated, camera.worldToScreen({50.0f, 65.0f}), camera), "rotated hit test should hit");
  check(!engine::hitTest(rotated, camera.worldToScreen({90.0f, 25.0f}), camera), "rotated hit test should miss");

  engine::RenderFrame frame;
  frame.drawRectangle({0, 0, 100, 100}, engine::CoordinateSpace::Screen, 1);
  frame.drawRectangle({50, 50, 100, 100}, engine::CoordinateSpace::Screen, 0);
  check(frame.pick({75, 75}) == 0, "higher layer should win picking");  // 层级更高的先提交也应被拾取
  check(frame.pick({200, 200}) == std::nullopt, "empty area should not be picked");
}

/**
 * @brief xorshift32 伪随机数生成器，比 <random> 更轻量，适合每帧大量调用。
 *
 */
class Random {
 public:
  explicit Random(std::uint32_t seed) noexcept : state_(seed ? seed : 1u) {}

  /**
   * @brief 生成 [0, 1) 范围内的随机数。
   *
   * @return float 随机数。
   */
  float next() noexcept {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return static_cast<float>(state_ >> 8) * (1.0f / 16777216.0f);  // 取高 24 位映射到浮点尾数精度
  }

  /**
   * @brief 生成 [low, high) 范围内的随机数。
   *
   * @param low 下界。
   * @param high 上界。
   * @return float 随机数。
   */
  float range(float low, float high) noexcept { return low + (high - low) * next(); }

 private:
  std::uint32_t state_;  // 生成器状态
};

/**
 * @brief 将 HSV 颜色转换为 RGB 颜色。
 *
 * @param hue 色相，取值 0 到 1。
 * @param saturation 饱和度。
 * @param value 明度。
 * @param alpha 透明度。
 * @return engine::Color RGB 颜色。
 */
engine::Color hsv(float hue, float saturation, float value, float alpha = 1.0f) noexcept {
  const auto channel = [&](float n) {
    const float k = std::fmod(n + hue * 6.0f, 6.0f);
    return value - value * saturation * std::clamp(std::min(k, 4.0f - k), 0.0f, 1.0f);
  };
  return {channel(5.0f), channel(3.0f), channel(1.0f), alpha};
}

/**
 * @brief 整数坐标哈希噪声，用于程序化纹理的颗粒细节。
 *
 * @param x 横坐标。
 * @param y 纵坐标。
 * @return float [0, 1) 范围内的噪声值。
 */
float hashNoise(std::uint32_t x, std::uint32_t y) noexcept {
  std::uint32_t h = x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return static_cast<float>((h ^ (h >> 16)) & 0xFFFFFFu) / 16777216.0f;
}

/**
 * @brief 按像素着色函数生成 RGBA8 纹理并注册到资源管理器。
 *
 * @tparam Shader 着色函数类型，签名为 Color(float u, float v)。
 * @param assets 资源管理器。
 * @param width 纹理宽度。
 * @param height 纹理高度。
 * @param shader 着色函数，参数为像素中心的归一化坐标。
 * @return engine::TextureHandle 生成的纹理句柄。
 */
template <typename Shader>
engine::TextureHandle bake(engine::Assets& assets, std::uint32_t width, std::uint32_t height, Shader&& shader) {
  std::vector<std::byte> rgba(std::size_t{width} * height * 4);
  const auto channel = [](float value) { return std::byte{static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f)}; };
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const engine::Color color = shader((x + 0.5f) / width, (y + 0.5f) / height);
      auto* pixel = rgba.data() + (std::size_t{y} * width + x) * 4;
      pixel[0] = channel(color.red);
      pixel[1] = channel(color.green);
      pixel[2] = channel(color.blue);
      pixel[3] = channel(color.alpha);
    }
  }
  return assets.createTexture(width, height, rgba);
}

/**
 * @brief 按波形函数合成单声道音效并注册到混音器。
 *
 * @tparam Wave 波形函数类型，签名为 float(float seconds)。
 * @param audio 音频混音器。
 * @param seconds 音效时长（秒）。
 * @param wave 波形函数，参数为采样时间。
 * @return engine::SoundHandle 合成的声音句柄。
 */
template <typename Wave>
engine::SoundHandle synth(engine::Audio& audio, float seconds, Wave&& wave) {
  std::vector<float> samples(static_cast<std::size_t>(seconds * kSampleRate));
  for (std::size_t index = 0; index < samples.size(); ++index) samples[index] = wave(static_cast<float>(index) / kSampleRate);
  return audio.createSound(samples, 1, kSampleRate);
}

/**
 * @brief MIDI 音符编号转换为频率。
 *
 * @param note MIDI 音符编号，69 为 A4。
 * @return float 频率（赫兹）。
 */
float midi(int note) noexcept { return 440.0f * std::exp2((note - 69) / 12.0f); }

/**
 * @brief 线性扫频正弦波在时刻 t 的相位（弧度），对频率积分避免扫频时的相位跳变。
 *
 * @param t 时间。
 * @param duration 总时长。
 * @param from 起始频率。
 * @param to 结束频率。
 * @return float 相位。
 */
float sweepPhase(float t, float duration, float from, float to) noexcept {
  return 2.0f * kPi * (from * t + (to - from) * t * t / (2.0f * duration));
}

/**
 * @brief 单个粒子，按值存储于连续数组中以保证遍历时的缓存友好。
 *
 */
struct Particle {
  float x, y;          // 位置
  float vx, vy;        // 速度
  float life, span;    // 剩余寿命和总寿命
  float size;          // 尺寸
  engine::Color color;  // 颜色
};

/**
 * @brief 星空中的一颗星星，坐标为相对屏幕的归一化坐标。
 *
 */
struct Star {
  float x, y;   // 归一化屏幕坐标
  float size;   // 尺寸（像素）
  float phase;  // 闪烁相位
  float speed;  // 闪烁速度
};
}  // namespace

/**
 * @brief 引擎全功能演示游戏：平台跳跃 + 物理沙盒 + 粒子压力测试。
 *
 */
class DemoGame final : public engine::Game {
 public:
  /**
   * @brief 加载纹理、字体、音效并搭建关卡。
   *
   * @param assets 资源管理器。
   */
  void load(engine::Assets& assets) override {
    assets_ = &assets;
    idle_texture_ = assets.loadTexture("../Sprites/IDLE.png");
    run_texture_ = assets.loadTexture("../Sprites/RUN.png");
    hurt_texture_ = assets.loadTexture("../Sprites/HURT.png");
    check(assets.loadTexture("../Sprites/IDLE.png").index == idle_texture_.index, "texture cache should return the same handle");  // 同一路径命中缓存

    bakeTextures(assets);
    loadFonts(assets);
    synthSounds();
    createStars();
    buildLevel();
  }

  /**
   * @brief 固定步长更新游戏逻辑。
   *
   * @param delta_time 时间步长（秒）。
   * @param input 当前输入状态。
   */
  void update(float delta_time, const engine::Input& input) override {
    if (input.pressed(engine::Key::Escape)) {
      requestQuit();
      return;
    }
    time_ += delta_time;
    handleToggles(input);

    mouse_screen_ = {input.mouse_x, input.mouse_y};
    mouse_world_ = camera_.screenToWorld(mouse_screen_);
    if (input.wheel_y != 0.0f) target_zoom_ = std::clamp(target_zoom_ * std::pow(1.1f, std::clamp(input.wheel_y, -10.0f, 10.0f)), 0.5f, 3.0f);  // 滚轮按指数缩放

    world_.setVelocity(mover_, {std::cos(time_ * 0.8f) * 140.0f, 0.0f});  // 运动学平台按速度往返移动
    updatePlayer(delta_time, input);
    updateSandbox(input);

    world_.step(delta_time, 2, 6);  // 两个子步提高堆叠稳定性
    handleContacts();
    updateParticles(delta_time);
    updateCamera(delta_time);
  }

  /**
   * @brief 提交本帧所有绘制命令，按层级从低到高提交以命中渲染器的免排序快速路径。
   *
   * @param frame 渲染帧。
   */
  void draw(engine::RenderFrame& frame) override {
    measureFrameTime();
    if (frame.viewport_width <= 0 || frame.viewport_height <= 0) return;
    viewport_width_ = static_cast<float>(frame.viewport_width);
    viewport_height_ = static_cast<float>(frame.viewport_height);

    frame.background = {0.05f, 0.06f, 0.15f, 1.0f};
    frame.sort = static_cast<engine::SortMode>(sort_mode_);
    frame.camera = camera_;
    if (shake_ > 0.0f) {
      frame.camera.x += random_.range(-1.0f, 1.0f) * shake_ * 10.0f;  // 屏幕震动只影响渲染相机，不影响鼠标换算
      frame.camera.y += random_.range(-1.0f, 1.0f) * shake_ * 10.0f;
    }
    view_left_ = frame.camera.x - 64.0f;
    view_top_ = frame.camera.y - 64.0f;
    view_right_ = frame.camera.x + viewport_width_ / frame.camera.zoom + 64.0f;
    view_bottom_ = frame.camera.y + viewport_height_ / frame.camera.zoom + 64.0f;

    drawBackground(frame);
    drawWorld(frame);
    drawParticles(frame);
    drawLaser(frame);
    drawPlayer(frame);
    if (debug_) drawColliders(frame);

    picked_.reset();
    if (const auto picked = frame.pick(mouse_screen_)) {
      auto& sprite = frame.sprites[*picked];
      if (sprite.layer > kLayerNear) {
        sprite.rectangle.color = {1.0f, 0.45f, 0.45f, sprite.rectangle.color.alpha};  // 高亮鼠标下最上层的精灵
        picked_ = std::pair{*picked, sprite.layer};
      }
    }

    drawHud(frame);
    drawControls(frame);
    drawCursor(frame);
  }

 private:
  /**
   * @brief 场景中刚体的绘制信息。
   *
   */
  struct Body {
    engine::BodyHandle handle;  // 刚体句柄
    Kind kind;                  // 刚体语义
    float half_width;           // 绘制半宽
    float half_height;          // 绘制半高
    engine::Color tint;         // 绘制颜色
  };

  // ---------------------------------------------------------------------------
  // 资源
  // ---------------------------------------------------------------------------

  /**
   * @brief 生成所有程序化纹理，演示 createTexture 接口。
   *
   * @param assets 资源管理器。
   */
  void bakeTextures(engine::Assets& assets) {
    disc_texture_ = bake(assets, 64, 64, [](float u, float v) {
      const float distance = std::hypot(u - 0.5f, v - 0.5f) * 2.0f;
      const float shade = 1.0f - 0.35f * std::hypot(u - 0.35f, v - 0.35f);  // 左上角高光
      return engine::Color{shade, shade, shade, std::clamp((1.0f - distance) * 32.0f, 0.0f, 1.0f)};
    });

    glow_texture_ = bake(assets, 64, 64, [](float u, float v) {
      const float falloff = std::max(0.0f, 1.0f - std::hypot(u - 0.5f, v - 0.5f) * 2.0f);
      return engine::Color{1.0f, 1.0f, 1.0f, falloff * falloff};
    });

    crate_texture_ = bake(assets, 32, 32, [](float u, float v) {
      const float px = u * 32.0f, py = v * 32.0f;
      const bool border = px < 3.0f || px > 29.0f || py < 3.0f || py > 29.0f;
      const bool brace = std::abs(px - py) < 2.5f || std::abs(px - (32.0f - py)) < 2.5f;
      const float grain = 0.9f + 0.1f * hashNoise(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py) / 4);
      if (border || brace) return engine::Color{0.45f * grain, 0.27f * grain, 0.12f * grain, 1.0f};
      return engine::Color{0.78f * grain, 0.55f * grain, 0.30f * grain, 1.0f};
    });

    ground_texture_ = bake(assets, 32, 32, [](float u, float v) {
      const auto x = static_cast<std::uint32_t>(u * 32.0f), y = static_cast<std::uint32_t>(v * 32.0f);
      const float grass_edge = 6.0f + 3.0f * hashNoise(x, 7);  // 草地边缘参差不齐
      const float noise = hashNoise(x, y);
      if (v * 32.0f < grass_edge) return engine::Color{0.30f + 0.1f * noise, 0.72f + 0.1f * noise, 0.32f, 1.0f};
      const float speck = noise > 0.92f ? 0.15f : 0.0f;
      return engine::Color{0.40f + speck, 0.26f + speck, 0.18f + speck, 1.0f};
    });

    spike_texture_ = bake(assets, 32, 32, [](float u, float v) {
      const float local = std::fmod(u * 2.0f, 1.0f);  // 每块贴图两根尖刺
      const float edge = std::abs(local - 0.5f) * 2.0f;
      if (v < edge) return engine::Color{0.0f, 0.0f, 0.0f, 0.0f};
      const float metal = 0.55f + 0.4f * (1.0f - local);
      return engine::Color{metal, metal, metal + 0.08f, 1.0f};
    });

    sky_texture_ = bake(assets, 1, 256, [](float, float v) {
      const engine::Color top{0.04f, 0.05f, 0.14f, 1.0f}, middle{0.20f, 0.12f, 0.32f, 1.0f}, bottom{0.62f, 0.32f, 0.38f, 1.0f};
      const auto mix = [](engine::Color a, engine::Color b, float t) { return engine::Color{a.red + (b.red - a.red) * t, a.green + (b.green - a.green) * t, a.blue + (b.blue - a.blue) * t, 1.0f}; };
      return v < 0.6f ? mix(top, middle, v / 0.6f) : mix(middle, bottom, (v - 0.6f) / 0.4f);
    });

    hills_texture_ = bake(assets, 512, 128, [](float u, float v) {
      const float angle = u * 2.0f * kPi;  // 整数倍频率保证左右无缝平铺
      const float height = 0.55f + 0.18f * std::sin(angle * 2.0f) + 0.10f * std::sin(angle * 5.0f + 1.3f) + 0.05f * std::sin(angle * 11.0f + 0.7f);
      return engine::Color{1.0f, 1.0f, 1.0f, std::clamp((v - (1.0f - height)) * 128.0f, 0.0f, 1.0f)};
    });
  }

  /**
   * @brief 加载内置像素字体，并尝试将系统中文字体烘焙为图集。
   *
   * @param assets 资源管理器。
   */
  void loadFonts(engine::Assets& assets) {
    pixel_font_.emplace(engine::BitmapFont::createDefault(assets));

    std::string characters;  // 界面用到的全部中文字符
    for (const auto& text : {kTitle, kFps, kStats, kScore, kAudioOff, kPicked, kHelpTitle, kHelpHidden, kOn, kOff}) characters += text.zh;
    for (const auto& text : kSortNames) characters += text.zh;
    for (const auto& control : kControls) characters.append(control.key.zh).append(control.action.zh);

    for (const char* candidate : kFontCandidates) {
      if (!std::filesystem::exists(candidate)) continue;
      try {
        ttf_font_.emplace(engine::BitmapFont::fromTtf(assets, candidate, 18.0f, characters));
        return;
      } catch (const std::exception& error) {
        std::fprintf(stderr, "Font %s unavailable: %s\n", candidate, error.what());
      }
    }
  }

  /**
   * @brief 程序化合成全部音效和背景音乐，演示 createSound 接口。
   *
   */
  void synthSounds() {
    auto& mixer = audio();

    jump_sound_ = synth(mixer, 0.16f, [](float t) {
      const float envelope = (1.0f - t / 0.16f) * (1.0f - t / 0.16f);
      return 0.35f * envelope * std::sin(sweepPhase(t, 0.16f, 320.0f, 760.0f));
    });

    hit_sound_ = synth(mixer, 0.14f, [noise = Random(1234)](float t) mutable {
      const float envelope = 1.0f - t / 0.14f;
      return 0.45f * envelope * envelope * envelope * noise.range(-1.0f, 1.0f) + 0.5f * envelope * std::sin(2.0f * kPi * 90.0f * t);
    });

    coin_sound_ = synth(mixer, 0.22f, [](float t) {
      const float frequency = t < 0.07f ? 988.0f : 1319.0f;  // B5 -> E6 经典拾取音
      const float phase = 2.0f * kPi * frequency * t;
      return 0.25f * (1.0f - t / 0.22f) * (std::sin(phase) + std::sin(phase * 3.0f) / 3.0f);
    });

    hurt_sound_ = synth(mixer, 0.3f, [](float t) {
      const float cycles = sweepPhase(t, 0.3f, 520.0f, 110.0f) / (2.0f * kPi);
      return 0.25f * (1.0f - t / 0.3f) * (2.0f * (cycles - std::floor(cycles)) - 1.0f);  // 下行锯齿波
    });

    spawn_sound_ = synth(mixer, 0.06f, [](float t) { return 0.3f * (1.0f - t / 0.06f) * std::sin(2.0f * kPi * 220.0f * t); });

    constexpr int melody[]{57, 60, 64, 67, 69, 67, 64, 60, 53, 57, 60, 64, 65, 64, 60, 57,
                           48, 52, 55, 60, 64, 60, 55, 52, 55, 59, 62, 67, 71, 67, 62, 59};  // Am-F-C-G 琶音
    constexpr int bass[]{45, 41, 36, 43};
    music_sound_ = synth(mixer, 8.0f, [&](float t) {
      const int step = static_cast<int>(t / 0.25f) % 32;
      const float local = t - step * 0.25f;
      const float lead = 2.0f / kPi * std::asin(std::sin(2.0f * kPi * midi(melody[step]) * local));  // 三角波
      const int chord = static_cast<int>(t / 2.0f) % 4;
      const float bass_time = t - chord * 2.0f;
      return 0.16f * lead * std::exp(-local * 5.0f) + 0.22f * std::sin(2.0f * kPi * midi(bass[chord]) * bass_time) * std::exp(-bass_time * 1.2f);
    });

    music_voice_ = mixer.play(music_sound_, 0.3f, 0.0f, true);
    mixer.setMasterVolume(volume_);
  }

  /**
   * @brief 随机生成星空。
   *
   */
  void createStars() {
    stars_.resize(180);
    for (auto& star : stars_) star = {random_.next(), random_.next() * 0.65f, random_.range(1.0f, 3.0f), random_.range(0.0f, 2.0f * kPi), random_.range(1.0f, 4.0f)};
  }

  // ---------------------------------------------------------------------------
  // 关卡
  // ---------------------------------------------------------------------------

  /**
   * @brief 创建矩形刚体定义。
   *
   */
  static engine::BodyDefinition box(engine::BodyType type, float x, float y, float half_width, float half_height) {
    engine::BodyDefinition definition;
    definition.type = type;
    definition.shape = engine::ShapeType::Box;
    definition.x = x;
    definition.y = y;
    definition.half_width = half_width;
    definition.half_height = half_height;
    return definition;
  }

  /**
   * @brief 创建圆形刚体定义。
   *
   */
  static engine::BodyDefinition circle(engine::BodyType type, float x, float y, float radius) {
    engine::BodyDefinition definition;
    definition.type = type;
    definition.shape = engine::ShapeType::Circle;
    definition.x = x;
    definition.y = y;
    definition.radius = radius;
    return definition;
  }

  /**
   * @brief 在物理世界中创建刚体并记录绘制信息。
   *
   * @param kind 刚体语义。
   * @param definition 刚体定义。
   * @param tint 绘制颜色。
   * @return engine::BodyHandle 刚体句柄。
   */
  engine::BodyHandle addBody(Kind kind, engine::BodyDefinition definition, engine::Color tint) {
    definition.user_data = static_cast<std::uint64_t>(kind);
    const auto handle = world_.createBody(definition);
    const bool round = definition.shape == engine::ShapeType::Circle;
    bodies_.push_back({handle, kind, round ? definition.radius : definition.half_width, round ? definition.radius : definition.half_height, tint});
    return handle;
  }

  /**
   * @brief 创建地形刚体。
   *
   */
  void addSolid(Kind kind, float x, float y, float half_width, float half_height, engine::Color tint) {
    auto definition = box(engine::BodyType::Static, x, y, half_width, half_height);
    definition.category = kWorldBit;
    definition.friction = 0.6f;
    addBody(kind, definition, tint);
  }

  /**
   * @brief 生成一个可交互物体（木箱或小球）。
   *
   * @param ball 是否为小球。
   * @param x 中心 X 坐标。
   * @param y 中心 Y 坐标。
   */
  void spawnProp(bool ball, float x, float y) {
    if (prop_count_ >= kMaxProps) return;
    auto definition = ball ? circle(engine::BodyType::Dynamic, x, y, 14.0f) : box(engine::BodyType::Dynamic, x, y, 20.0f, 20.0f);
    definition.mass = ball ? 0.6f : 1.0f;
    definition.restitution = ball ? 0.6f : 0.05f;
    definition.friction = ball ? 0.2f : 0.5f;
    definition.category = kPropBit;
    definition.mask = kWorldBit | kPropBit | kPlayerBit;
    addBody(ball ? Kind::Ball : Kind::Crate, definition, ball ? hsv(random_.next(), 0.65f, 1.0f) : engine::Color{1.0f, 1.0f, 1.0f, 1.0f});
    ++prop_count_;
  }

  /**
   * @brief 重建整个关卡和玩家状态。
   *
   */
  void buildLevel() {
    world_ = engine::PhysicsWorld(0.0f, kGravity, 64.0f);
    bodies_.clear();
    particles_.clear();
    prop_count_ = 0;
    coins_ = coins_total_ = 0;

    const engine::Color grass{1.0f, 1.0f, 1.0f, 1.0f};
    addSolid(Kind::Ground, (kLevelLeft + kLevelRight) * 0.5f, kGroundTop + 200.0f, (kLevelRight - kLevelLeft) * 0.5f, 200.0f, grass);
    addSolid(Kind::Ground, kLevelLeft - 20.0f, kGroundTop - 400.0f, 20.0f, 600.0f, grass);
    addSolid(Kind::Ground, kLevelRight + 20.0f, kGroundTop - 400.0f, 20.0f, 600.0f, grass);

    constexpr std::array<std::array<float, 3>, 5> platforms{{{520, 470, 192}, {840, 370, 160}, {1180, 470, 192}, {1560, 350, 224}, {2100, 300, 160}}};  // 中心 X、顶部 Y、宽度
    for (const auto& [x, top, width] : platforms) {
      addSolid(Kind::Platform, x, top + 12.0f, width * 0.5f, 12.0f, grass);
      for (int coin = -1; coin <= 1; ++coin) addCoin(x + coin * 40.0f, top - 30.0f);
    }
    for (float x = -200.0f; x < 400.0f; x += 60.0f) addCoin(x, kGroundTop - 30.0f);

    auto mover = box(engine::BodyType::Kinematic, 1840.0f, 440.0f, 64.0f, 10.0f);
    mover.category = kWorldBit;
    mover.friction = 0.8f;
    mover_ = addBody(Kind::Mover, mover, {0.55f, 0.8f, 1.0f, 1.0f});

    auto spikes = box(engine::BodyType::Static, 1350.0f, kGroundTop - 12.0f, 64.0f, 12.0f);
    spikes.sensor = true;
    spikes.category = kHazardBit;
    spikes.mask = kPlayerBit;
    addBody(Kind::Spike, spikes, {1.0f, 1.0f, 1.0f, 1.0f});

    for (int row = 0; row < 4; ++row) {
      for (int column = 0; column < 4 - row; ++column) spawnProp(false, 620.0f + row * 21.0f + column * 42.0f, kGroundTop - 20.5f - row * 40.5f);
    }  // 木箱金字塔
    for (int ball = 0; ball < 5; ++ball) spawnProp(true, 950.0f + ball * 34.0f, kGroundTop - 200.0f);

    auto player = box(engine::BodyType::Dynamic, 0.0f, kGroundTop - kPlayerHalfHeight - 1.0f, kPlayerHalfWidth, kPlayerHalfHeight);
    player.friction = 0.0f;  // 玩家水平速度由输入直接控制
    player.category = kPlayerBit;
    player.mask = kWorldBit | kPropBit | kPickupBit | kHazardBit;
    player_ = addBody(Kind::Player, player, {1.0f, 1.0f, 1.0f, 1.0f});

    attacking_ = hurting_ = false;
    hurt_timer_ = 0.0f;
    camera_center_ = {0.0f, kGroundTop - 120.0f};
  }

  /**
   * @brief 创建金币触发器。
   *
   */
  void addCoin(float x, float y) {
    auto coin = circle(engine::BodyType::Static, x, y, 10.0f);
    coin.sensor = true;  // 只产生接触事件
    coin.category = kPickupBit;
    coin.mask = kPlayerBit;
    addBody(Kind::Coin, coin, {1.0f, 0.82f, 0.25f, 1.0f});
    ++coins_total_;
  }

  // ---------------------------------------------------------------------------
  // 逻辑
  // ---------------------------------------------------------------------------

  /**
   * @brief 处理功能开关类按键。
   *
   */
  void handleToggles(const engine::Input& input) {
    if (input.pressed(engine::Key::R)) buildLevel();
    if (input.pressed(engine::Key::F1)) sort_mode_ = (sort_mode_ + 1) % 3;
    if (input.pressed(engine::Key::F2)) debug_ = !debug_;
    if (input.pressed(engine::Key::P)) storm_ = !storm_;
    if (input.pressed(engine::Key::H)) show_help_ = !show_help_;

    const float previous = volume_;
    if (input.pressed(engine::Key::Q)) volume_ = std::max(0.0f, volume_ - 0.1f);
    if (input.pressed(engine::Key::E)) volume_ = std::min(1.0f, volume_ + 0.1f);
    if (input.pressed(engine::Key::M)) muted_ = !muted_;
    if (previous != volume_ || input.pressed(engine::Key::M)) audio().setMasterVolume(muted_ ? 0.0f : volume_);
  }

  /**
   * @brief 根据世界 X 坐标计算立体声声像。
   *
   * @param x 声源的世界 X 坐标。
   * @return float 声像，-1 为最左，1 为最右。
   */
  float panAt(float x) const noexcept {
    const float half = viewport_width_ * 0.5f / camera_.zoom;
    return std::clamp((x - camera_center_.x) / half, -1.0f, 1.0f);
  }

  /**
   * @brief 玩家移动、跳跃、攻击和受伤状态机。
   *
   */
  void updatePlayer(float delta_time, const engine::Input& input) {
    const auto position = world_.position(player_);
    auto velocity = world_.velocity(player_);
    const float feet = position.y + kPlayerHalfHeight;

    grounded_ = false;
    for (const float offset : {-kPlayerHalfWidth + 2.0f, kPlayerHalfWidth - 2.0f}) {
      grounded_ |= world_.raycast({position.x + offset, feet - 2.0f}, {position.x + offset, feet + 4.0f}, kWorldBit | kPropBit).has_value();
    }  // 两侧脚底向下射线检测着地

    hurt_timer_ = std::max(0.0f, hurt_timer_ - delta_time);
    if (hurting_) {
      hurt_animation_.update(delta_time);
      hurting_ = !hurt_animation_.finished();
    }

    if (!hurting_) {
      const bool left = input.down(engine::Key::A) || input.down(engine::Key::Left);
      const bool right = input.down(engine::Key::D) || input.down(engine::Key::Right);
      const int direction = static_cast<int>(right) - static_cast<int>(left);
      velocity.x = direction * kPlayerSpeed;
      if (direction != 0) facing_left_ = direction < 0;

      const bool moving = direction != 0;
      if (moving != running_) {
        running_ = moving;
        (running_ ? run_animation_ : idle_animation_).reset();
      }

      if (grounded_ && (input.pressed(engine::Key::W) || input.pressed(engine::Key::Up) || input.pressed(engine::Key::Space))) {
        velocity.y = -kJumpSpeed;
        audio().play(jump_sound_, 0.8f, panAt(position.x));
      }
      world_.setVelocity(player_, velocity);

      if (!attacking_ && input.pressed(engine::Key::J)) startAttack();
    }

    if (attacking_) {
      attack_animation_.update(delta_time);
      attack_timer_ += delta_time;
      if (!attack_hit_done_ && attack_timer_ >= 0.12f) attackHit();  // 挥刀到第 3 帧左右时判定命中
      if (attack_animation_.finished()) attacking_ = false;
    }
    (running_ ? run_animation_ : idle_animation_).update(delta_time);

    if (position.y > kGroundTop + 1000.0f) {
      world_.setPosition(player_, {0.0f, kGroundTop - kPlayerHalfHeight - 1.0f});
      world_.setVelocity(player_, {0.0f, 0.0f});
    }  // 掉出世界后复活
  }

  /**
   * @brief 开始攻击，首次攻击时才加载攻击纹理，演示运行期增量上传纹理。
   *
   */
  void startAttack() {
    if (attack_texture_.index == engine::TextureHandle{}.index) attack_texture_ = assets_->loadTexture("../Sprites/ATTACK 1.png");
    attacking_ = true;
    attack_hit_done_ = false;
    attack_timer_ = 0.0f;
    attack_animation_.reset();
  }

  /**
   * @brief 攻击判定：查询前方矩形区域内的物体并施加冲量。
   *
   */
  void attackHit() {
    attack_hit_done_ = true;
    const auto position = world_.position(player_);
    const float direction = facing_left_ ? -1.0f : 1.0f;
    const float near_x = position.x, far_x = position.x + direction * 90.0f;

    query_.clear();
    world_.queryRect(std::min(near_x, far_x), position.y - 40.0f, std::max(near_x, far_x), position.y + 30.0f, query_, kPropBit);
    for (const auto body : query_) {
      world_.applyImpulse(body, direction * 420.0f, -320.0f);
      const auto hit = world_.position(body);
      emitBurst(hit.x, hit.y, 40, 360.0f, {1.0f, 0.75f, 0.35f, 1.0f}, 0.6f);
    }
    if (!query_.empty()) {
      audio().play(hit_sound_, 1.0f, panAt(position.x));
      shake_ = 0.35f;
    }
  }

  /**
   * @brief 鼠标交互：左键生成物体，右键激光射线。
   *
   */
  void updateSandbox(const engine::Input& input) {
    if (input.pressed(engine::MouseButton::Left)) {
      spawnProp(input.down(engine::Key::LeftShift) || input.down(engine::Key::RightShift), mouse_world_.x, mouse_world_.y);
      audio().play(spawn_sound_, 0.6f, panAt(mouse_world_.x));
    }

    laser_active_ = input.down(engine::MouseButton::Right);
    if (laser_active_) {
      const auto origin = world_.position(player_);
      laser_from_ = {origin.x, origin.y - 10.0f};
      const float dx = mouse_world_.x - laser_from_.x, dy = mouse_world_.y - laser_from_.y;
      const float length = std::max(std::hypot(dx, dy), 1e-3f);
      const engine::Point2D end{laser_from_.x + dx / length * 900.0f, laser_from_.y + dy / length * 900.0f};
      const auto hit = world_.raycast(laser_from_, end, kWorldBit | kPropBit);
      laser_to_ = hit ? engine::Point2D{hit->x, hit->y} : end;
      laser_hit_ = hit.has_value();
      if (hit) {
        const auto kind = static_cast<Kind>(world_.userData(hit->body));
        if (kind == Kind::Crate || kind == Kind::Ball) world_.applyImpulse(hit->body, dx / length * 30.0f, dy / length * 30.0f);  // 持续推动被照射的物体
        emitBurst(hit->x, hit->y, 3, 220.0f, {1.0f, 0.3f, 0.25f, 1.0f}, 0.35f);
      }
    }

    if (storm_) {
      const float left = camera_.x, width = viewport_width_ / camera_.zoom;
      for (int index = 0; index < 400 && particles_.size() < kMaxParticles; ++index) {
        particles_.push_back({left + random_.next() * width, camera_.y - 20.0f, random_.range(-30.0f, 30.0f), random_.range(100.0f, 300.0f), 0.0f, 0.0f, random_.range(3.0f, 7.0f), hsv(random_.range(0.5f, 0.65f), 0.6f, 1.0f, 0.8f)});
        particles_.back().life = particles_.back().span = random_.range(2.0f, 3.0f);
      }
    }  // 每步发射 400 个粒子，稳定在数万个精灵，用于压力测试
  }

  /**
   * @brief 处理物理接触事件：拾取金币、踩到尖刺。
   *
   */
  void handleContacts() {
    pending_.clear();
    for (const auto& contact : world_.contacts()) {
      if (!world_.valid(contact.a) || !world_.valid(contact.b)) continue;
      const auto kind_a = static_cast<Kind>(world_.userData(contact.a));
      const auto kind_b = static_cast<Kind>(world_.userData(contact.b));
      if (kind_a != Kind::Player && kind_b != Kind::Player) continue;
      const auto other = kind_a == Kind::Player ? contact.b : contact.a;
      const auto kind = kind_a == Kind::Player ? kind_b : kind_a;
      if (kind == Kind::Coin) pending_.push_back(other);
      if (kind == Kind::Spike) hurt();
    }

    for (const auto coin : pending_) {
      if (!world_.valid(coin)) continue;  // 同一金币可能在多个子步中重复上报
      const auto position = world_.position(coin);
      world_.destroyBody(coin);
      ++coins_;
      emitBurst(position.x, position.y, 30, 260.0f, {1.0f, 0.85f, 0.3f, 1.0f}, 0.7f);
      audio().play(coin_sound_, 0.8f, panAt(position.x));
    }
    if (!pending_.empty()) std::erase_if(bodies_, [&](const Body& body) { return !world_.valid(body.handle); });
  }

  /**
   * @brief 玩家受伤：播放受伤动画、击退并进入短暂无敌。
   *
   */
  void hurt() {
    if (hurt_timer_ > 0.0f) return;
    hurt_timer_ = 1.2f;
    hurting_ = true;
    attacking_ = false;
    hurt_animation_.reset();
    world_.setVelocity(player_, {facing_left_ ? 260.0f : -260.0f, -480.0f});
    const auto position = world_.position(player_);
    emitBurst(position.x, position.y, 40, 300.0f, {1.0f, 0.25f, 0.25f, 1.0f}, 0.6f);
    audio().play(hurt_sound_, 0.9f, panAt(position.x));
    shake_ = 0.6f;
  }

  /**
   * @brief 在指定位置发射一圈粒子。
   *
   */
  void emitBurst(float x, float y, int count, float speed, engine::Color color, float life) {
    for (int index = 0; index < count && particles_.size() < kMaxParticles; ++index) {
      const float angle = random_.range(0.0f, 2.0f * kPi), magnitude = random_.range(0.2f, 1.0f) * speed;
      const float span = life * random_.range(0.6f, 1.0f);
      particles_.push_back({x, y, std::cos(angle) * magnitude, std::sin(angle) * magnitude, span, span, random_.range(4.0f, 9.0f), color});
    }
  }

  /**
   * @brief 积分粒子运动，过期粒子与末尾交换后弹出，避免数组搬移。
   *
   */
  void updateParticles(float delta_time) {
    for (std::size_t index = 0; index < particles_.size();) {
      auto& particle = particles_[index];
      particle.life -= delta_time;
      if (particle.life <= 0.0f) {
        particle = particles_.back();
        particles_.pop_back();
        continue;
      }
      particle.vy += 900.0f * delta_time;
      particle.x += particle.vx * delta_time;
      particle.y += particle.vy * delta_time;
      if (particle.y > kGroundTop && particle.vy > 0.0f) {
        particle.y = kGroundTop;
        particle.vy *= -0.3f;  // 落地轻微反弹
      }
      ++index;
    }
  }

  /**
   * @brief 相机平滑跟随玩家并处理缩放和震动衰减。
   *
   */
  void updateCamera(float delta_time) {
    const float blend = 1.0f - std::exp(-delta_time * 6.0f);  // 与帧率无关的指数平滑
    const auto target = world_.position(player_);
    camera_center_.x += (target.x - camera_center_.x) * blend;
    camera_center_.y += (std::min(target.y - 60.0f, kGroundTop - 120.0f) - camera_center_.y) * blend;
    camera_.zoom += (target_zoom_ - camera_.zoom) * blend;
    camera_.x = camera_center_.x - viewport_width_ * 0.5f / camera_.zoom;
    camera_.y = camera_center_.y - viewport_height_ * 0.5f / camera_.zoom;
    shake_ = std::max(0.0f, shake_ - delta_time);
  }

  // ---------------------------------------------------------------------------
  // 绘制
  // ---------------------------------------------------------------------------

  /**
   * @brief 统计帧时间，用于界面上的帧率和帧时间曲线。
   *
   */
  void measureFrameTime() {
    const auto now = std::chrono::steady_clock::now();
    const float milliseconds = std::chrono::duration<float, std::milli>(now - last_draw_).count();
    last_draw_ = now;
    if (milliseconds <= 0.0f || milliseconds > 1000.0f) return;
    frame_ms_ += (milliseconds - frame_ms_) * 0.05f;
    history_[history_index_] = milliseconds;
    history_index_ = (history_index_ + 1) % history_.size();
  }

  /**
   * @brief 判断世界空间矩形是否在可见范围内。
   *
   */
  bool visible(float left, float top, float right, float bottom) const noexcept {
    return right >= view_left_ && left <= view_right_ && bottom >= view_top_ && top <= view_bottom_;
  }

  /**
   * @brief 在世界空间中水平平铺纹理，只提交可见的贴图块。
   *
   */
  void drawTiled(engine::RenderFrame& frame, engine::TextureHandle texture, float x, float y, float width, float height, engine::Color tint, std::int32_t layer) const {
    const float right = x + width;
    float left = x;
    if (view_left_ > left) left += std::floor((view_left_ - left) / kTile) * kTile;  // 跳过左侧不可见的贴图块
    for (; left < right && left <= view_right_; left += kTile) {
      const float tile = std::min(kTile, right - left);
      frame.drawSprite(texture, {left, y, tile, height}, {0.0f, 0.0f, tile / kTile, 1.0f}, tint, engine::CoordinateSpace::World, layer);
    }
  }

  /**
   * @brief 绘制屏幕空间的天空、星空和视差山脉。
   *
   */
  void drawBackground(engine::RenderFrame& frame) const {
    const auto screen = engine::CoordinateSpace::Screen;
    frame.drawSprite(sky_texture_, {0.0f, 0.0f, viewport_width_, viewport_height_}, {}, {1.0f, 1.0f, 1.0f, 1.0f}, screen, kLayerSky);

    for (const auto& star : stars_) {
      const float alpha = 0.725f + 0.275f * std::sin(time_ * star.speed + star.phase);
      frame.drawSprite(disc_texture_, {star.x * viewport_width_, star.y * viewport_height_, star.size, star.size}, {}, {1.0f, 0.95f, 0.85f, alpha}, screen, kLayerStars);
    }

    const auto hills = [&](float parallax, float height_ratio, engine::Color tint, std::int32_t layer) {
      const float width = 640.0f, height = viewport_height_ * height_ratio;
      float offset = -std::fmod(camera_.x * camera_.zoom * parallax, width);  // 视差系数越小越远
      if (offset > 0.0f) offset -= width;
      for (float x = offset; x < viewport_width_; x += width) frame.drawSprite(hills_texture_, {x, viewport_height_ - height, width + 1.0f, height}, {}, tint, screen, layer);
    };
    hills(0.15f, 0.55f, {0.22f, 0.16f, 0.36f, 1.0f}, kLayerFar);
    hills(0.4f, 0.4f, {0.13f, 0.10f, 0.24f, 1.0f}, kLayerNear);
  }

  /**
   * @brief 绘制世界空间的风车、地形、物体和金币。
   *
   */
  void drawWorld(engine::RenderFrame& frame) const {
    const auto world = engine::CoordinateSpace::World;

    constexpr engine::Point2D hub{1000.0f, 420.0f};
    if (visible(hub.x - 120.0f, hub.y - 120.0f, hub.x + 120.0f, kGroundTop)) {
      frame.drawRectangle({hub.x - 8.0f, hub.y, 16.0f, kGroundTop - hub.y, {0.25f, 0.2f, 0.3f, 1.0f}}, world, kLayerDecor);
      for (int blade = 0; blade < 4; ++blade) {
        auto& sprite = frame.drawSprite(engine::TextureHandle{0}, {hub.x, hub.y - 7.0f, 110.0f, 14.0f}, {}, {0.85f, 0.8f, 0.9f, 1.0f}, world, kLayerDecor);
        sprite.rotation = time_ * 1.2f + blade * kPi * 0.5f;  // 绕叶片根部旋转
        sprite.pivot_x = 0.0f;
        sprite.pivot_y = 0.5f;
      }
      frame.drawSprite(disc_texture_, {hub.x - 10.0f, hub.y - 10.0f, 20.0f, 20.0f}, {}, {0.4f, 0.35f, 0.45f, 1.0f}, world, kLayerDecor);
    }

    for (const auto& body : bodies_) {
      if (body.kind == Kind::Player) continue;
      const auto center = world_.position(body.handle);
      const float left = center.x - body.half_width, top = center.y - body.half_height;
      const float width = body.half_width * 2.0f, height = body.half_height * 2.0f;
      if (!visible(left, top, left + width, top + height)) continue;  // 视锥剔除

      switch (body.kind) {
        case Kind::Ground:
        case Kind::Platform:
        case Kind::Mover: {
          const float grass = std::min(kTile, height);
          drawTiled(frame, ground_texture_, left, top, width, grass, body.tint, kLayerWorld);
          if (height > grass) frame.drawRectangle({left, top + grass, width, height - grass, {0.30f, 0.19f, 0.13f, 1.0f}}, world, kLayerWorld);
          break;
        }
        case Kind::Spike:
          drawTiled(frame, spike_texture_, left, top, width, height, body.tint, kLayerWorld);
          break;
        case Kind::Crate:
          frame.drawSprite(crate_texture_, {left, top, width, height}, {}, body.tint, world, kLayerWorld);
          break;
        case Kind::Ball:
          frame.drawSprite(disc_texture_, {left, top, width, height}, {}, body.tint, world, kLayerWorld);
          break;
        case Kind::Coin: {
          const float bob = std::sin(time_ * 3.0f + center.x * 0.05f) * 3.0f;
          const float spin = std::max(0.15f, std::abs(std::cos(time_ * 4.0f + center.x)));  // 压缩宽度模拟金币翻转
          frame.drawSprite(glow_texture_, {center.x - 20.0f, center.y - 20.0f + bob, 40.0f, 40.0f}, {}, {1.0f, 0.8f, 0.3f, 0.35f}, world, kLayerWorld);
          frame.drawSprite(disc_texture_, {center.x - body.half_width * spin, top + bob, width * spin, height}, {}, body.tint, world, kLayerWorld);
          break;
        }
        default:
          break;
      }
    }
  }

  /**
   * @brief 绘制所有粒子，透明度随剩余寿命衰减。
   *
   */
  void drawParticles(engine::RenderFrame& frame) const {
    frame.sprites.reserve(frame.sprites.size() + particles_.size() + 256);
    for (const auto& particle : particles_) {
      const float half = particle.size * 0.5f;
      if (!visible(particle.x - half, particle.y - half, particle.x + half, particle.y + half)) continue;
      auto color = particle.color;
      color.alpha *= particle.life / particle.span;
      frame.drawSprite(glow_texture_, {particle.x - half, particle.y - half, particle.size, particle.size}, {}, color, engine::CoordinateSpace::World, kLayerFx);
    }
  }

  /**
   * @brief 绘制激光：以起点为旋转中心的细长矩形。
   *
   */
  void drawLaser(engine::RenderFrame& frame) const {
    if (!laser_active_) return;
    const float dx = laser_to_.x - laser_from_.x, dy = laser_to_.y - laser_from_.y;
    const float length = std::hypot(dx, dy), angle = std::atan2(dy, dx);
    for (const auto& [thickness, color] : {std::pair{10.0f, engine::Color{1.0f, 0.2f, 0.2f, 0.3f}}, std::pair{2.0f, engine::Color{1.0f, 0.9f, 0.9f, 1.0f}}}) {
      auto& beam = frame.drawSprite(engine::TextureHandle{0}, {laser_from_.x, laser_from_.y - thickness * 0.5f, length, thickness}, {}, color, engine::CoordinateSpace::World, kLayerLaser);
      beam.rotation = angle;
      beam.pivot_x = 0.0f;
      beam.pivot_y = 0.5f;
    }
    if (laser_hit_) frame.drawSprite(glow_texture_, {laser_to_.x - 16.0f, laser_to_.y - 16.0f, 32.0f, 32.0f}, {}, {1.0f, 0.4f, 0.3f, 0.9f}, engine::CoordinateSpace::World, kLayerLaser);
  }

  /**
   * @brief 根据状态选择动画并绘制玩家，向左时交换 UV 实现水平翻转。
   *
   */
  void drawPlayer(engine::RenderFrame& frame) const {
    const auto position = world_.position(player_);
    const engine::SpriteAnimation* animation = &idle_animation_;
    engine::TextureHandle texture = idle_texture_;
    if (hurting_) {
      animation = &hurt_animation_;
      texture = hurt_texture_;
    } else if (attacking_) {
      animation = &attack_animation_;
      texture = attack_texture_;
    } else if (running_ && grounded_) {
      animation = &run_animation_;
      texture = run_texture_;
    }

    auto uv = animation->uv();
    if (facing_left_) std::swap(uv.left, uv.right);
    const bool blink = hurt_timer_ > 0.0f && std::fmod(time_, 0.15f) < 0.075f;  // 无敌时间内闪烁
    const float size = 96.0f * kSpriteScale;
    frame.drawSprite(glow_texture_, {position.x - 28.0f, position.y + kPlayerHalfHeight - 6.0f, 56.0f, 12.0f}, {}, {0.0f, 0.0f, 0.0f, 0.45f}, engine::CoordinateSpace::World, kLayerPlayer);  // 脚底阴影
    frame.drawSprite(texture, {position.x - size * 0.5f, position.y + kPlayerHalfHeight - 80.0f * kSpriteScale, size, size}, uv, {1.0f, 1.0f, 1.0f, blink ? 0.35f : 1.0f}, engine::CoordinateSpace::World, kLayerPlayer);  // 精灵帧内脚底位于第 80 行
  }

  /**
   * @brief 绘制所有刚体的包围盒，用于调试物理。
   *
   */
  void drawColliders(engine::RenderFrame& frame) const {
    for (const auto& body : bodies_) {
      const auto center = world_.position(body.handle);
      const engine::Color color = body.kind == Kind::Player ? engine::Color{0.2f, 1.0f, 0.4f, 0.35f}
                                  : body.kind == Kind::Coin || body.kind == Kind::Spike ? engine::Color{1.0f, 0.9f, 0.2f, 0.3f}
                                                                                      : engine::Color{0.3f, 0.7f, 1.0f, 0.25f};
      frame.drawRectangle({center.x - body.half_width, center.y - body.half_height, body.half_width * 2.0f, body.half_height * 2.0f, color}, engine::CoordinateSpace::World, kLayerDebug);
    }
  }

  /**
   * @brief 获取界面字体，有中文字体时优先使用。
   *
   */
  const engine::BitmapFont& font() const noexcept { return ttf_font_ ? *ttf_font_ : *pixel_font_; }

  /**
   * @brief 获取界面文字缩放，像素字体放大两倍以便阅读。
   *
   */
  float fontScale() const noexcept { return ttf_font_ ? 1.0f : 2.0f; }

  /**
   * @brief 按当前字体选择中文或英文文本。
   *
   */
  const char* tr(const Text& text) const noexcept { return ttf_font_ ? text.zh : text.en; }

  /**
   * @brief 绘制带阴影的屏幕空间文字。
   *
   */
  void drawText(engine::RenderFrame& frame, const char* text, float x, float y, engine::Color color) const {
    font().draw(frame, text, x + 1.0f, y + 1.0f, fontScale(), {0.0f, 0.0f, 0.0f, 0.7f}, engine::CoordinateSpace::Screen, kLayerText);
    font().draw(frame, text, x, y, fontScale(), color, engine::CoordinateSpace::Screen, kLayerText);
  }

  /**
   * @brief 绘制一块带半透明底板的文字面板。
   *
   * @param lines 文字行。
   * @param count 行数。
   * @param x 面板左上角 X 坐标。
   * @param y 面板左上角 Y 坐标，为负数时从屏幕底部向上对齐。
   * @param highlight 首行是否高亮为标题。
   */
  void drawPanel(engine::RenderFrame& frame, const char* const* lines, std::size_t count, float x, float y, bool highlight) const {
    const float line_height = font().glyphHeight() * fontScale() + 4.0f;
    float width = 0.0f;
    for (std::size_t index = 0; index < count; ++index) width = std::max(width, font().measure(lines[index], fontScale()).x);
    const float height = line_height * count;
    if (y < 0.0f) y = viewport_height_ - height + y;

    frame.drawRectangle({x - 10.0f, y - 8.0f, width + 20.0f, height + 12.0f, {0.0f, 0.0f, 0.0f, 0.45f}}, engine::CoordinateSpace::Screen, kLayerPanel);
    for (std::size_t index = 0; index < count; ++index) {
      const engine::Color color = highlight && index == 0 ? engine::Color{1.0f, 0.85f, 0.4f, 1.0f} : engine::Color{0.92f, 0.94f, 1.0f, 1.0f};
      drawText(frame, lines[index], x, y + index * line_height, color);
    }
  }

  /**
   * @brief 绘制状态面板、操作说明和帧时间曲线。
   *
   */
  void drawHud(engine::RenderFrame& frame) const {
    const std::size_t sprite_count = frame.sprites.size();
    std::array<std::array<char, 160>, 8> buffers{};  // 栈上格式化缓冲区，界面绘制零堆分配
    std::array<const char*, 8> lines{};
    std::size_t count = 0;
    const auto line = [&](const char* format, auto... arguments) {
      std::snprintf(buffers[count].data(), buffers[count].size(), format, arguments...);
      lines[count] = buffers[count].data();
      ++count;
    };

    lines[count++] = tr(kTitle);
    line(tr(kFps), frame_ms_ > 0.0f ? 1000.0f / frame_ms_ : 0.0f, frame_ms_);
    line(tr(kStats), sprite_count, world_.bodyCount(), world_.contacts().size(), particles_.size());
    line(tr(kScore), coins_, coins_total_);
    if (!audio().available()) lines[count++] = tr(kAudioOff);
    if (picked_) line(tr(kPicked), picked_->first, picked_->second);
    drawPanel(frame, lines.data(), count, 20.0f, 18.0f, true);

    const float graph_right = viewport_width_ - 20.0f, graph_bottom = 90.0f;
    frame.drawRectangle({graph_right - history_.size() * 2.0f - 6.0f, 14.0f, history_.size() * 2.0f + 12.0f, graph_bottom - 8.0f, {0.0f, 0.0f, 0.0f, 0.45f}}, engine::CoordinateSpace::Screen, kLayerPanel);
    for (std::size_t index = 0; index < history_.size(); ++index) {
      const float milliseconds = history_[(history_index_ + index) % history_.size()];  // 从最旧到最新
      const float height = std::min(milliseconds * 2.0f, 66.0f);
      const engine::Color color = milliseconds < 17.0f ? engine::Color{0.3f, 0.9f, 0.4f, 0.9f} : milliseconds < 34.0f ? engine::Color{1.0f, 0.8f, 0.2f, 0.9f} : engine::Color{1.0f, 0.3f, 0.3f, 0.9f};
      frame.drawRectangle({graph_right - (history_.size() - index) * 2.0f, graph_bottom - height, 2.0f, height, color}, engine::CoordinateSpace::Screen, kLayerText);
    }
  }

  /**
   * @brief 生成某项功能当前状态的文本。
   *
   * @param status 状态类型。
   * @param buffer 输出缓冲区。
   * @return true 功能处于开启状态，用高亮颜色显示。
   */
  bool statusText(Status status, std::array<char, 32>& buffer) const {
    const auto write = [&](const char* text) { std::snprintf(buffer.data(), buffer.size(), "%s", text); };
    switch (status) {
      case Status::Storm:
        write(tr(storm_ ? kOn : kOff));
        return storm_;
      case Status::Sort:
        write(tr(kSortNames[sort_mode_]));
        return true;
      case Status::Colliders:
        write(tr(debug_ ? kOn : kOff));
        return debug_;
      case Status::Volume:
        std::snprintf(buffer.data(), buffer.size(), "%d%%", static_cast<int>(std::lround(volume_ * 100.0f)));
        return !muted_;
      case Status::Mute:
        write(tr(muted_ ? kOn : kOff));
        return muted_;
      case Status::None:
        break;
    }
    buffer[0] = '\0';
    return false;
  }

  /**
   * @brief 在屏幕右侧绘制操作说明：按键、功能和当前状态三列对齐。
   *
   */
  void drawControls(engine::RenderFrame& frame) const {
    const auto screen = engine::CoordinateSpace::Screen;
    const float scale = fontScale();
    const float line_height = font().glyphHeight() * scale + 6.0f;
    const engine::Color panel{0.0f, 0.0f, 0.0f, 0.55f}, title{1.0f, 0.85f, 0.4f, 1.0f};
    constexpr float top = 110.0f, gap = 18.0f;  // 位于帧时间曲线下方

    if (!show_help_) {
      const char* hint = tr(kHelpHidden);
      const float width = font().measure(hint, scale).x;
      frame.drawRectangle({viewport_width_ - width - 40.0f, top - 8.0f, width + 20.0f, line_height + 10.0f, panel}, screen, kLayerPanel);
      drawText(frame, hint, viewport_width_ - width - 30.0f, top, title);
      return;
    }

    std::array<std::array<char, 32>, std::size(kControls)> states{};
    std::array<bool, std::size(kControls)> active{};
    float key_width = 0.0f, action_width = 0.0f, state_width = 0.0f;
    for (std::size_t index = 0; index < std::size(kControls); ++index) {
      active[index] = statusText(kControls[index].status, states[index]);
      key_width = std::max(key_width, font().measure(tr(kControls[index].key), scale).x);
      action_width = std::max(action_width, font().measure(tr(kControls[index].action), scale).x);
      state_width = std::max(state_width, font().measure(states[index].data(), scale).x);
    }

    const float width = std::max(key_width + gap + action_width + gap + state_width, font().measure(tr(kHelpTitle), scale).x);
    const float height = line_height * (std::size(kControls) + 1);
    const float left = viewport_width_ - width - 30.0f;
    frame.drawRectangle({left - 10.0f, top - 8.0f, width + 20.0f, height + 10.0f, panel}, screen, kLayerPanel);
    drawText(frame, tr(kHelpTitle), left, top, title);

    for (std::size_t index = 0; index < std::size(kControls); ++index) {
      const float y = top + line_height * (index + 1);
      const float key_x = left + key_width - font().measure(tr(kControls[index].key), scale).x;  // 按键列右对齐
      frame.drawRectangle({key_x - 4.0f, y - 2.0f, left + key_width - key_x + 8.0f, line_height - 4.0f, {1.0f, 1.0f, 1.0f, 0.12f}}, screen, kLayerPanel);
      drawText(frame, tr(kControls[index].key), key_x, y, {0.55f, 0.85f, 1.0f, 1.0f});
      drawText(frame, tr(kControls[index].action), left + key_width + gap, y, {0.92f, 0.94f, 1.0f, 1.0f});
      if (states[index][0] != '\0') {
        const engine::Color color = active[index] ? engine::Color{0.4f, 1.0f, 0.5f, 1.0f} : engine::Color{0.6f, 0.6f, 0.65f, 1.0f};
        drawText(frame, states[index].data(), left + key_width + gap + action_width + gap, y, color);
      }
    }
  }

  /**
   * @brief 绘制屏幕空间准星。
   *
   */
  void drawCursor(engine::RenderFrame& frame) const {
    const engine::Color color{1.0f, 1.0f, 1.0f, 0.85f};
    frame.drawRectangle({mouse_screen_.x - 8.0f, mouse_screen_.y - 1.0f, 16.0f, 2.0f, color}, engine::CoordinateSpace::Screen, kLayerCursor);
    frame.drawRectangle({mouse_screen_.x - 1.0f, mouse_screen_.y - 8.0f, 2.0f, 16.0f, color}, engine::CoordinateSpace::Screen, kLayerCursor);
  }

 private:
  engine::Assets* assets_ = nullptr;  // 资源管理器，用于运行期加载纹理

  engine::TextureHandle idle_texture_, run_texture_, attack_texture_, hurt_texture_;  // 角色精灵表
  engine::TextureHandle disc_texture_, glow_texture_, crate_texture_, ground_texture_;  // 程序化纹理
  engine::TextureHandle spike_texture_, sky_texture_, hills_texture_;                  // 程序化纹理
  std::optional<engine::BitmapFont> pixel_font_, ttf_font_;                            // 像素字体和中文字体

  engine::SoundHandle jump_sound_, hit_sound_, coin_sound_, hurt_sound_, spawn_sound_, music_sound_;  // 程序化音效
  engine::VoiceHandle music_voice_;                                                                   // 背景音乐声部

  engine::PhysicsWorld world_{0.0f, kGravity, 64.0f};  // 物理世界
  std::vector<Body> bodies_;                           // 场景刚体
  std::vector<engine::BodyHandle> query_, pending_;    // 查询结果和待销毁刚体，跨帧复用容量
  engine::BodyHandle player_, mover_;                  // 玩家和移动平台
  std::size_t prop_count_ = 0;                         // 可交互物体数量
  int coins_ = 0, coins_total_ = 0;                    // 已拾取和总金币数

  engine::SpriteAnimation idle_animation_{10, 1, 10, 10.0f};           // 待机动画
  engine::SpriteAnimation run_animation_{16, 1, 16, 18.0f};            // 奔跑动画
  engine::SpriteAnimation attack_animation_{7, 1, 7, 20.0f, false};    // 攻击动画
  engine::SpriteAnimation hurt_animation_{4, 1, 4, 10.0f, false};      // 受伤动画
  bool facing_left_ = false, running_ = false, grounded_ = false;      // 玩家朝向和运动状态
  bool attacking_ = false, attack_hit_done_ = false, hurting_ = false;  // 玩家动作状态
  float attack_timer_ = 0.0f, hurt_timer_ = 0.0f;                      // 攻击计时和无敌剩余时间

  std::vector<Particle> particles_;  // 粒子池
  std::vector<Star> stars_;          // 星空
  Random random_{0x9E3779B9u};       // 随机数生成器

  engine::Camera2D camera_{0.0f, 0.0f, 1.5f};                 // 逻辑相机
  engine::Point2D camera_center_;                             // 相机中心的世界坐标
  float target_zoom_ = 1.5f, shake_ = 0.0f;                   // 目标缩放和震动强度
  float viewport_width_ = 1280.0f, viewport_height_ = 720.0f;  // 最近一帧的视口尺寸
  float view_left_ = 0, view_top_ = 0, view_right_ = 0, view_bottom_ = 0;  // 本帧可见的世界范围
  engine::Point2D mouse_screen_, mouse_world_;                // 鼠标的屏幕和世界坐标

  bool laser_active_ = false, laser_hit_ = false;  // 激光状态
  engine::Point2D laser_from_, laser_to_;          // 激光起点和终点

  int sort_mode_ = static_cast<int>(engine::SortMode::Layer);  // 当前排序模式
  bool debug_ = false, storm_ = false, muted_ = false;         // 功能开关
  bool show_help_ = true;                                      // 是否显示操作说明
  float volume_ = 0.8f;                                        // 主音量
  float time_ = 0.0f;                                          // 游戏时间

  std::chrono::steady_clock::time_point last_draw_ = std::chrono::steady_clock::now();  // 上一帧绘制时间
  float frame_ms_ = 16.7f;                                                              // 平滑后的帧时间
  std::array<float, 120> history_{};                                                    // 帧时间历史
  std::size_t history_index_ = 0;                                                       // 帧时间历史写入位置
  std::optional<std::pair<std::size_t, std::int32_t>> picked_;                          // 鼠标拾取到的精灵索引和层级
};

int main() {
  try {
    runSelfTests();

    engine::Engine application;
    DemoGame game;

    engine::WindowOptions options;
    options.title = "GZX Engine Demo";
    options.width = 1280;
    options.height = 720;
    options.resizable = true;
    options.pause_when_minimized = true;

    application.run(game, engine::SplashOptions{"../Sprites/GZX ENGINE_LOGO.png", 1.0, 0.8, 0.8}, options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Error: %s\n", error.what());
    return 1;
  }
  return 0;
}
