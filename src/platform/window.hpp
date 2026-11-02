#pragma once

#include "../../include/engine/window_options.hpp"

struct SDL_Window;

namespace engine {
class Input;
}

namespace engine::platform {
/**
 * @brief 窗口类，用于管理 SDL 窗口的创建、销毁和事件处理。
 * 
 */
class Window {
 public:
  /**
   * @brief 构造一个新的 Window 对象。
   * 
   * @param options 窗口的配置选项。
   */
  explicit Window(const WindowOptions& options = {});

  /**
   * @brief 销毁窗口对象。
   *
   */
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  /**
   * @brief 轮询窗口事件，处理用户输入和窗口消息。
   *
   * @param input 用于接收用户输入的输入对象。
   * @return true 如果有事件被处理。
   * @return false 如果没有事件被处理。
   */
  bool pollEvents(Input& input);

  /**
   * @brief 获取原生 SDL 窗口句柄。
   *
   * @return SDL_Window* 原生 SDL 窗口句柄。
   */
  SDL_Window* nativeHandle() const noexcept { return window_; }

 private:
  SDL_Window* window_ = nullptr;  // 原生 SDL 窗口句柄
};
}  // namespace engine::platform