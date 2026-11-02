#pragma once

#include <vector>

#include "../pipeline/render_pass.hpp"
#include "../vulkan/swapchain.hpp"

namespace engine::render {
/**
 * @brief 封装 Vulkan 渲染目标，包括图像视图和帧缓冲。
 *
 */
class RenderTargets {
 public:
  /**
   * @brief 构造渲染目标对象。
   *
   * @param device Vulkan 逻辑设备。
   * @param swapchain 交换链对象。
   * @param pass 渲染通道对象。
   */
  RenderTargets(VkDevice device, const Swapchain& swapchain, const RenderPass& pass);

  /**
   * @brief 销毁渲染目标对象。
   *
   */
  ~RenderTargets();

  RenderTargets(const RenderTargets&) = delete;
  RenderTargets& operator=(const RenderTargets&) = delete;

  /**
   * @brief 获取指定索引的帧缓冲对象。
   *
   * @param index 帧缓冲的索引。
   * @return VkFramebuffer 对应的帧缓冲对象。
   */
  VkFramebuffer framebuffer(std::size_t index) const { return buffers_.at(index); }  // at的意思是访问指定索引的元素，并在索引越界时抛出异常

 private:
  void release() noexcept;

 private:
  VkDevice device_;                     // 与渲染目标相关的 Vulkan 逻辑设备
  std::vector<VkImageView> views_;      // 渲染目标的图像视图
  std::vector<VkFramebuffer> buffers_;  // 渲染目标的帧缓冲区
};
}  // namespace engine::render