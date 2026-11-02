#include "vulkan_device.hpp"

#include <stdexcept>

namespace engine::render {
VulkanDevice::VulkanDevice(DeviceSelection selected) : selected_(selected) {
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info{};                           // 初始化 Vulkan 队列创建信息结构体
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;  // 设置结构体类型为 Vulkan 队列创建信息结构体类型
  queue_info.queueFamilyIndex = selected_.queue_family;           // 设置队列族索引
  queue_info.queueCount = 1;                                      // 设置队列数量
  queue_info.pQueuePriorities = &priority;                        // 设置队列优先级

  const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};  // 设置设备扩展列表
  VkDeviceCreateInfo info{};                                     // 初始化 Vulkan 设备创建信息结构体
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;             // 设置结构体类型为 Vulkan 设备创建信息结构体类型
  info.queueCreateInfoCount = 1;                                 // 设置队列创建信息数量
  info.pQueueCreateInfos = &queue_info;                          // 设置队列创建信息指针
  info.enabledExtensionCount = 1;                                // 设置设备扩展数量
  info.ppEnabledExtensionNames = extensions;                     // 设置设备扩展列表

  const VkResult result = vkCreateDevice(selected_.physical_device, &info, nullptr, &device_);  // 创建 Vulkan 设备
  if (result != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan device");         // 检查 Vulkan 设备创建结果
  vkGetDeviceQueue(device_, selected_.queue_family, 0, &queue_);                                // 获取 Vulkan 设备队列
}

VulkanDevice::~VulkanDevice() {
  (void)vkDeviceWaitIdle(device_);    // 等待 Vulkan 设备空闲
  vkDestroyDevice(device_, nullptr);  // 销毁 Vulkan 设备
}
}  // namespace engine::render