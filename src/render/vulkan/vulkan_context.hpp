#pragma once

#include "vulkan/vulkan.h"

struct SDL_Window;

namespace engine::render {
/**
 * @brief Vulkan 上下文，用于管理 Vulkan 实例和表面。
 *
 */
class VulkanContext {
 public:
  /**
   * @brief 创建 Vulkan 上下文对象。
   *
   * @param window 原生 SDL 窗口句柄。
   */
  explicit VulkanContext(SDL_Window* window);

  /**
   * @brief 销毁 Vulkan 上下文对象。
   *
   */
  ~VulkanContext();

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;

  /**
   * @brief 获取 Vulkan 实例。
   *
   * @return VkInstance Vulkan 实例。
   */
  VkInstance instance() const noexcept { return instance_; }

  /**
   * @brief 获取 Vulkan 表面。
   *
   * @return VkSurfaceKHR Vulkan 表面。
   */
  VkSurfaceKHR surface() const noexcept { return surface_; }

 private:
  VkInstance instance_ = VK_NULL_HANDLE;   // Vulkan 实例
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;  // Vulkan 表面
};
}  // namespace engine::render