#pragma once

#include "../vulkan/vulkan_device.hpp"

namespace engine::render {
/**
 * @brief 表示一个 Vulkan 纹理集布局。
 *
 */
class TextureSetLayout {
 public:
  /**
   * @brief 构造一个新的 TextureSetLayout 对象。
   *
   * @param device Vulkan 设备。
   */
  explicit TextureSetLayout(const VulkanDevice& device);

  /**
   * @brief 销毁 TextureSetLayout 对象。
   *
   */
  ~TextureSetLayout();

  TextureSetLayout(const TextureSetLayout&) = delete;
  TextureSetLayout& operator=(const TextureSetLayout&) = delete;

  /**
   * @brief 获取 Vulkan 描述符集布局句柄。
   *
   * @return VkDescriptorSetLayout Vulkan 描述符集布局句柄。
   */
  VkDescriptorSetLayout handle() const noexcept { return layout_; }

 private:
  VkDevice device_;                                // Vulkan 设备句柄
  VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;  // 描述符集布局句柄
};
}  // namespace engine::render