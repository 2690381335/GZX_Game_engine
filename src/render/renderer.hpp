#pragma once

#include <memory>
#include <span>
#include <vector>

#include "../../include/engine/assets.hpp"
#include "../../include/engine/render_frame.hpp"
#include "frame/draw_frame.hpp"
#include "frame/frame_resources.hpp"
#include "frame/render_targets.hpp"
#include "memory/upload_batch.hpp"
#include "pipeline/sprite_instance.hpp"
#include "pipeline/sprite_pipeline.hpp"
#include "texture/sampler.hpp"
#include "texture/texture_2d.hpp"
#include "texture/texture_descriptors.hpp"
#include "texture/texture_set_layout.hpp"
#include "vulkan/swapchain.hpp"

struct SDL_Window;

namespace engine::render {
/**
 * @brief 渲染器类，负责管理 Vulkan 渲染管线、交换链、纹理等资源，并执行渲染操作。
 *
 */
class Renderer {
 public:
  /**
   * @brief 构造一个新的 Renderer 对象，并初始化 Vulkan 渲染相关资源。
   *
   * @param device Vulkan 设备对象。
   * @param allocator GPU 分配器对象。
   * @param surface Vulkan 表面，用于与窗口系统交互。
   * @param window 指向 SDL 窗口的指针。
   */
  Renderer(const VulkanDevice& device, const GpuAllocator& allocator, VkSurfaceKHR surface, SDL_Window* window);

  /**
   * @brief 执行渲染操作。
   *
   * @param frame 渲染帧，包含背景颜色和矩形信息。
   * @return true 渲染成功。
   * @return false 渲染失败。
   */
  bool render(const engine::RenderFrame& frame);

  /**
   * @brief 将纹理数据上传到 GPU。
   *
   * @param images 需要上传的纹理数据列表。
   */
  void uploadTextures(std::span<const engine::Assets::ImageData> images);

 private:
  /**
   * @brief 精灵的排序条目，键相同时按提交索引排序以保证稳定性。
   *
   */
  struct DrawOrder {
    std::uint64_t key = 0;     // 绘制顺序键
    std::uint32_t index = 0;   // 精灵在渲染帧中的提交索引
  };

  const VulkanDevice& device_;                                 // 引用渲染器使用的 Vulkan 设备
  const GpuAllocator& allocator_;                              // 引用渲染器使用的 GPU 分配器
  VkSurfaceKHR surface_;                                       // Vulkan 表面，用于与窗口系统交互
  SDL_Window* window_;                                         // 指向 SDL 窗口的指针
  int width_ = 0, height_ = 0;                                 // 窗口的宽度和高度
  bool dirty_ = true;                                          // 标记渲染器状态是否需要更新
  std::unique_ptr<Sampler> sampler_;                           // 采样器
  std::unique_ptr<TextureSetLayout> texture_layout_;           // 纹理集布局
  std::unique_ptr<TextureDescriptors> texture_descriptors_;    // 纹理描述符管理器
  std::vector<std::unique_ptr<Texture2D>> textures_;           // 存储上传的纹理对象列表
  std::vector<TextureDescriptors::Binding> texture_bindings_;  // 存储纹理描述符绑定信息列表
  std::unique_ptr<UploadBatch> upload_batch_;                  // 上传批次管理器
  std::vector<SpriteInstance> instances_;                      // 存储精灵实例列表
  std::vector<DrawOrder> order_;                               // 存储精灵排序条目，跨帧复用容量
  std::vector<DrawBatch> batches_;                             // 存储合批列表，跨帧复用容量
  std::unique_ptr<Swapchain> swapchain_;                       // Vulkan 交换链
  std::unique_ptr<RenderPass> pass_;                           // 渲染通道
  std::unique_ptr<RenderTargets> targets_;                     // 渲染目标
  std::unique_ptr<SpritePipeline> pipeline_;                   // 精灵渲染管线
  std::unique_ptr<FrameResources> frames_;                     // 每帧的资源管理器
};
}  // namespace engine::render