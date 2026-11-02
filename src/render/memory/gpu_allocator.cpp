#include "gpu_allocator.hpp"

#include <stdexcept>
#include <string>

namespace engine::render {

GpuAllocator::GpuAllocator(VkInstance instance, const VulkanDevice& device) {
  VmaAllocatorCreateInfo info{};                  // 初始化 Vulkan 内存分配器创建信息结构体
  info.instance = instance;                       // 设置 Vulkan 实例句柄
  info.physicalDevice = device.physicalDevice();  // 设置 Vulkan 物理设备句柄
  info.device = device.handle();                  // 设置 Vulkan 逻辑设备句柄
  info.vulkanApiVersion = VK_API_VERSION_1_0;     // 设置 Vulkan API 版本

  const VkResult result = vmaCreateAllocator(&info, &allocator_);                                                          // 创建 Vulkan 内存分配器实例
  if (result != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan GPU allocator: " + std::to_string(result));  // 检查 Vulkan 内存分配器创建结果
}

GpuAllocator::~GpuAllocator() {
  if (allocator_) vmaDestroyAllocator(allocator_);  // 销毁 Vulkan 内存分配器实例
}
}  // namespace engine::render