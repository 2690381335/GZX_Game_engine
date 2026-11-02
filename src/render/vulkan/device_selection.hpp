#pragma once

#include "vulkan/vulkan.h"

namespace engine::render {
/**
 * @brief 设备选择结构体，用于存储选定的物理设备和队列族索引。
 *
 */
struct DeviceSelection {
  VkPhysicalDevice physical_device;  // 选定的物理设备
  uint32_t queue_family;             // 选定的队列族索引
};

/**
 * @brief 选择合适的物理设备和队列族索引。
 *
 * @param instance Vulkan 实例。
 * @param surface Vulkan 表面。
 * @return DeviceSelection 选定的物理设备和队列族索引。
 */
DeviceSelection selectDevice(VkInstance instance, VkSurfaceKHR surface);
}  // namespace engine::render