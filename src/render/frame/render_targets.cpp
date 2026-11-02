#include "render_targets.hpp"

#include <stdexcept>

namespace engine::render {
RenderTargets::RenderTargets(VkDevice device, const Swapchain& swapchain, const RenderPass& pass)
    : device_(device), views_(swapchain.images().size(), VK_NULL_HANDLE), buffers_(swapchain.images().size(), VK_NULL_HANDLE) {
  try {
    for (std::size_t index = 0; index < views_.size(); ++index) {
      VkImageViewCreateInfo view{};                                                                                                           // 初始化图像视图创建信息结构体
      view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;                                                                                  // 设置结构体类型为图像视图创建信息结构体
      view.image = swapchain.images()[index];                                                                                                 // 设置图像视图所关联的图像
      view.viewType = VK_IMAGE_VIEW_TYPE_2D;                                                                                                  // 设置图像视图类型为二维图像视图
      view.format = swapchain.format();                                                                                                       // 设置图像视图的格式
      view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};                                                                        // 设置图像视图的子资源范围
      if (vkCreateImageView(device_, &view, nullptr, &views_[index]) != VK_SUCCESS) throw std::runtime_error("Failed to create image view");  // 创建图像视图并检查是否成功

      VkFramebufferCreateInfo info{};                                                                                                              // 初始化帧缓冲创建信息结构体
      info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;                                                                                      // 设置结构体类型为帧缓冲创建信息结构体
      info.renderPass = pass.handle();                                                                                                             // 设置帧缓冲所关联的渲染通道
      info.attachmentCount = 1;                                                                                                                    // 设置帧缓冲的附件数量
      info.pAttachments = &views_[index];                                                                                                          // 设置帧缓冲的附件
      info.width = swapchain.extent().width;                                                                                                       // 设置帧缓冲的宽度
      info.height = swapchain.extent().height;                                                                                                     // 设置帧缓冲的高度
      info.layers = 1;                                                                                                                             // 设置帧缓冲的层数
      if (vkCreateFramebuffer(device_, &info, nullptr, &buffers_[index]) != VK_SUCCESS) throw std::runtime_error("Failed to create framebuffer");  // 创建帧缓冲并检查是否成功
    }
  } catch (...) {
    release();
    throw;
  }
}

RenderTargets::~RenderTargets() {
  (void)vkDeviceWaitIdle(device_);  // 等待设备空闲，确保所有操作完成后再释放资源
  release();                        // 释放渲染目标相关的资源
}

void RenderTargets::release() noexcept {
  for (const auto buffer : buffers_) {
    if (buffer) vkDestroyFramebuffer(device_, buffer, nullptr);  // 销毁帧缓冲
  }
  for (const auto view : views_) {
    if (view) vkDestroyImageView(device_, view, nullptr);  // 销毁图像视图
  }
}
}  // namespace engine::render