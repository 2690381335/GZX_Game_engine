#pragma once

#include <cstdint>

#include "../memory/gpu_allocator.hpp"
#include "../vulkan/vulkan_device.hpp"

namespace engine::render {
/**
 * @brief 表示 Vulkan 中的二维纹理。
 *
 */
class Texture2D {
 public:
  /**
   * @brief 构造一个新的 Texture2D 对象。
   *
   * @param device Vulkan 设备句柄。
   * @param allocator GPU 分配器。
   * @param width 纹理的宽度。
   * @param height 纹理的高度。
   */
  Texture2D(const VulkanDevice& device, const GpuAllocator& allocator, std::uint32_t width, std::uint32_t height);

  /**
   * @brief 销毁 Texture2D 对象。
   *
   */
  ~Texture2D();

  Texture2D(const Texture2D&) = delete;
  Texture2D& operator=(const Texture2D&) = delete;
  Texture2D(Texture2D&&) = delete;
  Texture2D& operator=(Texture2D&&) = delete;

  /**
   * @brief 获取 Vulkan 图像句柄。
   *
   * @return VkImage Vulkan 图像句柄。
   */
  VkImage image() const noexcept { return image_; }

  /**
   * @brief 获取 Vulkan 图像视图句柄。
   *
   * @return VkImageView Vulkan 图像视图句柄。
   */
  VkImageView view() const noexcept { return view_; }

  /**
   * @brief 获取纹理的宽度。
   *
   * @return std::uint32_t 纹理的宽度。
   */
  std::uint32_t width() const noexcept { return width_; }

  /**
   * @brief 获取纹理的高度。
   *
   * @return std::uint32_t 纹理的高度。
   */
  std::uint32_t height() const noexcept { return height_; }

  /**
   * @brief 获取纹理的格式。
   *
   * @return VkFormat 纹理的格式。
   */
  VkFormat format() const noexcept { return VK_FORMAT_R8G8B8A8_SRGB; }

 private:
  /**
   * @brief 释放 Vulkan 资源。
   *
   */
  void release() noexcept;

 private:
  VkDevice device_;                            // Vulkan 设备句柄
  std::uint32_t width_;                        // 纹理的宽度
  std::uint32_t height_;                       // 纹理的高度
  VkImage image_ = VK_NULL_HANDLE;             // Vulkan 图像句柄
  VmaAllocator allocator_;                     // GPU 分配器
  VmaAllocation allocation_ = VK_NULL_HANDLE;  // GPU 内存分配句柄
  VkImageView view_ = VK_NULL_HANDLE;          // Vulkan 图像视图句柄
};
}  // namespace engine::render