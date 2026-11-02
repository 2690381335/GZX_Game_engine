#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "SDL3/SDL.h"
#include "frame/draw_frame.hpp"
#include "memory/upload_batch.hpp"

namespace engine::render {
Renderer::Renderer(const VulkanDevice& device, const GpuAllocator& allocator, VkSurfaceKHR surface, SDL_Window* window)
    : device_(device), allocator_(allocator), surface_(surface), window_(window), sampler_(std::make_unique<Sampler>(device_)), texture_layout_(std::make_unique<TextureSetLayout>(device_)), texture_descriptors_(std::make_unique<TextureDescriptors>(device_, *texture_layout_)) {}

bool Renderer::render(const engine::RenderFrame& frame) {
  int width = 0, height = 0;
  if (!SDL_GetWindowSizeInPixels(window_, &width, &height)) throw std::runtime_error("Failed to get window size in pixels");  // 获取窗口的像素尺寸失败
  if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) || width <= 0 || height <= 0) {                                    // 窗口最小化或尺寸无效
    dirty_ = true;
    return false;
  }

  if (frame.sprites.size() > std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("Too many sprites in the frame");  // 精灵数量超过 uint32_t 的最大值

  if (!std::isfinite(frame.camera.x) || !std::isfinite(frame.camera.y) || !std::isfinite(frame.camera.zoom) || frame.camera.zoom <= 0.0f) {
    throw std::runtime_error("Invalid camera parameters");
  }  // 检查相机参数的有效性

  instances_.clear();                        // 清空当前存储的精灵实例列表
  instances_.reserve(frame.sprites.size());  // 预留足够的空间以存储所有精灵实例
  batches_.clear();                          // 清空上一帧的合批列表

  bool reordered = false;  // 标记是否需要按排序结果间接访问精灵
  if (frame.sort != engine::SortMode::None) {
    order_.resize(frame.sprites.size());  // 为每个精灵准备排序条目
    for (std::size_t index = 0; index < frame.sprites.size(); ++index) {
      order_[index] = {engine::drawOrderKey(frame.sprites[index], frame.sort), static_cast<std::uint32_t>(index)};  // 计算绘制顺序键
      reordered |= index && order_[index].key < order_[index - 1].key;                                           // 只要出现一次逆序就需要排序
    }
    if (reordered) {
      std::sort(order_.begin(), order_.end(), [](const DrawOrder& left, const DrawOrder& right) {
        return left.key != right.key ? left.key < right.key : left.index < right.index;  // 键相同时按提交顺序，等价于稳定排序
      });
    }  // 已有序时跳过排序，常见场景下仅有一次线性扫描的开销
  }

  for (std::size_t position = 0; position < frame.sprites.size(); ++position) {
    const auto& sprite = frame.sprites[reordered ? order_[position].index : position];  // 按绘制顺序取出精灵
    const auto index = sprite.texture.index;                                           // 获取精灵纹理的索引
    if (index >= texture_bindings_.size() || texture_bindings_[index].set == VK_NULL_HANDLE) {
      throw std::runtime_error("Texture binding not found for sprite");
    }  // 如果精灵的纹理绑定不存在，抛出运行时错误

    if (!std::isfinite(sprite.rotation) || !std::isfinite(sprite.pivot_x) || !std::isfinite(sprite.pivot_y)) {
      throw std::runtime_error("Invalid sprite rotation or pivot parameters");  // 检查精灵的旋转角度和旋转中心的有效性
    }

    const bool rotated = sprite.rotation != 0.0f;  // 未旋转的精灵跳过三角函数计算
    instances_.push_back(SpriteInstance{
      sprite.rectangle,                                    // 精灵的矩形信息
      sprite.uv.left,                                      // 精灵的 UV 左坐标
      sprite.uv.top,                                       // 精灵的 UV 上坐标
      sprite.uv.right,                                     // 精灵的 UV 右坐标
      sprite.uv.bottom,                                    // 精灵的 UV 下坐标
      sprite.pivot_x, sprite.pivot_y,                      // 精灵的旋转中心
      rotated ? std::sin(sprite.rotation) : 0.0f,          // 精灵的旋转角度的正弦值
      rotated ? std::cos(sprite.rotation) : 1.0f           // 精灵的旋转角度的余弦值
    });                                                    // 将精灵实例添加到实例列表中

    if (batches_.empty() || batches_.back().texture != index || batches_.back().space != sprite.space) {
      batches_.push_back(DrawBatch{index, sprite.space, static_cast<std::uint32_t>(position), 1});  // 纹理或坐标空间变化时开启新批次
    } else {
      ++batches_.back().count;  // 与上一精灵状态相同时并入当前批次
    }
  }

  if (dirty_ || width != width_ || height != height_) {  // 窗口尺寸变化或渲染器状态脏
    frames_.reset();                                     // 重置帧缓存
    pipeline_.reset();                                   // 重置矩形渲染管线
    targets_.reset();                                    // 重置渲染目标
    pass_.reset();                                       // 重置渲染通道
    swapchain_.reset();                                  // 重置交换链

    swapchain_ = std::make_unique<Swapchain>(device_, surface_, VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)});  // 创建新的交换链
    pass_ = std::make_unique<RenderPass>(device_.handle(), swapchain_->format());                                                          // 创建新的渲染通道实例
    targets_ = std::make_unique<RenderTargets>(device_.handle(), *swapchain_, *pass_);                                                     // 创建新的渲染目标实例
    pipeline_ = std::make_unique<SpritePipeline>(device_.handle(), pass_->handle(), texture_layout_->handle());                            // 创建新的精灵渲染管线实例
    frames_ = std::make_unique<FrameResources>(device_, allocator_, static_cast<uint32_t>(swapchain_->images().size()));                   // 创建新的帧资源实例

    width_ = width;    // 更新渲染器的宽度
    height_ = height;  // 更新渲染器的高度
    dirty_ = false;    // 清除渲染器的脏状态
  }
  const auto result = drawFrame(device_, allocator_, *swapchain_, *frames_, *pass_, *targets_, *pipeline_, frame, instances_, batches_, texture_bindings_);  // 绘制一帧并获取绘制结果
  if (result.recreate) dirty_ = true;                                                                                                              // 如果需要重新创建交换链，标记渲染器为脏
  return result.presented;                                                                                                                         // 返回帧是否成功呈现
}

void Renderer::uploadTextures(std::span<const engine::Assets::ImageData> images) {
  if (images.empty()) throw std::invalid_argument("No images provided for upload");                                               // 如果没有提供要上传的纹理数据，抛出无效参数异常
  if (images.size() < textures_.size()) throw std::logic_error("Number of images is less than the number of existing textures");  // 如果提供的图像数量少于已有的纹理数量，抛出逻辑错误
  if (images.size() == textures_.size()) return;                                                                                  // 如果提供的图像数量与已有的纹理数量相同，则无需上传新的纹理

  const auto reserve_for_append = [](auto& storage, std::size_t required) {
    if (required <= storage.capacity()) return;  // 如果当前容量足够，则无需扩展

    const auto maximum = storage.max_size();                                                            // 获取存储的最大容量
    if (required > maximum) throw std::length_error("Required size exceeds maximum storage capacity");  // 如果所需容量超过最大存储容量，抛出长度错误异常

    const auto current = storage.capacity();                           // 获取当前存储的容量
    const auto grown = current > maximum / 2 ? maximum : current * 2;  // 计算扩展后的容量，如果当前容量大于最大容量的一半，则扩展到最大容量，否则扩展为当前容量的两倍

    storage.reserve(std::max(required, grown));  // 预留存储空间，确保至少有所需的容量
  };  // 用于确保容器有足够容量的辅助函数

  reserve_for_append(textures_, images.size());          // 确保纹理对象列表有足够的容量以存储所有即将创建的纹理对象
  reserve_for_append(texture_bindings_, images.size());  // 确保纹理绑定列表有足够的容量以存储所有即将创建的纹理绑定

  images = images.subspan(textures_.size());  // 获取尚未上传的图像数据子视图

  VkDeviceSize capacity = VkDeviceSize{16} * 1024 * 1024;  // 计算纹理上传所需的容量，假设每张纹理最大为 16MB
  for (const auto& image : images) {
    capacity = std::max(capacity, VkDeviceSize(image.rgba.size()));  // 更新容量为每张纹理的实际大小与初始容量的最大值
  }

  std::vector<std::unique_ptr<Texture2D>> textures;  // 用于存储创建的纹理对象列表
  textures.reserve(images.size());                   // 为即将创建的纹理对象预留空间

  if (!upload_batch_ || upload_batch_->capacity() < capacity) {
    upload_batch_ = std::make_unique<UploadBatch>(device_, allocator_, capacity);  // 创建新的上传批次
  }  // 如果上传批次不存在或容量不足，创建新的上传批次

  auto& batch = *upload_batch_;
  try {
    if (!batch.tryBegin()) throw std::logic_error("Failed to begin texture upload batch");  // 如果无法开始上传批次，抛出逻辑错误

    for (const auto& image : images) {
      textures.push_back(std::make_unique<Texture2D>(device_, allocator_, image.width, image.height));  // 创建新的纹理对象并添加到列表中
      if (!batch.enqueue(*textures.back(), image.rgba)) {
        batch.submit();                                                                                                                          // 提交当前上传批次
        batch.wait();                                                                                                                            // 等待当前上传批次完成
        if (!batch.tryBegin() || !batch.enqueue(*textures.back(), image.rgba)) throw std::logic_error("Failed to enqueue texture after retry");  // 如果重试仍然失败，抛出逻辑错误
      }  // 如果上传失败，重试一次
    }
    batch.submit();  // 提交最后的上传批次
    batch.wait();    // 等待最后的上传批次完成

    std::vector<TextureDescriptors::Binding> bindings;  // 用于存储分配的纹理描述符绑定列表
    bindings.reserve(images.size());                    // 预先分配足够的空间以存储所有纹理描述符绑定

    try {
      for (const auto& texture : textures) {
        bindings.push_back(texture_descriptors_->allocate(texture->view(), sampler_->handle()));  // 分配纹理描述符绑定并添加到列表中
      }
    } catch (...) {
      for (auto& binding : bindings) {
        texture_descriptors_->free(binding);  // 释放绑定的纹理描述符
      }
      throw;  // 重新抛出异常以通知调用者纹理描述符分配失败
    }

    for (auto& texture : textures) {
      textures_.push_back(std::move(texture));
    }  // 每一个纹理对象都已移动到 textures_ 中
    for (auto& binding : bindings) {
      texture_bindings_.push_back(std::move(binding));
    }  // 每一个纹理描述符绑定都已移动到 texture_bindings_ 中
  } catch (...) {
    upload_batch_.reset();  // 清理上传批次资源
    throw;
  }
}
}  // namespace engine::render