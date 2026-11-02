#pragma once

#include "gpu_allocator.hpp"

namespace engine::render {
/**
 * @brief 表示用于向 GPU 传输数据的暂存缓冲区。
 *
 */
class StagingBuffer {
 public:
  /**
   * @brief 构建一个新的暂存缓冲区对象。
   *
   * @param allocator 用于分配 GPU 内存的分配器。
   * @param capacity 暂存缓冲区的容量。
   */
  StagingBuffer(const GpuAllocator& allocator, VkDeviceSize capacity);

  /**
   * @brief 销毁暂存缓冲区对象。
   *
   */
  ~StagingBuffer();

  StagingBuffer(const StagingBuffer&) = delete;
  StagingBuffer& operator=(const StagingBuffer&) = delete;

  /**
   * @brief 获取暂存缓冲区的 Vulkan 缓冲区句柄。
   *
   * @return VkBuffer 暂存缓冲区的 Vulkan 缓冲区句柄。
   */
  VkBuffer handle() const noexcept { return buffer_; }

  /**
   * @brief 获取暂存缓冲区映射的内存指针。
   *
   * @return void* 暂存缓冲区映射的内存指针。
   */
  void* data() const noexcept { return mapped_; }

  /**
   * @brief 获取暂存缓冲区的容量。
   *
   * @return VkDeviceSize 暂存缓冲区的容量。
   */
  VkDeviceSize capacity() const noexcept { return capacity_; }

  /**
   * @brief 刷新暂存缓冲区，将映射的内存内容同步到 GPU。
   *
   */
  void flush();

 private:
  VmaAllocator allocator_;                     // 用于此暂存缓冲区的分配器
  VkDeviceSize capacity_;                      // 暂存缓冲区的容量
  VkBuffer buffer_ = VK_NULL_HANDLE;           // 暂存缓冲区的 Vulkan 缓冲区句柄
  VmaAllocation allocation_ = VK_NULL_HANDLE;  // 暂存缓冲区的分配信息
  void* mapped_ = nullptr;                     // 暂存缓冲区映射的内存指针
};
}  // namespace engine::render