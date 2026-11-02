#include "shader_module.hpp"

#include <stdexcept>

namespace engine::render {
ShaderModule::ShaderModule(VkDevice device, std::span<const std::uint32_t> code) : device_(device) {
  if (code.size() < 5 || code.front() != 0x07230203u) throw std::runtime_error("Invalid embedded SPIR-V code");  // SPIR-V 最少包含 5 个字的文件头，并以魔数开头

  VkShaderModuleCreateInfo info{};                           // 初始化 VkShaderModuleCreateInfo 结构体，用于创建着色器模块
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;  // 设置结构体类型为 VkShaderModuleCreateInfo
  info.codeSize = code.size_bytes();                         // 设置着色器代码的字节大小
  info.pCode = code.data();                                  // 设置着色器代码的指针，内嵌数据无需额外拷贝

  const VkResult result = vkCreateShaderModule(device_, &info, nullptr, &module_);   // 创建着色器模块，如果失败则返回错误码
  if (result != VK_SUCCESS) throw std::runtime_error("Failed to create shader module");  // 如果创建着色器模块失败则抛出异常
}

ShaderModule::~ShaderModule() {
  vkDestroyShaderModule(device_, module_, nullptr);  // 销毁着色器模块
}
}  // namespace engine::render