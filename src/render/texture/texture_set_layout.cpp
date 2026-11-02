#include "texture_set_layout.hpp"

#include <stdexcept>

namespace engine::render {
TextureSetLayout::TextureSetLayout(const VulkanDevice& device) : device_(device.handle()) {
  VkDescriptorSetLayoutBinding binding{};                              // 描述符集布局绑定信息
  binding.binding = 0;                                                 // 绑定点索引，表示该绑定在着色器中的位置
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;  // 描述符类型为组合图像采样器
  binding.descriptorCount = 1;                                         // 描述符数量，表示该绑定点上有多少个描述符
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;                   // 指定该绑定在片段着色器阶段可见

  VkDescriptorSetLayoutCreateInfo info{};                            // 描述符集布局创建信息结构体
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;  // 指定结构体类型为描述符集布局创建信息结构体
  info.bindingCount = 1;                                             // 描述符集布局绑定数量
  info.pBindings = &binding;                                         // 指向描述符集布局绑定信息的指针

  if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &layout_) != VK_SUCCESS) throw std::runtime_error("Failed to create descriptor set layout");  // 创建描述符集布局失败
}

TextureSetLayout::~TextureSetLayout() {
  if (layout_) vkDestroyDescriptorSetLayout(device_, layout_, nullptr);  // 销毁描述符集布局
}
}  // namespace engine::render