#pragma once

#include "../vulkan/vulkan_device.hpp"

namespace engine::render {
/**
 * @brief Vulkan 采样器封装类
 *
 */
class Sampler {
 public:
  /**
   * @brief 构造一个新的采样器对象并创建 Vulkan 采样器。
   *
   * @param device Vulkan 设备封装对象
   */
  explicit Sampler(const VulkanDevice& device);

  /**
   * @brief 销毁采样器对象并清理 Vulkan 采样器。
   *
   */
  ~Sampler();

  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;

  /**
   * @brief 获取 Vulkan 采样器句柄。
   *
   * @return VkSampler Vulkan 采样器句柄
   */
  VkSampler handle() const noexcept { return sampler_; }

 private:
  VkDevice device_;                     // Vulkan 设备句柄
  VkSampler sampler_ = VK_NULL_HANDLE;  // Vulkan 采样器句柄
};
}  // namespace engine::render