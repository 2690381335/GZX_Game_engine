#pragma once

#include <span>
#include <vector>

#include "vulkan_device.hpp"

namespace engine::render {
/**
 * @brief 表示一个 Vulkan 交换链 (Swapchain)，用于管理图像的呈现和显示。
 *
 */
class Swapchain {
 public:
  /**
   * @brief 初始化 Vulkan 交换链 (Swapchain) 对象。
   *
   * @param device Vulkan 设备对象。
   * @param surface Vulkan 表面对象。
   * @param pixels 交换链的像素尺寸。
   */
  Swapchain(const VulkanDevice& device, VkSurfaceKHR surface, VkExtent2D pixels);

  /**
   * @brief 销毁 Vulkan 交换链 (Swapchain) 对象。
   *
   */
  ~Swapchain();

  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  /**
   * @brief 获取 Vulkan 交换链 (Swapchain) 的句柄。
   *
   * @return VkSwapchainKHR Vulkan 交换链的句柄。
   */
  VkSwapchainKHR handle() const noexcept { return swapchain_; }

  /**
   * @brief 获取 Vulkan 交换链 (Swapchain) 中的图像列表。span 意思是一个轻量级的视图，用于表示一段连续的内存区域，这里用于表示交换链图像列表的只读视图，不拥有底层数据的所有权。
   *
   * @return std::span<const VkImage> Vulkan 交换链中的图像列表。
   */
  std::span<const VkImage, std::dynamic_extent> images() const noexcept { return images_; }

  /**
   * @brief 获取 Vulkan 交换链 (Swapchain) 中图像的格式。
   *
   * @return VkFormat Vulkan 交换链中图像的格式。
   */
  VkFormat format() const noexcept { return format_; }

  /**
   * @brief 获取 Vulkan 交换链 (Swapchain) 中图像的尺寸。
   *
   * @return VkExtent2D Vulkan 交换链中图像的尺寸。
   */
  VkExtent2D extent() const noexcept { return extent_; }

 private:
  VkDevice device_;                            // Vulkan 设备的句柄
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;  // Vulkan 交换链的句柄
  std::vector<VkImage> images_;                // 交换链中的图像列表
  VkFormat format_;                            // 交换链图像的格式
  VkExtent2D extent_{};                        // 交换链图像的尺寸
};
}  // namespace engine::render