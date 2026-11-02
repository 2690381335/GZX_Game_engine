#include "draw_frame.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace engine::render {
DrawResult drawFrame(const VulkanDevice& device,
                     const GpuAllocator& allocator,
                     const Swapchain& swapchain,
                     FrameResources& frames,
                     const RenderPass& pass,
                     const RenderTargets& targets,
                     const SpritePipeline& pipeline,
                     const engine::RenderFrame& frame,
                     std::span<const SpriteInstance> instances,
                     std::span<const DrawBatch> batches,
                     std::span<const TextureDescriptors::Binding> bindings) {
  constexpr bool profile = true;  // 是否启用性能分析
  using Clock = std::chrono::steady_clock;
  std::array<Clock::time_point, 7> times{};  // 用于记录每帧的关键时间点，用于性能分析
  const auto mark = [&](std::size_t stage) {
    if constexpr (profile) times[stage] = Clock::now();  // if constexpr意思是编译时常量条件，如果profile为true则记录时间点，如果为false大括号内的代码会被编译器忽略
  };

  mark(0);  // 标记帧开始的时间点

  const auto check = [](VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan error: " + std::to_string(result));
  };

  auto& slot = frames.slots_[frames.current_];                                   // 获取当前帧的资源槽
  const std::size_t required = instances.size_bytes();                           // 当前帧所需的实例数据字节数
  check(vkWaitForFences(device.handle(), 1, &slot.fence, VK_TRUE, UINT64_MAX));  // 等待帧的栅栏，确保上一帧渲染完成
  mark(1);                                                                       // 标记等待帧栅栏完成的时间点

  if constexpr (profile) {
    if (slot.timestamps_pending) {           // 如果时间戳查询挂起，则获取查询结果
      std::array<std::uint64_t, 2> ticks{};  // 用于存储时间戳查询结果
      check(vkGetQueryPoolResults(device.handle(), slot.timestamps, 0, 2, sizeof(ticks), ticks.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT));

      const auto mask = std::numeric_limits<std::uint64_t>::max() >> (64 - frames.timestamp_bits_);  // 计算时间戳查询的掩码
      const auto delta = (ticks[1] - ticks[0]) & mask;                                               // 计算时间戳查询的差值并应用掩码
      frames.gpu_total_ms_ += static_cast<double>(delta) * frames.timestamp_ns_ / 1'000'000.0;       // 将时间戳差值转换为毫秒并累加到 GPU 总计时间
      ++frames.gpu_samples_;                                                                         // 增加 GPU 样本数量
      slot.timestamps_pending = false;                                                               // 重置时间戳查询挂起状态
    }
  }

  if (required > slot.instances->capacity()) {
    const auto current = slot.instances->capacity();                                                          // 当前实例缓冲区的容量
    const auto doubled = current <= std::numeric_limits<std::size_t>::max() / 2 ? current * 2 : current;      // 将当前实例缓冲区容量翻倍，如果翻倍后超过最大值则保持当前容量
    slot.instances = std::make_unique<InstanceBuffer>(allocator, std::max<VkDeviceSize>(required, doubled));  // 创建新的实例缓冲区，容量为计算得到的新的容量
  }  // 如果所需实例数量超过当前实例缓冲区容量，则创建新的实例缓冲区

  uint32_t index = 0;
  const VkResult acquired = vkAcquireNextImageKHR(device.handle(), swapchain.handle(), 1000000000ULL, slot.acquired, VK_NULL_HANDLE, &index);  // 获取下一帧的图像索引
  if (acquired == VK_ERROR_OUT_OF_DATE_KHR) return DrawResult{.presented = false, .recreate = true};                                           // 交换链过时，需要重新创建
  if (acquired == VK_TIMEOUT || acquired == VK_NOT_READY) return DrawResult{.presented = false, .recreate = false};                            // 获取图像超时或尚未准备好，跳过当前帧
  if (acquired != VK_SUBOPTIMAL_KHR) check(acquired);                                                                                          // 检查是否获取图像成功，非最优情况下也需要检查错误
  mark(2);                                                                                                                                     // 标记获取下一帧图像索引的时间点
  slot.instances->upload(std::as_bytes(instances));                                                                                            // 上传精灵实例数据到GPU缓冲区
  mark(3);                                                                                                                                     // 标记上传实例数据的时间点
  check(vkResetCommandBuffer(slot.commands, 0));                                                                                               // 重置命令缓冲区，为新一帧的渲染做准备

  VkCommandBufferBeginInfo begin{};                           // 命令缓冲区开始信息，用于记录新一帧的渲染命令
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;  // 指定结构体类型为命令缓冲区开始信息
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 指定命令缓冲区仅用于一次提交，优化性能
  check(vkBeginCommandBuffer(slot.commands, &begin));         // 开始记录命令缓冲区，为新一帧的渲染命令做准备

  if constexpr (profile) {
    if (slot.timestamps) {
      vkCmdResetQueryPool(slot.commands, slot.timestamps, 0, 2);                                  // 重置时间戳查询池，准备记录新的时间戳
      vkCmdWriteTimestamp(slot.commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, slot.timestamps, 0);  // 记录时间戳查询的起始时间
    }
  }

  VkClearValue clear{};  // 清除值，用于清空帧缓冲区
  const auto& background = frame.background;
  clear.color = {{background.red, background.green, background.blue, background.alpha}};  // 设置清除颜色值

  VkRenderPassBeginInfo render_begin{};                           // 渲染通道开始信息，用于开始新的渲染通道
  render_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;  // 指定结构体类型为渲染通道开始信息
  render_begin.renderPass = pass.handle();                        // 设置渲染通道句柄
  render_begin.framebuffer = targets.framebuffer(index);          // 设置帧缓冲区
  render_begin.renderArea.extent = swapchain.extent();            // 设置渲染区域的范围
  render_begin.clearValueCount = 1;                               // 清除值的数量
  render_begin.pClearValues = &clear;                             // 清除值的指针

  vkCmdBeginRenderPass(slot.commands, &render_begin, VK_SUBPASS_CONTENTS_INLINE);  // 开始渲染通道

  if (!instances.empty()) {
    const auto extent = swapchain.extent();                                                                                  // 获取交换链的渲染区域范围
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};  // 设置视口
    const VkRect2D scissor{{0, 0}, extent};                                                                                  // 设置裁剪矩形
    vkCmdSetViewport(slot.commands, 0, 1, &viewport);                                                                        // 设置视口
    vkCmdSetScissor(slot.commands, 0, 1, &scissor);                                                                          // 设置裁剪矩形
    vkCmdBindPipeline(slot.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());                                    // 绑定图形管线

    const VkBuffer buffer = slot.instances->handle();               // 获取实例缓冲区的句柄
    const VkDeviceSize offset = 0;                                  // 实例缓冲区的偏移量
    vkCmdBindVertexBuffers(slot.commands, 0, 1, &buffer, &offset);  // 绑定实例缓冲区

    for (std::size_t batch_index = 0; batch_index < batches.size(); ++batch_index) {
      const auto& batch = batches[batch_index];                               // 当前批次
      const auto* previous = batch_index ? &batches[batch_index - 1] : nullptr;  // 上一批次，用于跳过冗余的状态切换

      if (!previous || previous->space != batch.space) {
        const bool world = batch.space == engine::CoordinateSpace::World;  // 判断当前批次的坐标空间是否为世界坐标
        const float constants[]{static_cast<float>(extent.width),
                                static_cast<float>(extent.height),
                                world ? frame.camera.x : 0.0f,
                                world ? frame.camera.y : 0.0f,
                                world ? frame.camera.zoom : 1.0f};  // 构建用于顶点着色器的常量数组
        vkCmdPushConstants(slot.commands, pipeline.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), constants);  // 推送常量到顶点着色器阶段
      }

      if (!previous || previous->texture != batch.texture) {
        const VkDescriptorSet set = bindings[batch.texture].set;                                                             // 获取当前纹理对应的描述符集
        vkCmdBindDescriptorSets(slot.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), 0, 1, &set, 0, nullptr);  // 纹理变化时才重新绑定描述符集
      }
      vkCmdDraw(slot.commands, 6, batch.count, 0, batch.first);  // 绘制当前批次的精灵实例
    }
  }

  vkCmdEndRenderPass(slot.commands);  // 结束渲染通道

  if constexpr (profile) {
    if (slot.timestamps) {
      vkCmdWriteTimestamp(slot.commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, slot.timestamps, 1);  // 记录时间戳查询的结束时间
    }
  }

  check(vkEndCommandBuffer(slot.commands));  // 结束命令缓冲区的记录
  mark(4);                                   // 标记结束命令缓冲区记录的时间点

  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // 指定等待的管线阶段为颜色附件输出阶段
  const VkSemaphore finished = frames.finished_[index];                                   // 获取当前帧的完成信号量

  VkSubmitInfo submit{};                                         // 提交信息，用于将命令缓冲区提交到队列
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;                  // 指定结构体类型为提交信息
  submit.waitSemaphoreCount = 1;                                 // 等待信号量的数量
  submit.pWaitSemaphores = &slot.acquired;                       // 等待的信号量
  submit.pWaitDstStageMask = &wait_stage;                        // 等待的管线阶段
  submit.commandBufferCount = 1;                                 // 提交的命令缓冲区数量
  submit.pCommandBuffers = &slot.commands;                       // 提交的命令缓冲区
  submit.signalSemaphoreCount = 1;                               // 信号量的数量
  submit.pSignalSemaphores = &finished;                          // 信号量
  check(vkResetFences(device.handle(), 1, &slot.fence));         // 重置帧的栅栏，为新一帧的渲染做准备
  check(vkQueueSubmit(device.queue(), 1, &submit, slot.fence));  // 提交命令缓冲区到队列，开始新一帧的渲染

  if constexpr (profile) slot.timestamps_pending = slot.timestamps != VK_NULL_HANDLE;  // 如果启用性能分析，设置时间戳查询挂起状态

  mark(5);                                                         // 标记提交命令缓冲区到队列的时间点
  frames.current_ = (frames.current_ + 1) % frames.slots_.size();  // 更新当前帧索引，循环使用帧资源槽

  const VkSwapchainKHR handle = swapchain.handle();                                                 // 获取交换链的句柄
  VkPresentInfoKHR present{};                                                                       // 呈现信息，用于将渲染结果提交到交换链
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;                                               // 指定结构体类型为呈现信息
  present.waitSemaphoreCount = 1;                                                                   // 等待信号量的数量
  present.pWaitSemaphores = &finished;                                                              // 等待的信号量
  present.swapchainCount = 1;                                                                       // 交换链的数量
  present.pSwapchains = &handle;                                                                    // 交换链
  present.pImageIndices = &index;                                                                   // 图像索引
  const VkResult result = vkQueuePresentKHR(device.queue(), &present);                              // 提交呈现请求到队列
  if (result == VK_ERROR_OUT_OF_DATE_KHR) return DrawResult{.presented = false, .recreate = true};  // 交换链过时，需要重新创建
  if (result != VK_SUBOPTIMAL_KHR) check(result);                                                   // 检查呈现结果是否为错误，忽略次优情况
  mark(6);                                                                                          // 标记提交呈现请求到队列的时间点

  if constexpr (profile) {
    for (std::size_t stage = 0; stage < frames.cpu_totals_.size(); ++stage) {
      frames.cpu_totals_[stage] += std::chrono::duration<double, std::milli>(times[stage + 1] - times[stage]).count();  // 累加每个阶段的 CPU 时间
    }
    frames.report_elapsed_ += std::chrono::duration<double>(times[6] - times[0]).count();  // 累加总的 CPU 时间
    ++frames.samples_;                                                                     // 累加采样次数

    if (frames.report_elapsed_ >= 1.0) {
      const double count = frames.samples_;  // 获取当前采样次数
      std::fprintf(stdout, "CPU ms: fence=%.3f, acquire=%.3f, upload=%.3f, record=%.3f, submit=%.3f, present=%.3f\n",
                   frames.cpu_totals_[0] / count,
                   frames.cpu_totals_[1] / count,
                   frames.cpu_totals_[2] / count,
                   frames.cpu_totals_[3] / count,
                   frames.cpu_totals_[4] / count,
                   frames.cpu_totals_[5] / count);
      frames.cpu_totals_.fill(0.0);  // 重置每个阶段的 CPU 时间统计
      frames.report_elapsed_ = 0.0;  // 重置总的 CPU 时间统计
      frames.samples_ = 0;           // 重置采样次数

      if (frames.gpu_samples_ != 0) {
        std::fprintf(stdout, "GPU interval: %.3f ms (%u samples)\n",
                     frames.gpu_total_ms_ / frames.gpu_samples_,
                     frames.gpu_samples_);
        frames.gpu_total_ms_ = 0.0;  // 重置 GPU 总时间统计
        frames.gpu_samples_ = 0;     // 重置 GPU 采样次数
      }
    }
  }

  return DrawResult{.presented = true, .recreate = acquired == VK_SUBOPTIMAL_KHR || result == VK_SUBOPTIMAL_KHR};  // 返回绘制结果，表示帧已成功呈现
}
}  // namespace engine::render