#include "render_pass.hpp"

#include <stdexcept>
#include <string>

namespace engine::render {
RenderPass::RenderPass(VkDevice device, VkFormat format) : device_(device) {
  VkAttachmentDescription attachment{};                          // 渲染通道的附件描述结构体
  attachment.format = format;                                    // 附件的图像格式
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;                    // 附件的采样数，通常为1
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // 渲染通道开始时的加载操作
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             // 渲染通道结束时的存储操作
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    // 模板缓冲区开始时的加载操作
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 模板缓冲区结束时的存储操作
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          // 附件的初始布局
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // 附件的最终布局

  VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};  // 附件引用，指定附件索引和布局
  VkSubpassDescription subpass{};                                                // 子通道描述结构体
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;                   // 子通道绑定的管线类型
  subpass.colorAttachmentCount = 1;                                              // 子通道的颜色附件数量
  subpass.pColorAttachments = &reference;                                        // 子通道的颜色附件引用

  VkSubpassDependency dependency{};                                                                       // 子通道依赖结构体
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;                                                            // 源子通道
  dependency.dstSubpass = 0;                                                                              // 目标子通道
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;                                // 源阶段掩码
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;                                // 目标阶段掩码
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;  // 目标访问掩码

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;  // 结构体类型
  info.attachmentCount = 1;                                // 附件数量
  info.pAttachments = &attachment;                         // 附件描述
  info.subpassCount = 1;                                   // 子通道数量
  info.pSubpasses = &subpass;                              // 子通道描述
  info.dependencyCount = 1;                                // 子通道依赖数量
  info.pDependencies = &dependency;                        // 子通道依赖描述

  const VkResult result = vkCreateRenderPass(device_, &info, nullptr, &pass_);                                    // 创建渲染通道
  if (result != VK_SUCCESS) throw std::runtime_error("Failed to create render pass: " + std::to_string(result));  // 检查渲染通道创建结果
}

RenderPass::~RenderPass() {
  vkDestroyRenderPass(device_, pass_, nullptr);  // 销毁渲染通道
}
}  // namespace engine::render