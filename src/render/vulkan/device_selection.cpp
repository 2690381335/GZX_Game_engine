#include "device_selection.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace engine::render {
DeviceSelection selectDevice(VkInstance instance, VkSurfaceKHR surface) {
  const auto check = [](VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan query: " + std::to_string(result));
  };

  uint32_t count = 0;
  check(vkEnumeratePhysicalDevices(instance, &count, nullptr));  // 获取物理设备数量

  std::vector<VkPhysicalDevice> devices(count);
  if (count) check(vkEnumeratePhysicalDevices(instance, &count, devices.data()));  // 获取物理设备列表
  devices.resize(count);                                                           // 调整物理设备列表大小以匹配实际数量

  for (const auto device : devices) {
    uint32_t extension_count = 0;
    check(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));  // 获取设备扩展数量
    std::vector<VkExtensionProperties> extensions(extension_count);
    if (extension_count) check(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data()));  // 获取设备扩展列表
    extensions.resize(extension_count);                                                                                      // 调整设备扩展列表大小以匹配实际数量

    bool has_swapchain = false;  // 检查设备是否支持交换链扩展
    for (const auto& extension : extensions) {
      if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
        has_swapchain = true;  // 如果设备支持交换链扩展，设置标志为true
        break;                 // 找到交换链扩展后，跳出循环
      }  // 如果设备支持交换链扩展，设置标志为true
    }
    if (!has_swapchain) continue;  // 如果设备不支持交换链扩展，继续检查下一个设备

    uint32_t family_count = 0;                                                 // 获取队列族数量
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);  // 获取队列族数量

    std::vector<VkQueueFamilyProperties> families(family_count);                                         // 存储队列族属性列表
    if (family_count) vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());  // 获取队列族列表
    families.resize(family_count);                                                                       // 调整队列族列表大小以匹配实际数量

    for (uint32_t index = 0; index < family_count; ++index) {
      const auto& family = families[index];
      if (!family.queueCount || !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;  // 如果队列族不支持图形队列，继续检查下一个队列族

      VkBool32 present = VK_FALSE;                                                    // 检查队列族是否支持呈现能力
      check(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &present));  // 检查队列族是否支持呈现能力
      if (present) return {device, index};                                            // 如果队列族支持呈现能力，返回设备和队列族索引
    }
  }
  throw std::runtime_error("No suitable device found");  // 如果没有找到满足条件的设备，抛出异常
}
}  // namespace engine::render