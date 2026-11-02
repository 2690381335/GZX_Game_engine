#pragma once

#include <string>

namespace engine {
/**
 * @brief 窗口选项结构体，用于配置引擎窗口的属性。
 * @details 该结构体允许您在引擎中创建窗口时指定标题、尺寸、是否可调整大小以及最小化时是否暂停。
 *          所有选项都提供了默认值。
 */
struct WindowOptions {
  std::string title = "Engine";      // 窗口标题
  int width = 1280;                  // 窗口宽度
  int height = 720;                  // 窗口高度
  bool resizable = true;             // 窗口是否可调整大小
  bool pause_when_minimized = true;  // 窗口最小化时是否暂停
};
}  // namespace engine