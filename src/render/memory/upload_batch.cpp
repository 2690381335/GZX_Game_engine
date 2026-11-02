#include "upload_batch.hpp"

#include <algorithm>
#include <cstring>
#include <limits>  // For std::numeric_limits<std::size_t>::max()
#include <stdexcept>

namespace {
void checkUpload(VkResult result) {
  if (result != VK_SUCCESS) throw std::runtime_error("Vulkan upload error");
}
}  // namespace

namespace engine::render {
UploadBatch::UploadBatch(const VulkanDevice& device, const GpuAllocator& allocator, VkDeviceSize capacity)
    : device_(device.handle()), queue_(device.queue()), staging_(allocator, capacity) {
  try {
    VkCommandPoolCreateInfo pool{};                                     // 用于临时命令缓冲区的命令池
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;            // 指定结构体类型
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;                  // 指定命令池为临时使用
    pool.queueFamilyIndex = device.queueFamily();                       // 指定命令池所属的队列族索引
    checkUpload(vkCreateCommandPool(device_, &pool, nullptr, &pool_));  // 创建命令池并检查结果

    VkCommandBufferAllocateInfo commands{};                                 // 用于分配命令缓冲区的结构体
    commands.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;        // 指定结构体类型
    commands.commandPool = pool_;                                           // 指定命令缓冲区所属的命令池
    commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;                       // 指定命令缓冲区为主命令缓冲区
    commands.commandBufferCount = 1;                                        // 指定分配的命令缓冲区数量
    checkUpload(vkAllocateCommandBuffers(device_, &commands, &commands_));  // 分配命令缓冲区并检查结果

    VkFenceCreateInfo fence{};                                      // 用于创建栅栏的结构体
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;              // 指定结构体类型
    checkUpload(vkCreateFence(device_, &fence, nullptr, &fence_));  // 创建栅栏并检查结果
  } catch (...) {
    release();
    throw;
  }
}

UploadBatch::~UploadBatch() {
  if (state_ == State::Inflight) (void)vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);  // 等待命令缓冲区执行完成
  release();                                                                                       // 释放资源
}

bool UploadBatch::tryBegin() {
  if (state_ == State::Recording) throw std::logic_error("Upload batch is already recording");  // 当前上传批次仍在记录时，无法开始新的上传批次

  if (state_ == State::Inflight) {                              // 如果上传批次处于执行中状态，检查命令缓冲区是否完成执行
    const VkResult result = vkGetFenceStatus(device_, fence_);  // 检查栅栏状态
    if (result == VK_NOT_READY) return false;                   // 栅栏仍然处于信号状态，表示命令缓冲区尚未完成执行
    checkUpload(result);                                        // 检查栅栏状态是否为其他错误
    state_ = State::Ready;                                      // 命令缓冲区已完成执行，上传批次准备就绪
  }

  checkUpload(vkResetCommandPool(device_, pool_, 0));  // 重置命令池，为新的命令缓冲区记录做准备

  VkCommandBufferBeginInfo begin{};                           // 用于开始命令缓冲区记录的结构体
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;  // 指定结构体类型
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 指定命令缓冲区使用一次性提交标志
  checkUpload(vkBeginCommandBuffer(commands_, &begin));       // 开始命令缓冲区记录并检查结果

  used_ = 0;                  // 重置已使用的上传空间计数器
  images_.clear();            // 清空已记录的图像列表
  state_ = State::Recording;  // 设置上传批次状态为记录中
  return true;                // 返回true表示成功开始新的上传批次
}

bool UploadBatch::enqueue(Texture2D& texture, std::span<const std::byte> rgba) {
  if (state_ != State::Recording) throw std::logic_error("Upload batch is not recording");  // 只有处于记录中的上传批次才能入队

  const std::uint64_t pixels = static_cast<std::uint64_t>(texture.width()) * static_cast<std::uint64_t>(texture.height());                                  // 计算纹理的像素数量
  const auto maximum = std::numeric_limits<std::size_t>::max();                                                                                             // 获取 std::size_t 类型的最大值
  if (pixels > maximum / 4 || rgba.size() != pixels * 4) throw std::runtime_error("Texture size exceeds maximum allowed size or RGBA data size mismatch");  // 检查纹理大小是否超过允许的最大值或 RGBA 数据大小是否匹配
  if (std::find(images_.begin(), images_.end(), texture.image()) != images_.end()) throw std::runtime_error("Texture has already been enqueued");           // 检查纹理是否已经入队
  if (rgba.size() > staging_.capacity() - used_) return false;                                                                                              // 检查暂存缓冲区是否有足够的空间
  if (used_ > maximum - rgba.size()) throw std::runtime_error("Staging buffer overflow");                                                                   // 检查暂存缓冲区是否会溢出

  images_.push_back(texture.image());                                                                                 // 将纹理的 Vulkan 图像句柄加入已记录的图像列表
  std::memcpy(static_cast<std::byte*>(staging_.data()) + static_cast<std::size_t>(used_), rgba.data(), rgba.size());  // 将 RGBA 数据拷贝到暂存缓冲区

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;                                                                                      // 指定结构体类型
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;                                                                                               // 旧的图像布局
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;                                                                                    // 新的图像布局
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;                                                                                        // 目标访问掩码，表示将进行传输写操作
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;                                                                                       // 源队列族索引
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;                                                                                       // 目标队列族索引
  barrier.image = texture.image();                                                                                                             // 指定要操作的图像
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};                                                                          // 指定子资源范围，包括颜色方面、mipmap层级和数组层级
  vkCmdPipelineBarrier(commands_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);  // 插入管线屏障以确保图像布局转换在传输操作之前完成

  VkBufferImageCopy copy{};                                                                                               // 定义缓冲区到图像的拷贝结构体
  copy.bufferOffset = used_;                                                                                              // 指定缓冲区的偏移量
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};                                                           // 指定图像子资源，包括颜色方面、mipmap层级和数组层级
  copy.imageExtent = {texture.width(), texture.height(), 1};                                                              // 指定图像的宽度、高度和深度
  vkCmdCopyBufferToImage(commands_, staging_.handle(), texture.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);  // 执行缓冲区到图像的拷贝操作

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;                                                                                        // 旧的图像布局
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;                                                                                    // 新的图像布局
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;                                                                                            // 源访问掩码，表示传输写操作已经完成
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;                                                                                               // 目标访问掩码，表示将进行着色器读取操作
  vkCmdPipelineBarrier(commands_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);  // 插入管线屏障以确保图像布局转换在着色器读取之前完成

  used_ += static_cast<VkDeviceSize>(rgba.size());  // 更新已使用的缓冲区大小
  return true;
}

void UploadBatch::submit() {
  if (state_ != State::Recording) throw std::logic_error("Upload batch is not recording");  // 只有处于记录中的上传批次才能提交

  if (images_.empty()) {
    checkUpload(vkEndCommandBuffer(commands_));  // 结束命令缓冲区记录并检查结果
    state_ = State::Ready;                       // 设置上传批次状态为准备就绪
    return;                                      // 提交完成后直接返回
  }

  staging_.flush();                            // 让CPU写入时对GPU可见
  checkUpload(vkEndCommandBuffer(commands_));  // 结束命令缓冲区记录并检查结果

  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;  // 指定结构体类型
  submit.commandBufferCount = 1;                 // 提交的命令缓冲区数量
  submit.pCommandBuffers = &commands_;           // 指向要提交的命令缓冲区

  checkUpload(vkResetFences(device_, 1, &fence_));         // 重置栅栏，为新的命令缓冲区提交做准备
  checkUpload(vkQueueSubmit(queue_, 1, &submit, fence_));  // 提交命令缓冲区并检查结果
  state_ = State::Inflight;                                // 设置上传批次状态为执行中
}

void UploadBatch::wait() {
  if (state_ == State::Recording) throw std::logic_error("Upload batch is still recording");  // 不能等待处于记录状态的上传批次

  if (state_ == State::Inflight) {
    checkUpload(vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX));  // 等待栅栏，确保命令缓冲区执行完成
    state_ = State::Ready;                                                   // 设置上传批次状态为准备就绪
  }
}

void UploadBatch::release() noexcept {
  if (pool_) vkDestroyCommandPool(device_, pool_, nullptr);  // 销毁命令池
  if (fence_) vkDestroyFence(device_, fence_, nullptr);      // 销毁栅栏
  pool_ = VK_NULL_HANDLE;                                    // 重置命令池句柄
  commands_ = VK_NULL_HANDLE;                                // 重置命令缓冲区句柄
  fence_ = VK_NULL_HANDLE;                                   // 重置栅栏句柄
}
}  // namespace engine::render