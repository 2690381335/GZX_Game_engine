#include "texture_descriptors.hpp"

#include <algorithm>
#include <stdexcept>

namespace engine::render {
TextureDescriptors::TextureDescriptors(const VulkanDevice& device, const TextureSetLayout& layout, std::uint32_t sets_per_page)
    : device_(device.handle()), layout_(layout.handle()), sets_per_page_(sets_per_page) {
  if (!sets_per_page_) throw std::invalid_argument("sets_per_page must be greater than 0");  // 检查每页的描述符集数量是否大于0
}

TextureDescriptors::~TextureDescriptors() {
  for (const auto& page : pages_) {
    vkDestroyDescriptorPool(device_, page.pool, nullptr);  // 销毁每一页的描述符池
  }
}

TextureDescriptors::Binding TextureDescriptors::allocate(VkImageView view, VkSampler sampler) {
  if (!view || !sampler) throw std::invalid_argument("view and sampler must be valid Vulkan handles");                     // 检查传入的 Vulkan 图像视图和采样器句柄是否有效
  auto page = std::find_if(pages_.begin(), pages_.end(), [](const Page& candidate) { return candidate.available != 0; });  // 查找有可用描述符集的页面
  if (page == pages_.end()) {
    addPage();                // 如果没有可用的页面，则添加一页新的描述符池
    page = pages_.end() - 1;  // 获取新添加的页面的迭代器
  }

  VkDescriptorSetAllocateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.descriptorPool = page->pool;
  info.descriptorSetCount = 1;
  info.pSetLayouts = &layout_;
  VkDescriptorSet set = VK_NULL_HANDLE;
  if (vkAllocateDescriptorSets(device_, &info, &set)) throw std::runtime_error("Failed to allocate descriptor set");  // 分配描述符集失败时抛出异常
  --page->available;                                                                                                  // 分配成功后，减少该页面的可用描述符集数量
  VkDescriptorImageInfo image{sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};                               // 创建描述符图像信息结构体

  VkWriteDescriptorSet write{};                                      // 创建描述符写入结构体，用于更新描述符集
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;              // 设置结构体类型为写入描述符集
  write.dstSet = set;                                                // 设置目标描述符集
  write.dstBinding = 0;                                              // 设置目标绑定点
  write.descriptorCount = 1;                                         // 设置描述符数量
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;  // 设置描述符类型
  write.pImageInfo = &image;                                         // 设置图像信息指针
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);            // 更新描述符集
  return {set, page->pool};                                          // 返回分配的描述符集和所属的描述符池
}

void TextureDescriptors::free(Binding& binding) {
  if (!binding.set) return;
  auto page = std::find_if(pages_.begin(), pages_.end(), [&](const Page& candidate) { return candidate.pool == binding.pool; });              // 查找与绑定的描述符集所属的描述符池匹配的页面
  if (page == pages_.end()) throw std::runtime_error("Failed to find descriptor pool for freeing descriptor set");                            // 如果未找到匹配的描述符池，则抛出异常
  if (vkFreeDescriptorSets(device_, binding.pool, 1, &binding.set) != VK_SUCCESS) throw std::runtime_error("Failed to free descriptor set");  // 释放描述符集失败时抛出异常
  ++page->available;                                                                                                                          // 释放描述符集后，增加该页面的可用描述符集数量
  binding = {};
}

void TextureDescriptors::addPage() {
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets_per_page_};  // 创建描述符池大小结构体，指定描述符类型和数量

  VkDescriptorPoolCreateInfo info{};                                                                                                       // 创建描述符池创建信息结构体
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;                                                                              // 设置结构体类型为描述符池创建信息
  info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;                                                                          // 设置描述符池创建标志，允许单独释放描述符集
  info.maxSets = sets_per_page_;                                                                                                           // 设置描述符池中最大描述符集数量
  info.poolSizeCount = 1;                                                                                                                  // 设置描述符池大小结构体数量
  info.pPoolSizes = &size;                                                                                                                 // 设置描述符池大小结构体指针
  VkDescriptorPool pool = VK_NULL_HANDLE;                                                                                                  // 初始化描述符池句柄为无效值
  if (vkCreateDescriptorPool(device_, &info, nullptr, &pool) != VK_SUCCESS) throw std::runtime_error("Failed to create descriptor pool");  // 创建描述符池失败时抛出异常

  try {
    pages_.push_back({pool, sets_per_page_});  // 将新创建的描述符池添加到页面列表中
  } catch (...) {
    vkDestroyDescriptorPool(device_, pool, nullptr);  // 如果添加页面失败，销毁新创建的描述符池以释放资源
    throw;
  }
}
}  // namespace engine::render