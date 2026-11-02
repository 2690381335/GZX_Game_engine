#include "../include/engine/engine.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include "SDL3/SDL.h"
#include "platform/window.hpp"
#include "render/memory/gpu_allocator.hpp"
#include "render/renderer.hpp"
#include "render/vulkan/device_selection.hpp"
#include "render/vulkan/vulkan_context.hpp"
#include "render/vulkan/vulkan_device.hpp"

namespace engine {
/**
 * @brief 显示启动画面。
 *
 * @param window 窗口实例
 * @param renderer 渲染器实例
 * @param texture 启动画面纹理的句柄
 * @param image 启动画面图像的数据
 * @param options 启动画面的显示选项
 * @return true 启动画面显示成功
 * @return false 启动画面显示失败
 */
bool showSplash(platform::Window& window, render::Renderer& renderer, TextureHandle texture, const Assets::ImageData& image, const SplashOptions& options) {
  using Clock = std::chrono::steady_clock;

  Input input;                                                                // 当前帧的输入状态
  RenderFrame frame;                                                          // 当前帧的渲染信息
  auto previous = Clock::now();                                               // 上一帧的时间点
  double elapsed = 0.0;                                                       // 当前帧的时间增量（秒）
  const double duration = options.fade_in + options.hold + options.fade_out;  // 启动画面的总持续时间（秒）

  while (elapsed < duration) {
    if (!window.pollEvents(input)) return false;  // 如果窗口事件轮询返回 false，则退出启动画面

    if (!SDL_GetWindowSizeInPixels(window.nativeHandle(), &frame.viewport_width, &frame.viewport_height)) {
      throw std::runtime_error("Failed to get window size in pixels");
    }  // 如果获取窗口像素尺寸失败，则抛出异常

    if ((SDL_GetWindowFlags(window.nativeHandle()) & SDL_WINDOW_MINIMIZED) || frame.viewport_width <= 0 || frame.viewport_height <= 0) {
      SDL_Delay(20);            // 如果窗口最小化或尺寸无效，则延迟一段时间再继续循环
      previous = Clock::now();  // 更新上一帧的时间点
      continue;                 // 如果窗口最小化或尺寸无效，则跳过本帧的渲染
    }

    const auto now = Clock::now();                                     // 当前帧的时间点
    elapsed += std::chrono::duration<double>(now - previous).count();  // 计算当前帧的时间增量（秒）
    previous = now;                                                    // 更新上一帧的时间点

    if (elapsed >= duration) break;  // 如果启动画面已显示足够的时间，则退出循环

    frame.reset();                                // 重置当前帧的渲染信息
    frame.background = {0.0f, 0.0f, 0.0f, 1.0f};  // 设置背景颜色为黑色，完全不透明

    const float scale = std::min(float(frame.viewport_width) / image.width, float(frame.viewport_height) / image.height);  // 计算图像在窗口中适应的缩放比例
    const float width = image.width * scale, height = image.height * scale;                                                // 计算图像在窗口中的显示尺寸
    const float alpha = float(std::min({1.0, elapsed / options.fade_in, (duration - elapsed) / options.fade_out}));        // 计算图像的透明度，根据淡入和淡出时间调整

    frame.drawSprite(texture,
                     {(frame.viewport_width - width) * 0.5f, (frame.viewport_height - height) * 0.5f, width, height},  // 图像在窗口中的显示区域
                     {},                                                                                               // 纹理的裁剪区域，空表示使用整个纹理
                     {1.0f, 1.0f, 1.0f, alpha});                                                                       // 绘制图像精灵，应用计算得到的透明度
    if (!renderer.render(frame)) SDL_Delay(20);                                                                        // 如果渲染失败，则延迟一段时间再继续循环
  }
  return true;
}

void Engine::run(Game& game, const SplashOptions& splash, const WindowOptions& window_options) {
  game.quit_requested_ = false;  // 重置游戏的退出请求标志

  if (!splash.image.empty() && (!std::isfinite(splash.fade_in) || !std::isfinite(splash.hold) || !std::isfinite(splash.fade_out) || splash.fade_in <= 0 || splash.hold <= 0 || splash.fade_out <= 0)) {
    throw std::invalid_argument("Invalid splash screen timing parameters");
  }  // 检查启动画面时间参数的有效性

  engine::platform::Window window(window_options);  // 创建窗口实例，使用指定的窗口选项

  engine::Audio audio;  // 创建音频混音器，必须在窗口之后创建、之前销毁，因为窗口析构时会调用 SDL_Quit
  struct AudioBinding {
    Game& game;
    ~AudioBinding() { game.audio_ = nullptr; }  // 任何退出路径下都解除游戏对混音器的引用
  } audio_binding{game};
  game.audio_ = &audio;  // 将混音器暴露给游戏对象

  engine::render::VulkanContext context(window.nativeHandle());  // 创建Vulkan上下文实例

  const auto selected = engine::render::selectDevice(context.instance(), context.surface());  // 选择合适的Vulkan设备

  engine::render::VulkanDevice device(selected);  // 创建Vulkan设备实例

  engine::render::GpuAllocator allocator(context.instance(), device);  // 创建GPU内存分配器实例

  engine::Assets assets;                                               // 创建游戏资源管理实例
  TextureHandle logo;                                                  // 用于存储启动画面纹理的句柄
  if (!splash.image.empty()) logo = assets.loadTexture(splash.image);  // 如果启动画面图片路径不为空，则加载启动画面纹理

  game.load(assets);  // 加载游戏所需的资源

  if (game.quitRequested()) return;  // 如果游戏在加载资源后请求退出，则直接返回，用于初始化阶段也可以取消启动

  engine::render::Renderer renderer(device, allocator, context.surface(), window.nativeHandle());  // 创建渲染器实例
  renderer.uploadTextures(assets.images());                                                        // 上传游戏资源中的纹理数据到渲染器
  assets.releasePixels();                                                                          // 释放游戏资源中的像素数据，节省内存

  std::size_t uploaded_images = assets.images().size();  // 记录已上传的图像数量

  if (!splash.image.empty() && !showSplash(window, renderer, logo, assets.images()[logo.index], splash)) {
    std::fprintf(stderr, "Failed to display splash screen\n");
    return;
  }  // 如果启动画面图片路径不为空且显示失败

  using Clock = std::chrono::steady_clock;
  constexpr double step = 1.0 / 60.0;  // 以60帧每秒为目标帧率
  auto previous = Clock::now();        // 上一帧的时间点
  double accumulator = 0.0;            // 累积的时间，用于固定时间步长更新

  Input input;        // 创建用于存储当前帧输入状态的结构体实例
  RenderFrame frame;  // 创建用于存储当前帧渲染信息的结构体实例

  std::array<double, 4> totals{};    // 用于存储各阶段累计时间的数组
  std::uint64_t samples = 0;         // 用于存储采样的帧数
  double peak_ms = 0.0;              // 用于存储帧时间的峰值（毫秒）
  auto report_start = Clock::now();  // 统计报告的起始时间点

  while (!game.quitRequested()) {
    const auto begin = Clock::now();       // 当前帧的开始时间点
    if (!window.pollEvents(input)) break;  // 如果窗口事件轮询返回 false，则退出主循环

    if (!SDL_GetWindowSizeInPixels(window.nativeHandle(), &frame.viewport_width, &frame.viewport_height)) {
      throw std::runtime_error("Failed to get window size in pixels");
    }  // 如果获取窗口像素尺寸失败，则抛出异常

    const bool minimized = (SDL_GetWindowFlags(window.nativeHandle()) & SDL_WINDOW_MINIMIZED) != 0;  // 判断窗口是否最小化
    if ((window_options.pause_when_minimized && minimized) || frame.viewport_width <= 0 || frame.viewport_height <= 0) {
      input.releaseAll();  // 释放所有按键输入状态
      input.clearEdges();  // 清除输入的边缘状态
      SDL_Delay(20);       // 延迟一段时间以降低CPU占用

      previous = Clock::now();  // 更新上一帧的时间点
      accumulator = 0.0;        // 重置累积时间
      totals.fill(0.0);         // 重置各阶段累计时间
      samples = 0;              // 重置采样的帧数
      peak_ms = 0.0;            // 重置帧时间的峰值
      report_start = previous;  // 重置统计报告的起始时间点
      continue;                 // 跳过当前帧，继续下一帧的循环
    }

    const auto events_end = Clock::now();                                                 // 事件处理结束的时间点
    const double elapsed = std::chrono::duration<double>(events_end - previous).count();  // 计算事件处理所消耗的时间（秒）
    previous = events_end;                                                                // 更新上一帧的时间点
    accumulator += std::min(elapsed, step * 5.0);                                         // 累积时间，限制最大步长避免过大时间跳变

    while (accumulator >= step && !game.quitRequested()) {
      game.update(static_cast<float>(step), input);  // 更新游戏逻辑，使用固定时间步长和当前输入状态
      input.clearEdges();                            // 清除输入的边缘状态，为下一帧的输入做准备
      accumulator -= step;                           // 减去已处理的时间步长
    }  // 如果累积时间足够且游戏未请求退出，则更新游戏逻辑

    if (game.quitRequested()) break;  // 如果游戏请求退出，则跳出主循环

    const auto update_end = Clock::now();  // 游戏逻辑更新结束的时间点

    frame.reset();  // 重置当前帧的渲染信息，为下一帧做准备

    if (assets.images().size() != uploaded_images) {
      renderer.uploadTextures(assets.images());  // 上传尚未上传的纹理数据
      assets.releasePixels(uploaded_images);     // 释放已上传的图像像素数据以节省内存
      uploaded_images = assets.images().size();  // 更新已上传的图像数量
    }  // 如果有新的图像需要上传，则进行上传处理

    game.draw(frame);  // 绘制当前帧的游戏内容

    if (game.quitRequested()) break;  // 如果游戏请求退出，则跳出主循环

    const auto prepare_end = Clock::now();  // 当前帧的渲染准备结束的时间点

    if (!renderer.render(frame)) SDL_Delay(20);  // 如果渲染失败，则延迟一段时间以避免过高的CPU占用
    const auto end = Clock::now();               // 当前帧的渲染结束的时间点

    const std::array points{begin, events_end, update_end, prepare_end, end};  // 当前帧各阶段的时间点数组
    for (std::size_t stage = 0; stage < totals.size(); ++stage) {
      totals[stage] += std::chrono::duration<double, std::milli>(points[stage + 1] - points[stage]).count();  // 累加各阶段的时间（毫秒）
    }

    const double loop_ms = std::chrono::duration<double, std::milli>(end - begin).count();  // 当前帧的总时间（毫秒）
    peak_ms = std::max(peak_ms, loop_ms);                                                   // 更新帧时间的峰值（毫秒）
    ++samples;                                                                              // 增加采样的帧数

    const double seconds = std::chrono::duration<double>(end - report_start).count();  // 计算自统计报告起始时间点以来的秒数
    if (seconds >= 1.0) {
      const double count = static_cast<double>(samples);  // 当前统计周期内的帧数
      std::fprintf(stdout, "Loop CPU ms: events=%.3f update=%.3f prepare=%.3f render_wait=%.3f peak=%.3f loop_hz=%.1f\n",
                   totals[0] / count,
                   totals[1] / count,
                   totals[2] / count,
                   totals[3] / count,
                   peak_ms,
                   count / seconds);
      totals.fill(0.0);
      samples = 0;
      peak_ms = 0.0;
      report_start = Clock::now();
    }
  }
}
}  // namespace engine