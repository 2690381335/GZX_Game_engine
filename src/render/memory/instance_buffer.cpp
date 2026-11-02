#include "instance_buffer.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

namespace engine::render {
InstanceBuffer::InstanceBuffer(const GpuAllocator& allocator, VkDeviceSize capacity) : allocator_(allocator.handle()), capacity_(capacity) {
  if (!capacity || capacity > std::numeric_limits<std::size_t>::max()) throw std::runtime_error("Invalid instance buffer capacity");  // 检查实例缓冲区的容量是否有效

  VkBufferCreateInfo buffer{};                          // 创建 Vulkan 缓冲区的描述信息结构体
  buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;  // 设置结构体类型为 Vulkan 缓冲区创建信息
  buffer.size = capacity;                               // 设置缓冲区的大小为实例缓冲区的容量
  buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;     // 设置缓冲区的用途为顶点缓冲区
  buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;       // 设置缓冲区的共享模式为独占模式

  VmaAllocationCreateInfo allocation{};                                                                          // 创建 Vulkan 内存分配信息结构体
  allocation.usage = VMA_MEMORY_USAGE_AUTO;                                                                      // 设置内存的使用类型为自动选择
  allocation.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;  // 设置内存分配的标志，允许主机顺序写入并映射内存
  VmaAllocationInfo info{};                                                                                      // 用于获取内存分配的详细信息
  if (vmaCreateBuffer(allocator_, &buffer, &allocation, &buffer_, &allocation_, &info) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create instance buffer");
  }  // 如果创建 Vulkan 实例缓冲区失败，则抛出异常
  mapped_ = info.pMappedData;  // 获取映射到主机可访问内存的指针
}

InstanceBuffer::~InstanceBuffer() {
  if (buffer_) vmaDestroyBuffer(allocator_, buffer_, allocation_);  // 销毁 Vulkan 实例缓冲区及其内存分配
}

void InstanceBuffer::upload(std::span<const std::byte> bytes) {
  if (bytes.size() > capacity_) throw std::runtime_error("InstanceBuffer capacity exceeded");  // 检查上传的字节数据是否超过缓冲区容量
  if (bytes.empty()) return;                                                                   // 如果上传的字节数据为空，则直接返回
  std::memcpy(mapped_, bytes.data(), bytes.size());                                            // 将字节数据复制到映射的内存中

  if (vmaFlushAllocation(allocator_, allocation_, 0, bytes.size()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to flush instance buffer allocation");  // 如果刷新内存分配失败，则抛出异常
  }
}
}  // namespace engine::render