#pragma once

#include <filesystem>
#include <stdexcept>

#include "assets.hpp"
#include "audio.hpp"
#include "bitmap_font.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "render_frame.hpp"
#include "window_options.hpp"

namespace engine {
/**
 * @brief 表示游戏的抽象基类，定义了游戏逻辑更新和渲染的接口。
 *
 */
class Game {
 public:
  virtual ~Game() = default;

  /**
   * @brief 加载游戏所需的资源。
   *
   */
  virtual void load(Assets&) {}

  /**
   * @brief 更新游戏逻辑。
   *
   * @param delta_time 游戏逻辑更新的时间增量（秒）。
   * @param input 当前帧的输入状态。
   */
  virtual void update(float delta_time, const Input& input) = 0;

  /**
   * @brief 绘制当前帧的游戏画面。
   * @param render_frame 当前帧的渲染信息。
   */
  virtual void draw(RenderFrame&) {}

  /**
   * @brief 请求退出游戏。
   *
   */
  void requestQuit() noexcept {
    quit_requested_ = true;  // 设置退出请求标志
  }

  /**
   * @brief 检查游戏是否请求退出。
   *
   * @return true 如果游戏请求退出。
   * @return false 如果游戏未请求退出。
   */
  bool quitRequested() const noexcept {
    return quit_requested_;
  }

  /**
   * @brief 获取引擎创建的音频混音器，仅在 Engine::run 运行期间（包括 load）有效。
   *
   * @return Audio& 音频混音器的引用。
   */
  Audio& audio() const {
    if (!audio_) throw std::logic_error("Audio is only available while the engine is running");
    return *audio_;
  }

 private:
  friend class Engine;           // 允许 Engine 类访问 Game 类的私有成员，包括 quit_requested_
  bool quit_requested_ = false;  // 标记游戏是否请求退出
  Audio* audio_ = nullptr;       // 指向引擎音频混音器的指针，由 Engine 在运行期间设置
};

/**
 * @brief 启动画面选项，包括图片路径和淡入、保持、淡出时间。
 *
 */
struct SplashOptions {
  std::filesystem::path image;                     // 启动画面的图片路径
  double fade_in = 6, hold = 1.0, fade_out = 2.5;  // 启动画面的淡入、保持和淡出时间（秒）
};

/**
 * @brief 负责运行应用程序循环的主引擎类。
 *
 */
class Engine {
 public:
  /**
   * @brief 运行应用程序的主循环。
   *
   * @param game 游戏对象的引用。
   * @param splash 启动画面选项。
   * @param window_options 窗口选项。
   */
  void run(Game& game, const SplashOptions& splash = {}, const WindowOptions& window_options = {});
};
}  // namespace engine