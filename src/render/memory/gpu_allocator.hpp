#pragma once

#include "vma/vk_mem_alloc.h"  // Windows / Linux 的 Vulkan SDK 布局一致：Include/vma/vk_mem_alloc.h
#include "../vulkan/vulkan_device.hpp"

namespace engine::render {
/**
 * @brief Vulkan GPU 内存分配器的封装类。
 *
 */
class GpuAllocator {
 public:
  /**
   * @brief 构造 Vulkan GPU 内存分配器对象。
   *
   * @param instance Vulkan 实例。
   * @param device Vulkan 设备封装对象。
   */
  GpuAllocator(VkInstance instance, const VulkanDevice& device);

  /**
   * @brief 销毁 Vulkan GPU 内存分配器对象。
   *
   */
  ~GpuAllocator();

  GpuAllocator(const GpuAllocator&) = delete;             // 禁止拷贝构造
  GpuAllocator& operator=(const GpuAllocator&) = delete;  // 禁止拷贝赋值
  GpuAllocator(GpuAllocator&&) = delete;                  // 禁止移动构造
  GpuAllocator& operator=(GpuAllocator&&) = delete;       // 禁止移动赋值

  /**
   * @brief 获取 Vulkan GPU 内存分配器的句柄。
   *
   * @return VmaAllocator Vulkan GPU 内存分配器的句柄。
   */
  VmaAllocator handle() const noexcept { return allocator_; }

 private:
  VmaAllocator allocator_ = VK_NULL_HANDLE;  // Vulkan 内存分配器句柄
};
}  // namespace engine::render