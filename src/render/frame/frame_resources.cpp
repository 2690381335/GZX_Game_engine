#include "frame_resources.hpp"

#include <stdexcept>

#include "../pipeline/sprite_instance.hpp"

namespace engine::render {
FrameResources::FrameResources(const VulkanDevice& device, const GpuAllocator& allocator, uint32_t image_count) : finished_(image_count, VK_NULL_HANDLE), device_(device.handle()) {
  if (!image_count) throw std::runtime_error("Image count must be greater than zero");  // 检查图像数量是否大于零

  const auto check = [](VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan operation failed");
  };

  uint32_t family_count = 0;                                                                  // 队列族数量
  vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice(), &family_count, nullptr);  // 获取物理设备的队列族属性数量

  std::vector<VkQueueFamilyProperties> families(family_count);                                        // 队列族属性数组
  vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice(), &family_count, families.data());  // 获取物理设备的队列族属性数组

  timestamp_bits_ = families.at(device.queueFamily()).timestampValidBits;  // 获取时间戳查询的有效位数

  VkPhysicalDeviceProperties properties{};                              // 物理设备属性结构体
  vkGetPhysicalDeviceProperties(device.physicalDevice(), &properties);  // 获取物理设备属性
  timestamp_ns_ = properties.limits.timestampPeriod;                    // 获取时间戳查询的纳秒数

  try {
    VkSemaphoreCreateInfo semaphore{};                          // 信号量创建信息结构
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;  // 设置结构类型为信号量创建信息结构
    for (auto& slot : slots_) {
      if (timestamp_bits_ != 0) {
        VkQueryPoolCreateInfo query{};                                         // 查询池创建信息结构
        query.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;                // 设置结构类型为查询池创建信息结构
        query.queryType = VK_QUERY_TYPE_TIMESTAMP;                             // 设置查询类型为时间戳查询
        query.queryCount = 2;                                                  // 设置查询数量为 2
        check(vkCreateQueryPool(device_, &query, nullptr, &slot.timestamps));  // 创建查询池并检查结果
      }

      const VkDeviceSize initial_bytes = VkDeviceSize{65536} * sizeof(SpriteInstance);  // 计算实例缓冲区的初始字节数
      slot.instances = std::make_unique<InstanceBuffer>(allocator, initial_bytes);      // 为每个帧资源槽创建实例缓冲区

      VkCommandPoolCreateInfo pool{};                                   // 命令池创建信息结构
      pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;          // 设置结构类型为命令池创建信息结构
      pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;     // 允许重置命令缓冲区
      pool.queueFamilyIndex = device.queueFamily();                     // 设置命令池所属的队列族索引
      check(vkCreateCommandPool(device_, &pool, nullptr, &slot.pool));  // 创建命令池并检查结果

      VkCommandBufferAllocateInfo allocation{};                                // 命令缓冲区分配信息结构
      allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;       // 设置结构类型为命令缓冲区分配信息结构
      allocation.commandPool = slot.pool;                                      // 设置命令缓冲区所属的命令池
      allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;                      // 设置命令缓冲区级别为主命令缓冲区
      allocation.commandBufferCount = 1;                                       // 设置命令缓冲区的数量
      check(vkAllocateCommandBuffers(device_, &allocation, &slot.commands));   // 分配命令缓冲区并检查结果
      check(vkCreateSemaphore(device_, &semaphore, nullptr, &slot.acquired));  // 创建信号量并检查结果

      VkFenceCreateInfo fence{};                                    // 栅栏创建信息结构
      fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;            // 设置结构类型为栅栏创建信息结构
      fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;                   // 设置栅栏初始状态为已信号
      check(vkCreateFence(device_, &fence, nullptr, &slot.fence));  // 创建栅栏并检查结果
    }
    for (auto& finished : finished_) {
      check(vkCreateSemaphore(device_, &semaphore, nullptr, &finished));  // 创建渲染完成信号量并检查结果
    }
  } catch (...) {
    release();
    throw;
  }
}

FrameResources::~FrameResources() {
  (void)vkDeviceWaitIdle(device_);  // 等待设备空闲，确保所有命令完成执行后再释放资源
  release();                        // 释放 Vulkan 帧资源
}

void FrameResources::release() noexcept {
  for (const auto finished : finished_) {
    if (finished) vkDestroySemaphore(device_, finished, nullptr);  // 销毁渲染完成信号量
  }
  for (auto& slot : slots_) {
    if (slot.fence) vkDestroyFence(device_, slot.fence, nullptr);                // 销毁栅栏
    if (slot.acquired) vkDestroySemaphore(device_, slot.acquired, nullptr);      // 销毁图像获取信号量
    if (slot.pool) vkDestroyCommandPool(device_, slot.pool, nullptr);            // 销毁命令池
    slot.instances.reset();                                                      // 释放实例缓冲区
    if (slot.timestamps) vkDestroyQueryPool(device_, slot.timestamps, nullptr);  // 销毁时间戳查询池
  }
}
}  // namespace engine::render