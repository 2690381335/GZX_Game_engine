#pragma once

#include <cstdint>
#include <span>

#include "vulkan/vulkan.h"

namespace engine::render {
/**
 * @brief 封装了一个 Vulkan 着色器模块，管理其创建和销毁。
 *
 */
class ShaderModule {
 public:
  /**
   * @brief 生成一个新的 Vulkan 着色器模块对象。
   *
   * @param device Vulkan 设备句柄。
   * @param code 编译期内嵌的 SPIR-V 代码。
   */
  ShaderModule(VkDevice device, std::span<const std::uint32_t> code);

  /**
   * @brief 销毁 Vulkan 着色器模块对象。
   *
   */
  ~ShaderModule();

  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;

  /**
   * @brief 获取 Vulkan 着色器模块的句柄。
   *
   * @return VkShaderModule Vulkan 着色器模块的句柄。
   */
  VkShaderModule handle() const noexcept { return module_; }

 private:
  VkDevice device_;                         // Vulkan 设备句柄
  VkShaderModule module_ = VK_NULL_HANDLE;  // Vulkan 着色器模块句柄
};
}  // namespace engine::render