#pragma once

#include "vulkan/vulkan.h"

namespace engine::render {
/**
 * @brief Vulkan 渲染通道封装类。
 * 
 */
class RenderPass {
 public:
  /**
   * @brief 构造 Vulkan 渲染通道对象。
   * 
   * @param device Vulkan 设备句柄。
   * @param format 渲染通道的图像格式。
   */
  RenderPass(VkDevice device, VkFormat format);

  /**
   * @brief 销毁 Vulkan 渲染通道对象。
   * 
   */
  ~RenderPass();

  RenderPass(const RenderPass&) = delete;            // 禁止拷贝构造
  RenderPass& operator=(const RenderPass&) = delete;  // 禁止拷贝赋值

  /**
   * @brief 获取 Vulkan 渲染通道句柄。
   * 
   * @return VkRenderPass Vulkan 渲染通道句柄。
   */
  VkRenderPass handle() const noexcept { return pass_; }

 private:
  VkDevice device_;                     // Vulkan 设备句柄
  VkRenderPass pass_ = VK_NULL_HANDLE;  // Vulkan 渲染通道句柄
};
}  // namespace engine::render