#include "texture_2d.hpp"

#include <stdexcept>

namespace engine::render {
Texture2D::Texture2D(const VulkanDevice& device, const GpuAllocator& allocator, std::uint32_t width, std::uint32_t height) : device_(device.handle()), width_(width), height_(height), allocator_(allocator.handle()) {
  if (!width || !height) throw std::runtime_error("Texture width and height must be greater than 0.");  // 检查纹理的宽度和高度是否大于 0

  const auto check = [](VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan operation failed.");
  };

  VkImageFormatProperties supported{};                                                                                                                                // 用于存储图像格式的支持属性
  const auto usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;                                                                                    // 指定图像的使用方式，作为传输目标和采样使用
  check(vkGetPhysicalDeviceImageFormatProperties(device.physicalDevice(), format(), VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &supported));                // 检查图像格式是否受支持
  if (width > supported.maxExtent.width || height > supported.maxExtent.height) throw std::runtime_error("Texture dimensions exceed the maximum supported extent.");  // 检查纹理的宽度和高度是否超过支持的最大范围

  try {
    VkImageCreateInfo image{};                          // 用于创建 Vulkan 图像的结构体
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;  // 指定结构体类型为 Vulkan 图像创建信息
    image.imageType = VK_IMAGE_TYPE_2D;                 // 指定图像类型为二维图像
    image.format = format();                            // 指定图像的格式
    image.extent = {width_, height_, 1};                // 指定图像的宽度、高度和深度
    image.mipLevels = 1;                                // 指定图像的 mip 级别数量
    image.arrayLayers = 1;                              // 指定图像的数组层数
    image.samples = VK_SAMPLE_COUNT_1_BIT;              // 指定图像的采样数量
    image.tiling = VK_IMAGE_TILING_OPTIMAL;             // 指定图像的布局方式
    image.usage = usage;                                // 指定图像的使用方式
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;      // 指定图像的共享模式
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;    // 指定图像的初始布局

    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocation.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    check(vmaCreateImage(allocator_, &image, &allocation, &image_, &allocation_, nullptr));  // 使用 VMA 创建 Vulkan 图像并检查是否成功

    VkImageViewCreateInfo view{};                                     // 用于创建 Vulkan 图像视图的结构体
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;            // 指定结构体类型
    view.image = image_;                                              // 指定要创建视图的 Vulkan 图像
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;                            // 指定视图类型为二维图像视图
    view.format = format();                                           // 指定视图的格式
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};  // 指定子资源范围，包括 mip 级别和数组层的起始和数量
    check(vkCreateImageView(device_, &view, nullptr, &view_));        // 创建 Vulkan 图像视图并检查是否成功
  } catch (...) {
    release();  // 释放已分配的 Vulkan 资源
    throw;      // 重新抛出异常
  }
}

Texture2D::~Texture2D() {
  release();  // 释放已分配的 Vulkan 资源
}

void Texture2D::release() noexcept {
  if (view_) vkDestroyImageView(device_, view_, nullptr);        // 销毁 Vulkan 图像视图
  if (image_) vmaDestroyImage(allocator_, image_, allocation_);  // 销毁 Vulkan 图像及其分配的内存
  view_ = VK_NULL_HANDLE;                                        // 重置 Vulkan 图像视图句柄
  image_ = VK_NULL_HANDLE;                                       // 重置 Vulkan 图像句柄
  allocation_ = VK_NULL_HANDLE;                                  // 重置 Vulkan 内存句柄
}
}  // namespace engine::render