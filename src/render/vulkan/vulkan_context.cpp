#include "vulkan_context.hpp"

#include <stdexcept>

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

namespace engine::render {
VulkanContext::VulkanContext(SDL_Window* window) {
  Uint32 count = 0;
  const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);  // 获取 Vulkan 所需的 SDL 扩展名列表
  if (!extensions) throw std::runtime_error("Failed to get Vulkan instance extensions from SDL.");

  VkApplicationInfo app{};                         // Vulkan 应用信息结构体，用于描述应用程序本身的信息
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;  // 指定结构体类型为 Vulkan 应用信息结构体
  app.pApplicationName = "Engine";                 // 设置应用程序名称
  app.apiVersion = VK_API_VERSION_1_0;             // 设置 Vulkan API 版本

  VkInstanceCreateInfo info{};                          // Vulkan 实例创建信息结构体，用于描述如何创建 Vulkan 实例
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;  // 指定结构体类型为 Vulkan 实例创建信息结构体
  info.pApplicationInfo = &app;                         // 指向 Vulkan 应用信息结构体
  info.enabledExtensionCount = count;                   // 指定启用的扩展数量
  info.ppEnabledExtensionNames = extensions;            // 指向启用的扩展名列表

  const VkResult result = vkCreateInstance(&info, nullptr, &instance_);  // 创建 Vulkan 实例
  if (result != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan instance.");
  if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_)) {  // 创建 Vulkan 表面失败的处理
    vkDestroyInstance(instance_, nullptr);
    throw std::runtime_error("Failed to create Vulkan surface.");
  }
}

VulkanContext::~VulkanContext() {
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkDestroyInstance(instance_, nullptr);
}
}  // namespace engine::render