#include "staging_buffer.hpp"

#include <stdexcept>

namespace engine::render {
StagingBuffer::StagingBuffer(const GpuAllocator& allocator, VkDeviceSize capacity) : allocator_(allocator.handle()), capacity_(capacity) {
  if (!capacity) throw std::invalid_argument("StagingBuffer capacity must be greater than 0.");  // 检查容量是否为0，如果是则抛出异常

  VkBufferCreateInfo buffer{};                          // 创建 Vulkan 缓冲区创建信息结构体并初始化为零
  buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;  // 指定结构体类型为 Vulkan 缓冲区创建信息结构体类型
  buffer.size = capacity;                               // 设置缓冲区的大小为指定的容量
  buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;      // 设置缓冲区的使用标志为传输源
  buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;       // 设置缓冲区的共享模式为独占模式

  VmaAllocationCreateInfo allocation{};                                                                          // 创建 Vulkan 内存分配信息结构体并初始化为零
  allocation.usage = VMA_MEMORY_USAGE_AUTO;                                                                      // 设置内存分配的使用类型为自动
  allocation.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;  // 设置内存分配的标志，表示主机顺序写访问和映射内存

  VmaAllocationInfo info{};                                                                                                                                          // 创建 Vulkan 内存分配信息结构体并初始化为零
  if (vmaCreateBuffer(allocator_, &buffer, &allocation, &buffer_, &allocation_, &info) != VK_SUCCESS) throw std::runtime_error("Failed to create staging buffer.");  // 创建 Vulkan 缓冲区并分配内存，如果失败则抛出异常
  mapped_ = info.pMappedData;                                                                                                                                        // 获取映射的内存指针
}

StagingBuffer::~StagingBuffer() {
  if (buffer_) vmaDestroyBuffer(allocator_, buffer_, allocation_);  // 销毁 Vulkan 缓冲区并释放内存
}

void StagingBuffer::flush() {
  if (vmaFlushAllocation(allocator_, allocation_, 0, capacity_) != VK_SUCCESS) throw std::runtime_error("Failed to flush staging buffer.");  // 刷新 Vulkan 缓冲区的内存，如果失败则抛出异常
}
}  // namespace engine::render