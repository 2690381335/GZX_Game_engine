#pragma once

#include <cstdint>
#include <vector>

#include "texture_set_layout.hpp"

namespace engine::render {
/**
 * @brief 管理 Vulkan 描述符集及其关联描述符池的类。
 *
 */
class TextureDescriptors {
 public:
  /**
   * @brief 表示描述符集及其关联描述符池的绑定。
   *
   */
  struct Binding {
    VkDescriptorSet set = VK_NULL_HANDLE;    // 此绑定的描述符集句柄
    VkDescriptorPool pool = VK_NULL_HANDLE;  // 此绑定的描述符池句柄
  };

  /**
   * @brief 构造一个新的 TextureDescriptors 对象。
   *
   * @param device Vulkan 设备句柄。
   * @param layout 描述符集布局。
   * @param sets_per_page 每页的描述符集数量。
   */
  TextureDescriptors(const VulkanDevice& device, const TextureSetLayout& layout, std::uint32_t sets_per_page = 1024);

  /**
   * @brief 销毁 TextureDescriptors 对象。
   *
   */
  ~TextureDescriptors();

  TextureDescriptors(const TextureDescriptors&) = delete;
  TextureDescriptors& operator=(const TextureDescriptors&) = delete;

  /**
   * @brief 分配一个新的描述符集绑定。
   *
   * @param view Vulkan 图像视图句柄。
   * @param sampler Vulkan 采样器句柄。
   * @return Binding 分配的描述符集绑定。
   */
  Binding allocate(VkImageView view, VkSampler sampler);

  /**
   * @brief 释放一个描述符集绑定。
   *
   * @param binding 要释放的描述符集绑定。
   */
  void free(Binding& binding);

 private:
  /**
   * @brief 表示描述符池中的一页描述符集合。
   *
   */
  struct Page {
    VkDescriptorPool pool = VK_NULL_HANDLE;  // 此页面的描述符池句柄
    std::uint32_t available = 0;             // 此页面中可用的描述符集数量
  };

  /**
   * @brief 添加一页新的描述符池。
   *
   */
  void addPage();

 private:
  VkDevice device_;               // Vulkan 设备句柄
  VkDescriptorSetLayout layout_;  // 描述符集布局句柄
  std::uint32_t sets_per_page_;   // 每页的描述符集数量
  std::vector<Page> pages_;       // 描述符池页的集合
};
}  // namespace engine::render