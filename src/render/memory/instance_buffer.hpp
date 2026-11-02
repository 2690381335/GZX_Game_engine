#pragma once

#include <cstddef>
#include <span>

#include "gpu_allocator.hpp"

namespace engine::render {
/**
 * @brief 表示用于存储矩形实例的 Vulkan 实例缓冲区。
 *
 */
class InstanceBuffer {
 public:
  /**
   * @brief 构造一个新的实例缓冲区对象。
   *
   * @param allocator 用于分配 Vulkan 资源的 GPU 分配器。
   * @param capacity 实例缓冲区的容量（可容纳的矩形数量）。
   */
  InstanceBuffer(const GpuAllocator& allocator, VkDeviceSize capacity);

  /**
   * @brief 析构实例缓冲区对象。
   *
   */
  ~InstanceBuffer();

  InstanceBuffer(const InstanceBuffer&) = delete;
  InstanceBuffer& operator=(const InstanceBuffer&) = delete;

  /**
   * @brief 将字节数据上传到实例缓冲区。
   *
   * @param bytes 要上传的字节数据。
   */
  void upload(std::span<const std::byte> bytes);  // span意思是表示一段连续的内存区域，这里用于传递字节数据。

  /**
   * @brief 获取 Vulkan 实例缓冲区的句柄。
   *
   * @return VkBuffer Vulkan 实例缓冲区的句柄。
   */
  VkBuffer handle() const noexcept { return buffer_; }

  /**
   * @brief 获取实例缓冲区的容量（以字节为单位）。
   *
   * @return VkDeviceSize 实例缓冲区的容量（以字节为单位）。
   */
  VkDeviceSize capacity() const noexcept { return capacity_; }

 private:
  VmaAllocator allocator_;                     // 用于分配 Vulkan 资源的 GPU 分配器
  VkDeviceSize capacity_;                      // 实例缓冲区的容量（以字节为单位）
  VkBuffer buffer_ = VK_NULL_HANDLE;           // Vulkan 实例缓冲区句柄
  VmaAllocation allocation_ = VK_NULL_HANDLE;  // Vulkan 实例缓冲区的内存分配句柄
  void* mapped_ = nullptr;                     // 映射到主机可访问内存的指针
};
}  // namespace engine::render