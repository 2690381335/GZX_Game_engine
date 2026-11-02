#include "sampler.hpp"

#include <stdexcept>

namespace engine::render {

Sampler::Sampler(const VulkanDevice& device) : device_(device.handle()) {
  VkSamplerCreateInfo info{};                                 // 初始化采样器创建信息结构体
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;         // 指定结构体类型为采样器创建信息结构体
  info.magFilter = VK_FILTER_NEAREST;                         // 设置放大过滤器为最近邻
  info.minFilter = VK_FILTER_LINEAR;                          // 设置缩小过滤器为线性
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;           // 设置多级渐远纹理采样模式为最近邻
  info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // 设置U方向的纹理寻址模式为边缘夹紧
  info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // 设置V方向的纹理寻址模式为边缘夹紧
  info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // 设置W方向的纹理寻址模式为边缘夹紧
  info.minLod = 0.0f;                                         // 设置最小LOD等级
  info.maxLod = VK_LOD_CLAMP_NONE;                            // 设置最大LOD等级，VK_LOD_CLAMP_NONE表示不限制最大LOD等级
  info.maxAnisotropy = 1.0f;                                  // 设置最大各向异性过滤值

  if (vkCreateSampler(device_, &info, nullptr, &sampler_) != VK_SUCCESS) throw std::runtime_error("Failed to create sampler");  // 创建采样器失败时抛出异常
}

Sampler::~Sampler() {
  if (sampler_) vkDestroySampler(device_, sampler_, nullptr);  // 销毁采样器
}
}  // namespace engine::render