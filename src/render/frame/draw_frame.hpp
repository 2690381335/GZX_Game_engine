#pragma once

#include <cstdint>
#include <span>

#include "../../../include/engine/render_frame.hpp"
#include "../pipeline/sprite_instance.hpp"
#include "../pipeline/sprite_pipeline.hpp"
#include "../texture/texture_descriptors.hpp"
#include "../vulkan/swapchain.hpp"
#include "frame_resources.hpp"
#include "render_targets.hpp"

namespace engine::render {
/**
 * @brief 表示一次合批绘制，同一批次内的实例共享纹理和坐标空间。
 *
 */
struct DrawBatch {
  std::uint32_t texture = 0;                       // 批次使用的纹理索引
  engine::CoordinateSpace space = engine::CoordinateSpace::World;  // 批次使用的坐标空间
  std::uint32_t first = 0;                         // 批次在实例缓冲区中的起始实例索引
  std::uint32_t count = 0;                         // 批次包含的实例数量
};
/**
 * @brief 绘制结果，包含帧是否已呈现以及是否需要重新创建交换链。
 *
 * @struct DrawResult
 * @brief 表示一次绘制操作的结果。
 */
struct DrawResult {
  bool presented = false;  // 帧是否已呈现
  bool recreate = false;   // 是否需要重新创建交换链
};

/**
 * @brief 绘制一帧并将其呈现到交换链。
 *
 * @param device Vulkan 设备。
 * @param allocator 用于分配 Vulkan 资源的 GPU 分配器。
 * @param swapchain 交换链。
 * @param frames 帧资源。
 * @param pass 渲染通道。
 * @param targets 渲染目标。
 * @param pipeline 精灵渲染管线。
 * @param frame 要渲染的帧，提供背景色和相机。
 * @param instances 精灵实例数据，已按绘制顺序排列。
 * @param batches 合批列表，按绘制顺序覆盖全部实例。
 * @param bindings 纹理描述符绑定。
 * @return DrawResult 绘制结果，包含帧是否已呈现以及是否需要重新创建交换链。
 */
DrawResult drawFrame(const VulkanDevice& device,
                     const GpuAllocator& allocator,
                     const Swapchain& swapchain,
                     FrameResources& frames,
                     const RenderPass& pass,
                     const RenderTargets& targets,
                     const SpritePipeline& pipeline,
                     const engine::RenderFrame& frame,
                     std::span<const SpriteInstance> instances,
                     std::span<const DrawBatch> batches,
                     std::span<const TextureDescriptors::Binding> bindings);
}  // namespace engine::render