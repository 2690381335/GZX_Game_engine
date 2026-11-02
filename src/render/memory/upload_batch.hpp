#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "../texture/texture_2d.hpp"
#include "staging_buffer.hpp"

namespace engine::render {
/**
 * @brief 上传批次类，用于管理 Vulkan 上传操作的批处理。
 *
 */
class UploadBatch {
 public:
  /**
   * @brief 初始化上传批次对象。
   *
   * @param device 与上传批次相关的 Vulkan 逻辑设备。
   * @param allocator 与上传批次相关的 GPU 分配器。
   * @param capacity 上传批次的容量，即临时缓冲区的总大小。
   */
  UploadBatch(const VulkanDevice& device, const GpuAllocator& allocator, VkDeviceSize capacity);

  ~UploadBatch();

  UploadBatch(const UploadBatch&) = delete;
  UploadBatch& operator=(const UploadBatch&) = delete;

  /**
   * @brief 尝试开始记录上传命令，如果上传批次处于准备好状态，则可以开始记录上传命令。
   *
   * @return true 上传批次已准备好，可以开始记录上传命令
   * @return false 上传批次未准备好，无法开始记录上传命令
   */
  bool tryBegin();

  /**
   * @brief 将纹理的上传任务加入上传批次队列，如果上传批次处于记录状态，则可以加入任务。
   *
   * @param texture 需要上传的纹理对象。
   * @param rgba 纹理数据的字节数组。
   * @return true 上传任务已成功加入队列。
   * @return false 上传任务加入失败，可能是上传批次未处于记录状态。
   */
  bool enqueue(Texture2D& texture, std::span<const std::byte> rgba);

  /**
   * @brief 提交上传批次的命令，使其开始执行上传操作。
   *
   */
  void submit();

  /**
   * @brief 等待上传批次的命令执行完成。
   *
   */
  void wait();

  /**
   * @brief 获取上传批次的容量，即临时缓冲区的总大小。
   *
   * @return VkDeviceSize 上传批次的容量，即临时缓冲区的总大小。
   */
  VkDeviceSize capacity() const noexcept {
    return staging_.capacity();
  }

 private:
  /**
   * @brief 上传批次的状态枚举
   *
   */
  enum class State {
    Ready,      // 上传批次已准备好，可以开始记录上传命令
    Recording,  // 上传批次正在记录上传命令
    Inflight    // 上传批次的命令已提交，正在执行中
  };

  /**
   * @brief 释放上传批次占用的 Vulkan 资源
   *
   */
  void release() noexcept;

 private:
  VkDevice device_;                            // 与此上传批次相关的 Vulkan 逻辑设备
  VkQueue queue_;                              // 与此上传批次相关的 Vulkan 队列
  StagingBuffer staging_;                      // 用于临时存储上传数据的缓冲区
  VkCommandPool pool_ = VK_NULL_HANDLE;        // 用于分配命令缓冲区的命令池
  VkCommandBuffer commands_ = VK_NULL_HANDLE;  // 用于记录上传命令的命令缓冲区
  VkFence fence_ = VK_NULL_HANDLE;             // 用于同步上传操作的栅栏
  VkDeviceSize used_ = 0;                      // 已使用的临时缓冲区大小
  State state_ = State::Ready;                 // 上传批次的当前状态
  std::vector<VkImage> images_;                // 需要上传的 Vulkan 图像列表
};
}  // namespace engine::render