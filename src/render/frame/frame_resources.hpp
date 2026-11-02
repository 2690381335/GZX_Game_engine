#pragma once

#include <array>
#include <memory>
#include <vector>

#include "../memory/gpu_allocator.hpp"
#include "../memory/instance_buffer.hpp"
#include "../vulkan/vulkan_device.hpp"

namespace engine::render {
/**
 * @brief 表示单个帧的 Vulkan 资源，包括命令池、命令缓冲区、信号量、栅栏和实例缓冲区。
 *
 */
struct FrameSlot {
  VkCommandPool pool = VK_NULL_HANDLE;        // 命令池
  VkCommandBuffer commands = VK_NULL_HANDLE;  // 命令缓冲区
  VkSemaphore acquired = VK_NULL_HANDLE;      // 图像获取信号量
  VkFence fence = VK_NULL_HANDLE;             // 栅栏
  std::unique_ptr<InstanceBuffer> instances;  // 实例缓冲区

  VkQueryPool timestamps = VK_NULL_HANDLE;  // 时间戳查询池
  bool timestamps_pending = false;          // 时间戳查询是否挂起
};

/**
 * @brief 封装 Vulkan 帧资源管理，包括多个帧资源槽、渲染完成信号量以及 CPU 和 GPU 时间统计。
 *
 */
class FrameResources {
 public:
  /**
   * @brief 构造一个新的 Frame Resources 对象。
   *
   * @param device Vulkan 设备对象。
   * @param allocator GPU 分配器对象。
   * @param image_count 图像数量。
   */
  FrameResources(const VulkanDevice& device, const GpuAllocator& allocator, uint32_t image_count);

  /**
   * @brief 销毁 Vulkan 帧资源对象。
   *
   */
  ~FrameResources();

  FrameResources(const FrameResources&) = delete;
  FrameResources& operator=(const FrameResources&) = delete;

 private:
  /**
   * @brief 释放 Vulkan 帧资源。
   *
   */
  void release() noexcept;

 public:
  std::array<FrameSlot, 2> slots_;      // 帧资源槽数组
  std::vector<VkSemaphore> finished_;   // 渲染完成信号量列表
  std::size_t current_ = 0;             // 当前帧索引
  std::array<double, 6> cpu_totals_{};  // CPU 总计时间数组，用于记录每帧的 CPU 时间消耗
  double report_elapsed_ = 0.0;         // 报告的已消耗时间
  uint32_t samples_ = 0;                // 样本数量

  uint32_t timestamp_bits_ = 0;  // 时间戳查询的位掩码
  double timestamp_ns_ = 0.0;    // 时间戳查询的纳秒数
  double gpu_total_ms_ = 0.0;    // GPU 总计时间（毫秒）
  uint32_t gpu_samples_ = 0;     // GPU 样本数量

 private:
  VkDevice device_;  // Vulkan 设备句柄
};
}  // namespace engine::render