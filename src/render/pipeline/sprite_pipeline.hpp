#pragma once

#include "vulkan/vulkan.h"

namespace engine::render {
class SpritePipeline {
 public:
  /**
   * @brief 构建一个新的精灵管线对象，着色器代码在构建时内嵌进库中。
   *
   * @param device Vulkan 逻辑设备句柄
   * @param pass Vulkan 渲染通道句柄
   * @param texture_layout Vulkan 纹理描述符集布局句柄
   */
  SpritePipeline(VkDevice device, VkRenderPass pass, VkDescriptorSetLayout texture_layout);

  /**
   * @brief 销毁精灵管线对象。
   *
   */
  ~SpritePipeline();

  SpritePipeline(const SpritePipeline&) = delete;
  SpritePipeline& operator=(const SpritePipeline&) = delete;
  SpritePipeline(SpritePipeline&&) = delete;
  SpritePipeline& operator=(SpritePipeline&&) = delete;

  /**
   * @brief 获取精灵管线的 Vulkan 管线句柄。
   *
   * @return VkPipeline Vulkan 管线句柄。
   */
  VkPipeline handle() const noexcept { return pipeline_; }

  /**
   * @brief 获取精灵管线的 Vulkan 管线布局句柄。
   *
   * @return VkPipelineLayout Vulkan 管线布局句柄。
   */
  VkPipelineLayout layout() const noexcept { return layout_; }

 private:
  /**
   * @brief 释放精灵管线占用的 Vulkan 资源。
   *
   */
  void release() noexcept;

 private:
  VkDevice device_;                           // Vulkan 逻辑设备句柄
  VkPipelineLayout layout_ = VK_NULL_HANDLE;  // Vulkan 管线布局句柄
  VkPipeline pipeline_ = VK_NULL_HANDLE;      // Vulkan 管线句柄
};
}  // namespace engine::render