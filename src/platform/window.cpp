#include "window.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

#include "../../include/engine/input.hpp"
#include "SDL3/SDL.h"

namespace engine::platform {
namespace {
constexpr std::uint8_t kUnmapped = 0xFF;  // 表示扫描码没有对应的引擎按键
static_assert(Input::key_count < kUnmapped, "Key count must fit in the scancode lookup table");

/**
 * @brief 在编译期构建 SDL 扫描码到引擎按键的查找表，运行时按键映射为 O(1) 数组访问。
 *
 * @return std::array<std::uint8_t, SDL_SCANCODE_COUNT> 扫描码到按键索引的查找表。
 */
consteval std::array<std::uint8_t, SDL_SCANCODE_COUNT> buildKeyTable() {
  std::array<std::uint8_t, SDL_SCANCODE_COUNT> table{};
  table.fill(kUnmapped);  // 默认所有扫描码均未映射

  const auto map = [&](SDL_Scancode scancode, Key key) { table[scancode] = static_cast<std::uint8_t>(key); };

  map(SDL_SCANCODE_LEFT, Key::Left);
  map(SDL_SCANCODE_RIGHT, Key::Right);
  map(SDL_SCANCODE_UP, Key::Up);
  map(SDL_SCANCODE_DOWN, Key::Down);
  map(SDL_SCANCODE_SPACE, Key::Space);
  map(SDL_SCANCODE_ESCAPE, Key::Escape);
  map(SDL_SCANCODE_RETURN, Key::Enter);
  map(SDL_SCANCODE_TAB, Key::Tab);
  map(SDL_SCANCODE_BACKSPACE, Key::Backspace);
  map(SDL_SCANCODE_LSHIFT, Key::LeftShift);
  map(SDL_SCANCODE_RSHIFT, Key::RightShift);
  map(SDL_SCANCODE_LCTRL, Key::LeftCtrl);
  map(SDL_SCANCODE_RCTRL, Key::RightCtrl);
  map(SDL_SCANCODE_LALT, Key::LeftAlt);
  map(SDL_SCANCODE_RALT, Key::RightAlt);

  for (int offset = 0; offset < 26; ++offset) {
    map(static_cast<SDL_Scancode>(SDL_SCANCODE_A + offset), static_cast<Key>(static_cast<std::size_t>(Key::A) + offset));  // SDL中字母扫描码连续排列
  }
  map(SDL_SCANCODE_0, Key::Num0);  // SDL中数字0的扫描码位于9之后，需要单独映射
  for (int offset = 0; offset < 9; ++offset) {
    map(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + offset), static_cast<Key>(static_cast<std::size_t>(Key::Num1) + offset));  // 数字1到9的扫描码连续排列
  }
  for (int offset = 0; offset < 12; ++offset) {
    map(static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + offset), static_cast<Key>(static_cast<std::size_t>(Key::F1) + offset));  // F1到F12的扫描码连续排列
  }
  return table;
}

constexpr auto kKeyTable = buildKeyTable();  // 扫描码到按键索引的查找表
}  // namespace
Window::Window(const WindowOptions& options) {
  if (options.width <= 0 || options.height <= 0) {
    throw std::invalid_argument("Window width and height must be greater than 0");
  }  // 如果窗口的宽度或高度小于等于0，则抛出异常
  if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error("Failed to initialize SDL");  // 初始化SDL视频子系统，如果失败则抛出异常

  SDL_WindowFlags flags = SDL_WINDOW_VULKAN;             // 设置窗口标志为使用Vulkan渲染
  if (options.resizable) flags |= SDL_WINDOW_RESIZABLE;  // 如果窗口可调整大小，则设置相应的标志

  window_ = SDL_CreateWindow(options.title.c_str(), options.width, options.height, flags);  // 创建SDL窗口，使用指定的标题、宽度、高度和标志

  if (!window_) {
    SDL_Quit();
    throw std::runtime_error("Failed to create SDL window");
  }
}

Window::~Window() {
  SDL_DestroyWindow(window_);
  SDL_Quit();
}

bool Window::pollEvents(Input& input) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) return false;  // 如果接收到退出或窗口关闭请求事件，则返回 false表示窗口应关闭
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) input.releaseAll();                                 // 如果窗口失去焦点，释放所有按键
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {                                                         // 如果事件是鼠标滚轮事件，则处理滚轮的滚动量
      const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;          // 根据滚轮方向决定滚动量的正负
      input.wheel_x += event.wheel.x * direction;                                                      // 更新鼠标滚轮的水平滚动量
      input.wheel_y += event.wheel.y * direction;                                                      // 更新鼠标滚轮的垂直滚动量
      continue;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      MouseButton button;
      switch (event.button.button) {
        case SDL_BUTTON_LEFT:
          button = MouseButton::Left;
          break;
        case SDL_BUTTON_MIDDLE:
          button = MouseButton::Middle;
          break;
        case SDL_BUTTON_RIGHT:
          button = MouseButton::Right;
          break;
        case SDL_BUTTON_X1:
          button = MouseButton::X1;
          break;
        case SDL_BUTTON_X2:
          button = MouseButton::X2;
          break;
        default:
          continue;  // 如果鼠标按键不在枚举范围内，则跳过
      }
      input.set(button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);  // 设置鼠标按键状态
      continue;                                                      // 继续处理下一个事件
    }

    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP) continue;  // 如果事件不是按键按下或释放事件，则跳过
    if (event.key.repeat) continue;                                                    // 如果按键事件是重复按下事件，则跳过

    const auto scancode = static_cast<std::size_t>(event.key.scancode);       // 获取按键扫描码
    if (scancode >= kKeyTable.size() || kKeyTable[scancode] == kUnmapped) continue;  // 如果按键不在枚举范围内，则跳过
    input.set(static_cast<Key>(kKeyTable[scancode]), event.type == SDL_EVENT_KEY_DOWN);  // 设置按键状态
  }

  float mouse_x = 0.0f, mouse_y = 0.0f;   // 初始化鼠标的水平和垂直位置
  SDL_GetMouseState(&mouse_x, &mouse_y);  // 获取当前鼠标的水平和垂直位置

  int logical_width = 0, logical_height = 0;  // 初始化窗口的逻辑宽度和高度
  int pixel_width = 0, pixel_height = 0;      // 初始化窗口的像素宽度和高度
  if (!SDL_GetWindowSize(window_, &logical_width, &logical_height) || !SDL_GetWindowSizeInPixels(window_, &pixel_width, &pixel_height)) {
    throw std::runtime_error("Failed to get window size");  // 如果获取窗口大小失败或像素大小失败，则抛出异常
  }
  if (logical_width > 0 && logical_height > 0) {
    input.mouse_x = mouse_x * static_cast<float>(pixel_width) / logical_width;    // 根据逻辑宽度和像素宽度计算鼠标的水平位置
    input.mouse_y = mouse_y * static_cast<float>(pixel_height) / logical_height;  // 根据逻辑高度和像素高度计算鼠标的垂直位置
  }  // 如果逻辑宽度和逻辑高度大于0，则根据像素宽度和高度计算鼠标位置

  return true;
}
}  // namespace engine::platform