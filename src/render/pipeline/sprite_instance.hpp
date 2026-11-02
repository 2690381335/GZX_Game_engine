#pragma once

#include <cstddef>
#include <type_traits>

#include "../../../include/engine/render_frame.hpp"

namespace engine::render {
/**
 * @brief 表示具有其矩形和 UV 坐标的精灵实例。
 *
 */
struct SpriteInstance {
  engine::Rectangle rectangle{
    0.0f, 0.0f, 64.0f, 64.0f,
    engine::Color{1.0f, 1.0f, 1.0f, 1.0f}};
  float uv_left = 0.0f;                            // 精灵左侧的 UV 坐标
  float uv_top = 0.0f;                             // 精灵顶部的 UV 坐标
  float uv_right = 1.0f;                           // 精灵右侧的 UV 坐标
  float uv_bottom = 1.0f;                          // 精灵底部的 UV 坐标
  float pivot_x = 0.5f, pivot_y = 0.5f;            // 精灵的旋转中心，默认为矩形中心
  float rotation_sin = 0.0f, rotation_cos = 1.0f;  // 精灵的旋转角度的正弦和余弦值
};

static_assert(std::is_standard_layout_v<SpriteInstance>, "SpriteInstance must have a standard layout");    // 确保结构体具有标准的内存布局，is_standard_layout_v 用于检查类型是否具有标准布局
static_assert(std::is_trivially_copyable_v<SpriteInstance>, "SpriteInstance must be trivially copyable");  // 确保结构体可以按位复制，is_trivially_copyable_v 用于检查类型是否可以按位复制
static_assert(offsetof(SpriteInstance, rectangle) == 0, "Rectangle must be at offset 0");                  // 确保 rectangle 成员在偏移量 0，offsetof 用于获取成员的偏移量
static_assert(offsetof(SpriteInstance, uv_left) == 32, "UV left must be at offset 32");                    // 确保 uv_left 成员在偏移量 32，offsetof 用于获取成员的偏移量
static_assert(offsetof(SpriteInstance, pivot_x) == 48, "Pivot X must be at offset 48");                    // 确保 pivot_x 成员在偏移量 48，offsetof 用于获取成员的偏移量
static_assert(sizeof(SpriteInstance) == 64, "SpriteInstance must be 64 bytes in size");                    // 确保结构体的大小为 64 字节，sizeof 用于获取类型的大小
}  // namespace engine::render