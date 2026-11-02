#include "sprite_pipeline.hpp"

#include <cstdint>
#include <stdexcept>

#include "shader_module.hpp"
#include "sprite_instance.hpp"

namespace engine::render {
namespace {
constexpr std::uint32_t kSpriteVertexCode[]{
#include "../../../shaders/sprite.vert.inc"
};  // 手动执行 glslc -mfmt=num 生成的顶点着色器 SPIR-V，内嵌进库中无需运行时加载文件

constexpr std::uint32_t kSpriteFragmentCode[]{
#include "../../../shaders/sprite.frag.inc"
};  // 手动执行 glslc -mfmt=num 生成的片段着色器 SPIR-V
}  // namespace

SpritePipeline::SpritePipeline(VkDevice device, VkRenderPass pass, VkDescriptorSetLayout texture_layout)
    : device_(device) {
  try {
    ShaderModule vertex(device_, kSpriteVertexCode);      // 创建顶点着色器模块
    ShaderModule fragment(device_, kSpriteFragmentCode);  // 创建片段着色器模块

    const VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, 5 * sizeof(float)};  // 定义推送常量范围，指定在顶点着色器阶段使用，偏移量为0，大小为5个float的字节数

    VkPipelineLayoutCreateInfo layout_info{};                           // 创建管线布局信息结构体
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;  // 指定结构体类型为管线布局创建信息
    layout_info.setLayoutCount = 1;                                     // 指定描述符集布局的数量
    layout_info.pSetLayouts = &texture_layout;                          // 指定描述符集布局的指针
    layout_info.pushConstantRangeCount = 1;                             // 指定推送常量范围的数量
    layout_info.pPushConstantRanges = &range;                           // 指定推送常量范围的指针

    if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &layout_) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create pipeline layout");
    }  // 创建管线布局

    VkPipelineShaderStageCreateInfo stages[2]{};  // 定义管线着色器阶段信息数组
    for (size_t i = 0; i < 2; ++i) {
      stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;  // 指定结构体类型为管线着色器阶段创建信息
      stages[i].pName = "main";                                               // 指定着色器入口函数名称
    }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;    // 指定顶点着色器阶段
    stages[0].module = vertex.handle();              // 设置顶点着色器模块
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // 指定片段着色器阶段
    stages[1].module = fragment.handle();            // 设置片段着色器模块

    VkVertexInputBindingDescription binding{
      0, sizeof(SpriteInstance), VK_VERTEX_INPUT_RATE_INSTANCE};  // 定义顶点输入绑定描述信息结构体

    VkVertexInputAttributeDescription attributes[]{
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(SpriteInstance, rectangle)},                                       // 矩形属性
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(SpriteInstance, rectangle) + offsetof(engine::Rectangle, color)},  // 颜色属性
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(SpriteInstance, uv_left)},                                         // UV属性
      {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(SpriteInstance, pivot_x)}                                          // 旋转中心属性
    };  // 定义顶点输入属性描述信息结构体数组

    VkPipelineVertexInputStateCreateInfo input{};                             // 创建顶点输入状态创建信息结构体
    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;  // 指定结构体类型为顶点输入状态创建信息
    input.vertexBindingDescriptionCount = 1;                                  // 指定顶点绑定描述信息的数量
    input.pVertexBindingDescriptions = &binding;                              // 指定顶点绑定描述信息的指针
    input.vertexAttributeDescriptionCount = 4;                                // 指定顶点属性描述信息的数量
    input.pVertexAttributeDescriptions = attributes;                          // 指定顶点属性描述信息的指针

    VkPipelineInputAssemblyStateCreateInfo assembly{};                             // 创建输入装配状态创建信息结构体
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;  // 指定结构体类型为输入装配状态创建信息
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;                       // 指定图元拓扑类型为三角形列表

    VkPipelineViewportStateCreateInfo viewport{};                            // 创建视口状态创建信息结构体
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;  // 指定结构体类型为视口状态创建信息
    viewport.viewportCount = 1;                                              // 指定视口的数量
    viewport.scissorCount = 1;                                               // 指定裁剪矩形的数量

    VkPipelineRasterizationStateCreateInfo raster{};                            // 创建光栅化状态创建信息结构体
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;  // 指定结构体类型为光栅化状态创建信息
    raster.polygonMode = VK_POLYGON_MODE_FILL;                                  // 指定多边形填充模式
    raster.cullMode = VK_CULL_MODE_NONE;                                        // 指定不进行面剔除
    raster.lineWidth = 1.0f;                                                    // 指定线宽

    VkPipelineMultisampleStateCreateInfo samples{};                            // 创建多重采样状态创建信息结构体
    samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;  // 指定结构体类型为多重采样状态创建信息
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;                      // 指定光栅化采样数为1

    VkPipelineColorBlendAttachmentState attachment{
      .blendEnable = VK_TRUE,                                      // 启用颜色混合
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // 指定源颜色混合因子为源 alpha
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // 指定目标颜色混合因子为 1 - 源 alpha
      .colorBlendOp = VK_BLEND_OP_ADD,                             // 指定颜色混合操作为加法
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,                  // 指定源 alpha 混合因子为 1
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // 指定目标 alpha 混合因子为 1 - 源 alpha
      .alphaBlendOp = VK_BLEND_OP_ADD,                             // 指定 alpha 混合操作为加法
      .colorWriteMask = 0xF                                        // 指定颜色写入掩码为全部通道
    };  // 创建颜色混合附件状态结构体

    VkPipelineColorBlendStateCreateInfo blend{};                             // 创建颜色混合状态创建信息结构体
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;  // 指定结构体类型为颜色混合状态创建信息
    blend.attachmentCount = 1;                                               // 指定颜色混合附件的数量
    blend.pAttachments = &attachment;                                        // 指定颜色混合附件的指针

    VkDynamicState states[]{
      VK_DYNAMIC_STATE_VIEWPORT,  // 指定视口为动态状态
      VK_DYNAMIC_STATE_SCISSOR    // 指定裁剪矩形为动态状态
    };  // 创建动态状态数组

    VkPipelineDynamicStateCreateInfo dynamic{};                            // 创建动态状态创建信息结构体
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;  // 指定结构体类型为动态状态创建信息
    dynamic.dynamicStateCount = 2;                                         // 指定动态状态的数量
    dynamic.pDynamicStates = states;                                       // 指定动态状态数组的指针

    VkGraphicsPipelineCreateInfo info{};                           // 创建图形管线创建信息结构体
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;  // 指定结构体类型为图形管线创建信息
    info.stageCount = 2;                                           // 指定着色器阶段的数量
    info.pStages = stages;                                         // 指定着色器阶段数组的指针
    info.pVertexInputState = &input;                               // 指定顶点输入状态的指针
    info.pInputAssemblyState = &assembly;                          // 指定输入装配状态的指针
    info.pViewportState = &viewport;                               // 指定视口状态的指针
    info.pRasterizationState = &raster;                            // 指定光栅化状态的指针
    info.pMultisampleState = &samples;                             // 指定多重采样状态的指针
    info.pColorBlendState = &blend;                                // 指定颜色混合状态的指针
    info.pDynamicState = &dynamic;                                 // 指定动态状态的指针
    info.layout = layout_;                                         // 指定管线布局
    info.renderPass = pass;                                        // 指定渲染通道

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create graphics pipeline");
    }  // 如果创建图形管线失败
  } catch (...) {
    release();  // 释放图形管线资源以防止资源泄漏
    throw;
  }
}

SpritePipeline::~SpritePipeline() {
  release();  // 释放图形管线资源
}

void SpritePipeline::release() noexcept {
  if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);    // 销毁图形管线
  if (layout_) vkDestroyPipelineLayout(device_, layout_, nullptr);  // 销毁管线布局
  pipeline_ = VK_NULL_HANDLE;                                       // 重置图形管线句柄
  layout_ = VK_NULL_HANDLE;                                         // 重置管线布局句柄
}
}  // namespace engine::render