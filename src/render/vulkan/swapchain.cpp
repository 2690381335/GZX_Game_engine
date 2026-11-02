#include "swapchain.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace engine::render {
Swapchain::Swapchain(const VulkanDevice& device, VkSurfaceKHR surface, VkExtent2D pixels) : device_(device.handle()) {
  const auto check = [](VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan swapchain");
  };

  VkSurfaceCapabilitiesKHR caps{};                                                            // 查询给定表面的物理设备的表面能力
  check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice(), surface, &caps));  // 获取表面的能力信息

  uint32_t count = 0;                                                                              // 查询表面格式的数量
  check(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice(), surface, &count, nullptr));  // 获取表面格式的数量
  if (!count) throw std::runtime_error("Failed to get Vulkan surface formats");                    // 如果没有表面格式则抛出异常

  std::vector<VkSurfaceFormatKHR> formats(count);                                                         // 分配用于存储表面格式的向量
  check(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice(), surface, &count, formats.data()));  // 获取表面格式的详细信息
  if (!count) throw std::runtime_error("Failed to get Vulkan surface formats");                           // 如果没有表面格式则抛出异常

  formats.resize(count);                                                                                                        // 调整表面格式向量的大小以匹配实际获取的数量
  auto format = formats.front();                                                                                                // 选择第一个表面格式作为默认格式
  if (formats.size() == 1 && format.format == VK_FORMAT_UNDEFINED && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {  // 如果只有一个表面格式且其格式未定义且颜色空间为非线性 sRGB
    format.format = VK_FORMAT_B8G8R8A8_SRGB;                                                                                    // 设置默认的表面格式为 B8G8R8A8_SRGB
  } else {
    const auto selected = std::find_if(formats.begin(), formats.end(), [](const VkSurfaceFormatKHR& candidate) {
      return candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && (candidate.format == VK_FORMAT_B8G8R8A8_SRGB || candidate.format == VK_FORMAT_R8G8B8A8_SRGB);  // 查找符合条件的表面格式，即颜色空间为非线性 sRGB 且格式为 B8G8R8A8_SRGB 或 R8G8B8A8_SRGB
    });
    if (selected == formats.end()) throw std::runtime_error("Failed to find a suitable Vulkan surface format");  // 如果没有找到符合条件的表面格式则抛出异常
    format = *selected;                                                                                          // 选择找到的符合条件的表面格式
  }

  VkExtent2D extent = caps.currentExtent;                                                               // 使用表面能力中的当前扩展作为交换链的像素尺寸
  if (extent.width == UINT32_MAX) {                                                                     // 如果表面能力中的当前扩展未定义，则使用提供的像素尺寸并进行限制，UINT32_MAX表示最大无符号32位整数值
    extent.width = std::clamp(pixels.width, caps.minImageExtent.width, caps.maxImageExtent.width);      // clamp意思是将像素宽度限制在最小和最大值之间
    extent.height = std::clamp(pixels.height, caps.minImageExtent.height, caps.maxImageExtent.height);  // clamp意思是将像素高度限制在最小和最大值之间
  }
  if (!extent.width || !extent.height) throw std::runtime_error("Invalid swapchain extent");  // 如果交换链的像素尺寸无效则抛出异常

  const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;                                             // 指定交换链图像的使用标志，表示图像将作为颜色附件使用
  if ((caps.supportedUsageFlags & usage) != usage) throw std::runtime_error("Unsupported swapchain image usage");  // 检查交换链图像的使用标志是否被支持

  format_ = format.format;  // 保存交换链图像的格式
  extent_ = extent;         // 保存交换链图像的像素尺寸

  VkCompositeAlphaFlagsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // 指定复合 alpha 模式为不透明

  while (!(caps.supportedCompositeAlpha & alpha)) alpha <<= 1;  // 循环尝试不同的复合 alpha 模式，直到找到支持的模式，<<=意思是将 alpha 左移一位，相当于乘以 2

  VkSwapchainCreateInfoKHR info{};                                                                // 初始化交换链创建信息结构体并设置为零值
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;                                       // 设置结构体类型为交换链创建信息结构体
  info.surface = surface;                                                                         // 设置交换链关联的表面
  info.minImageCount = caps.minImageCount + 1;                                                    // 设置交换链的最小图像数量为表面能力的最小值加一
  if (caps.maxImageCount) info.minImageCount = std::min(info.minImageCount, caps.maxImageCount);  // 如果表面能力中存在最大图像数量，则将最小图像数量限制在最大值之内
  info.imageFormat = format.format;                                                               // 设置交换链图像的格式
  info.imageColorSpace = format.colorSpace;                                                       // 设置交换链图像的颜色空间
  info.imageExtent = extent;                                                                      // 设置交换链图像的像素尺寸
  info.imageArrayLayers = 1;                                                                      // 设置交换链图像的数组层数为1
  info.imageUsage = usage;                                                                        // 设置交换链图像的使用标志
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;                                              // 设置交换链图像的共享模式为独占模式
  info.preTransform = caps.currentTransform;                                                      // 设置交换链图像的预变换
  info.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(alpha);                          // 设置交换链图像的复合 alpha 模式
  info.presentMode = VK_PRESENT_MODE_FIFO_KHR;                                                    // 使用 FIFO 呈现模式，保证垂直同步
  info.clipped = VK_TRUE;                                                                         // 启用裁剪，忽略被其他窗口覆盖的像素
  check(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_));                              // 创建交换链并检查返回结果

  try {
    for (;;) {
      uint32_t image_count = 0;
      check(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr));                          // 获取交换链图像的数量
      if (!image_count) throw std::runtime_error("Failed to get swapchain images");                        // 如果获取到的交换链图像数量为零则抛出异常
      images_.resize(image_count);                                                                         // 调整图像列表的大小以容纳所有交换链图像
      const VkResult result = vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, images_.data());  // 获取交换链图像列表
      if (result == VK_INCOMPLETE) continue;                                                               // 如果返回结果为 VK_INCOMPLETE，则继续尝试获取交换链图像
      check(result);                                                                                       // 检查获取交换链图像列表的返回结果是否成功
      images_.resize(image_count);                                                                         // 根据实际获取到的图像数量调整图像列表的大小
      break;                                                                                               // 成功获取到交换链图像后跳出循环
    }
  } catch (...) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);  // 销毁交换链以释放资源
    swapchain_ = VK_NULL_HANDLE;                          // 将交换链句柄置为空，表示交换链已被销毁
    throw;                                                // 继续向上抛出异常以通知调用者交换链创建失败
  }
}

Swapchain::~Swapchain() {
  (void)vkDeviceWaitIdle(device_);                      // 等待设备空闲，确保在销毁交换链之前没有正在使用交换链的操作
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);  // 销毁交换链
}
}  // namespace engine::render