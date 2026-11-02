#pragma once

#include <array>
#include <cstddef>

namespace engine {
/**
 * @brief 表示键盘按键的枚举类型。
 *
 */
enum class Key : std::size_t {  // enum意思是强类型枚举，底层类型为std::size_t
  Left,                         // 左方向键
  Right,                        // 右方向键
  Up,                           // 上方向键
  Down,                         // 下方向键
  Space,                        // 空格键
  Escape,                       // 退出键
  Enter,                        // 回车键
  Tab,                          // 制表键
  Backspace,                    // 退格键
  LeftShift,                    // 左Shift键
  RightShift,                   // 右Shift键
  LeftCtrl,                     // 左Ctrl键
  RightCtrl,                    // 右Ctrl键
  LeftAlt,                      // 左Alt键
  RightAlt,                     // 右Alt键
  A, B, C, D, E, F, G, H, I, J, K, L, M,                   // 字母键A到M
  N, O, P, Q, R, S, T, U, V, W, X, Y, Z,                   // 字母键N到Z
  Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,  // 主键盘数字键0到9
  F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,          // 功能键F1到F12
  Count                                                    // 键盘按键的数量
};

/**
 * @brief 表示鼠标按键的枚举类型。
 *
 */
enum class MouseButton : std::size_t {
  Left,    // 鼠标左键
  Middle,  // 鼠标中键
  Right,   // 鼠标右键
  X1,      // 鼠标侧键1
  X2,      // 鼠标侧键2
  Count    // 鼠标按键的数量
};

/**
 * @brief 表示输入系统的类，包含键盘和鼠标的状态。
 *
 */
class Input {
 public:
  float mouse_x = 0.0f;                                                                     // 鼠标的水平位置
  float mouse_y = 0.0f;                                                                     // 鼠标的垂直位置
  float wheel_x = 0.0f;                                                                     // 鼠标滚轮的水平滚动量
  float wheel_y = 0.0f;                                                                     // 鼠标滚轮的垂直滚动量
  static constexpr std::size_t key_count = static_cast<std::size_t>(Key::Count);            // 键盘按键的数量
  using States = std::array<bool, key_count>;                                               // 存储每个按键的状态，true表示按下，false表示未按下
  static constexpr std::size_t mouse_count = static_cast<std::size_t>(MouseButton::Count);  // 鼠标按键的数量

  using MouseStates = std::array<bool, mouse_count>;  // 存储每个鼠标按键的状态，true表示按下，false表示未按下

  /**
   * @brief 检查指定按键是否处于按下状态。
   *
   * @param key 要检查的按键。
   * @return true 如果按键处于按下状态。
   * @return false 如果按键未按下。
   */
  bool down(Key key) const noexcept { return read(down_, key); }

  /**
   * @brief 检查指定按键是否在当前帧被按下。
   *
   * @param key 要检查的按键。
   * @return true 如果按键在当前帧被按下。
   * @return false 如果按键在当前帧未被按下。
   */
  bool pressed(Key key) const noexcept { return read(pressed_, key); }

  /**
   * @brief 检查指定按键是否在当前帧被释放。
   *
   * @param key 要检查的按键。
   * @return true 如果按键在当前帧被释放。
   * @return false 如果按键在当前帧未被释放。
   */
  bool released(Key key) const noexcept { return read(released_, key); }

  /**
   * @brief 设置指定按键的按下状态。
   *
   * @param key 要设置的按键。
   * @param held 按键是否被按下，true表示按下，false表示未按下。
   */
  void set(Key key, bool held) noexcept {
    const auto index = static_cast<std::size_t>(key);
    if (index >= key_count || down_[index] == held) return;
    down_[index] = held;
    if (held) {
      pressed_[index] = true;
    } else {
      released_[index] = true;
    }
  }

  /**
   * @brief 检查指定鼠标按键是否处于按下状态。
   *
   * @param button 要检查的鼠标按键。
   * @return true 如果鼠标按键处于按下状态。
   * @return false 如果鼠标按键未按下。
   */
  bool down(MouseButton button) const noexcept { return read(mouse_down_, button); }

  /**
   * @brief 检查指定鼠标按键是否在当前帧被按下。
   *
   * @param button 要检查的鼠标按键。
   * @return true 如果鼠标按键在当前帧被按下。
   * @return false 如果鼠标按键在当前帧未被按下。
   */
  bool pressed(MouseButton button) const noexcept { return read(mouse_pressed_, button); }

  /**
   * @brief 检查指定鼠标按键是否在当前帧被释放。
   *
   * @param button 要检查的鼠标按键。
   * @return true 如果鼠标按键在当前帧被释放。
   * @return false 如果鼠标按键在当前帧未被释放。
   */
  bool released(MouseButton button) const noexcept { return read(mouse_released_, button); }

  /**
   * @brief 设置指定鼠标按键的按下状态。
   *
   * @param button 要设置的鼠标按键。
   * @param held 鼠标按键是否被按下，true表示按下，false表示未按下。
   */
  void set(MouseButton button, bool held) noexcept {
    const auto index = static_cast<std::size_t>(button);
    if (index >= mouse_count || mouse_down_[index] == held) return;
    mouse_down_[index] = held;
    if (held) {
      mouse_pressed_[index] = true;
    } else {
      mouse_released_[index] = true;
    }
  }

  /**
   * @brief 释放所有按键和鼠标按键，将它们的状态设置为未按下。
   *
   */
  void releaseAll() noexcept {
    for (std::size_t index = 0; index < key_count; ++index) {
      set(static_cast<Key>(index), false);
    }

    for (std::size_t index = 0; index < mouse_count; ++index) {
      set(static_cast<MouseButton>(index), false);
    }
  }

  /**
   * @brief 清除所有按键和鼠标按键的按下事件状态和释放事件状态，同时重置鼠标滚轮的滚动量。
   * 
   */
  void clearEdges() noexcept {
    wheel_x = 0.0f;               // 清除鼠标滚轮的水平滚动量
    wheel_y = 0.0f;               // 清除鼠标滚轮的垂直滚动量
    pressed_.fill(false);         // 清除所有按键的按下事件状态，fill意思是将数组的所有元素设置为指定的值
    released_.fill(false);        // 清除所有按键的释放事件状态，fill意思是将数组的所有元素设置为指定的值
    mouse_pressed_.fill(false);   // 清除所有鼠标按键的按下事件状态
    mouse_released_.fill(false);  // 清除所有鼠标按键的释放事件状态
  }

 private:
  /**
   * @brief 从给定的按键状态中读取指定按键的状态。
   *
   * @param states 每个按键的状态数组。
   * @param key 要读取的按键。
   * @return true 如果按键处于按下状态。
   * @return false 如果按键未按下。
   */
  static bool read(const States& states, Key key) noexcept {
    const auto index = static_cast<std::size_t>(key);  // 将按键枚举值转换为数组索引
    return index < key_count && states[index];         // 如果索引在有效范围内，返回按键的状态，否则返回false
  }

  /**
   * @brief 从给定的鼠标按键状态中读取指定鼠标按键的状态。
   *
   * @param states 每个鼠标按键的状态数组。
   * @param button 要读取的鼠标按键。
   * @return true 如果鼠标按键处于按下状态。
   * @return false 如果鼠标按键未按下。
   */
  static bool read(const MouseStates& states, MouseButton button) noexcept {
    const auto index = static_cast<std::size_t>(button);  // 将鼠标按键枚举值转换为数组索引
    return index < mouse_count && states[index];          // 如果索引在有效范围内，返回鼠标按键的状态，否则返回false
  }

 private:
  States down_{}, pressed_{}, released_{};                         // 存储每个按键的当前按下状态、按下事件状态和释放事件状态
  MouseStates mouse_down_{}, mouse_pressed_{}, mouse_released_{};  // 存储每个鼠标按键的当前按下状态、按下事件状态和释放事件状态
};
}  // namespace engine