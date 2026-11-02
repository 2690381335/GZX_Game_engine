#pragma once

#include "device_selection.hpp"

namespace engine::render {
/**
 * @brief Vulkan 设备类，用于管理 Vulkan 逻辑设备和队列。
 *
 */
class VulkanDevice {
 public:
  /**
   * @brief 创建 Vulkan 设备对象。
   *
   * @param selected 设备选择信息。
   */
  explicit VulkanDevice(DeviceSelection selected);

  /**
   * @brief 销毁 Vulkan 设备对象。
   *
   */
  ~VulkanDevice();

  VulkanDevice(const VulkanDevice&) = delete;
  VulkanDevice& operator=(const VulkanDevice&) = delete;

  /**
   * @brief 获取 Vulkan 逻辑设备句柄。
   *
   * @return VkDevice Vulkan 逻辑设备句柄。
   */
  VkDevice handle() const noexcept { return device_; }

  /**
   * @brief 获取 Vulkan 队列句柄。
   *
   * @return VkQueue Vulkan 队列句柄。
   */
  VkQueue queue() const noexcept { return queue_; }

  /**
   * @brief 获取 Vulkan 物理设备句柄。
   *
   * @return VkPhysicalDevice Vulkan 物理设备句柄。
   */
  VkPhysicalDevice physicalDevice() const noexcept { return selected_.physical_device; }

  /**
   * @brief 获取 Vulkan 队列族索引。
   *
   * @return uint32_t Vulkan 队列族索引。
   */
  uint32_t queueFamily() const noexcept { return selected_.queue_family; }

 private:
  DeviceSelection selected_;          // 设备选择信息，包括物理设备和队列族索引
  VkDevice device_ = VK_NULL_HANDLE;  // Vulkan 逻辑设备的句柄
  VkQueue queue_ = VK_NULL_HANDLE;    // Vulkan 队列的句柄
};
}  // namespace engine::render