#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>  // for offsetof
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace engine {
/**
 * @brief 表示颜色的结构体。
 * @details 包含红、绿、蓝和透明度四个分量，取值范围为0.0到1.0。
 */
struct Color {
  float red = 0.0f, green = 0.0f, blue = 0.0f, alpha = 1.0f;
};

/**
 * @brief 表示矩形的结构体。
 * @details 包含矩形的位置、尺寸和颜色信息。
 */
struct Rectangle {
  float x = 0.0f, y = 0.0f, width = 64.0f, height = 64.0f;
  Color color{0.9f, 0.3f, 0.2f, 1.0f};
};

static_assert(std::is_standard_layout_v<Rectangle>, "Rectangle must have a standard layout");    // 确保Rectangle具有标准布局
static_assert(std::is_trivially_copyable_v<Rectangle>, "Rectangle must be trivially copyable");  // 确保Rectangle是平凡可拷贝的类型
static_assert(sizeof(Rectangle) == 32, "Rectangle must be 32 bytes");                            // 确保Rectangle的大小为32字节
static_assert(offsetof(Rectangle, color) == 16, "Color must be at offset 16 in Rectangle");      // 确保Color在Rectangle中的偏移量为16字节

/**
 * @brief 表示纹理句柄的结构体。
 * @details 包含纹理在纹理池中的索引信息，用于标识和访问纹理资源。
 */
struct TextureHandle {
  std::uint32_t index = std::numeric_limits<std::uint32_t>::max();  // 默认值为无效索引，numeric_limits提供了类型的极值信息
};

/**
 * @brief 表示纹理的UV坐标矩形。
 * @details 包含纹理在UV空间中的左、上、右和下边界坐标，取值范围为0.0到1.0。
 */
struct UvRect {
  float left = 0.0f, top = 0.0f, right = 1.0f, bottom = 1.0f;
};

/**
 * @brief 坐标空间的枚举类型。
 * @details 用于区分世界坐标和屏幕坐标。
 */
enum class CoordinateSpace {
  World,  // 世界坐标
  Screen  // 屏幕坐标
};

/**
 * @brief 表示精灵的结构体。
 * @details 包含精灵的纹理句柄、矩形信息、UV坐标矩形、坐标空间、旋转角度、旋转中心和层级，用于在渲染帧中绘制精灵。
 */
struct Sprite {
  TextureHandle texture;                                        // 纹理句柄，用于标识和访问纹理资源
  Rectangle rectangle{0, 0, 64, 64, {1.0f, 1.0f, 1.0f, 1.0f}};  // 矩形的初始位置、尺寸和颜色信息
  UvRect uv;                                                    // 纹理的UV坐标矩形
  CoordinateSpace space = CoordinateSpace::World;               // 坐标空间，默认为世界坐标
  float rotation = 0.0f;                                        // 精灵的旋转角度，默认为0度
  float pivot_x = 0.5f, pivot_y = 0.5f;                         // 精灵的旋转中心，默认为矩形中心
  std::int32_t layer = 0;                                       // 精灵的层级，数值越大越靠上绘制
};

/**
 * @brief 渲染帧中精灵的排序模式。
 * @details 控制渲染器合批前如何重排精灵，排序结果同时决定拾取优先级。
 */
enum class SortMode : std::uint8_t {
  None,         // 不排序，严格按提交顺序绘制
  Layer,        // 按层级稳定排序，同层保持提交顺序
  LayerTexture  // 按层级再按纹理排序，同层内不同纹理的重叠顺序不保证，但合批效率最高
};

/**
 * @brief 计算精灵在指定排序模式下的绘制顺序键。
 * @details 键的高32位为偏移后的层级，低32位为纹理索引（仅 LayerTexture 模式），键相同时按提交顺序绘制。
 *
 * @param sprite 要计算键的精灵。
 * @param mode 排序模式。
 * @return std::uint64_t 绘制顺序键，数值越大越靠上绘制。
 */
constexpr std::uint64_t drawOrderKey(const Sprite& sprite, SortMode mode) noexcept {
  if (mode == SortMode::None) return 0;                                                                // 不排序时所有精灵键相同
  const auto layer = static_cast<std::uint64_t>(static_cast<std::uint32_t>(sprite.layer) ^ 0x80000000u);  // 翻转符号位，使有符号层级按无符号整数正确排序
  const auto texture = mode == SortMode::LayerTexture ? sprite.texture.index : 0u;                     // 仅在按纹理排序时加入纹理索引
  return (layer << 32) | texture;
}

/**
 * @brief 表示二维点的结构体。
 * @details 包含点的X和Y坐标信息，默认为原点(0,0)。
 */
struct Point2D {
  float x = 0.0f, y = 0.0f;  // 点的X和Y坐标，默认为原点(0,0)
};

/**
 * @brief 表示二维相机的结构体。
 * @details 包含相机在世界空间中的位置和缩放比例信息。
 */
struct Camera2D {
  float x = 0.0f;     // 相机在世界空间中的X坐标
  float y = 0.0f;     // 相机在世界空间中的Y坐标
  float zoom = 1.0f;  // 相机的缩放比例

  /**
   * @brief 将世界坐标转换为屏幕坐标。
   * @details 根据相机的位置和缩放比例，将世界空间中的点转换为屏幕空间中的点。
   *
   * @param world 世界空间中的点坐标。
   * @return constexpr Point2D 屏幕空间中的点坐标。
   */
  constexpr Point2D worldToScreen(Point2D world) const noexcept {
    return {(world.x - x) * zoom, (world.y - y) * zoom};
  }

  /**
   * @brief 将屏幕坐标转换为世界坐标。
   * @details 根据相机的位置和缩放比例，将屏幕空间中的点转换为世界空间中的点。
   *
   * @param screen 屏幕空间中的点坐标。
   * @return constexpr Point2D 世界空间中的点坐标。
   */
  constexpr Point2D screenToWorld(Point2D screen) const noexcept {
    return {screen.x / zoom + x, screen.y / zoom + y};
  }
};

/**
 * @brief 判断屏幕坐标是否与精灵发生碰撞。
 * @details 根据精灵的矩形信息、旋转角度和旋转中心，判断屏幕空间中的点是否在精灵的可见区域内。
 *
 * @param sprite 要进行碰撞检测的精灵。
 * @param screen 屏幕空间中的点坐标。
 * @param camera 当前的二维相机信息。
 * @return true 如果点在精灵的可见区域内，则返回true。
 * @return false 如果点不在精灵的可见区域内，则返回false。
 */
inline bool hitTest(const Sprite& sprite, Point2D screen, const Camera2D& camera) {
  const auto point = sprite.space == CoordinateSpace::World ? camera.screenToWorld(screen) : screen;  // 将屏幕坐标转换为世界坐标（如果精灵在世界坐标空间中）
  const auto& rectangle = sprite.rectangle;                                                           // 获取精灵的矩形信息
  if (rectangle.width == 0.0f || rectangle.height == 0.0f) return false;                              // 如果矩形的宽度或高度为0，则命中测试失败

  const float pivot_x = sprite.pivot_x * rectangle.width;                // 计算旋转中心在矩形中的X偏移
  const float pivot_y = sprite.pivot_y * rectangle.height;               // 计算旋转中心在矩形中的Y偏移
  const float offset_x = point.x - rectangle.x - pivot_x;                // 计算点相对于旋转中心的X偏移
  const float offset_y = point.y - rectangle.y - pivot_y;                // 计算点相对于旋转中心的Y偏移
  const float sine = std::sin(sprite.rotation);                          // 计算旋转角度的正弦值
  const float cosine = std::cos(sprite.rotation);                        // 计算旋转角度的余弦值
  const float local_x = offset_x * cosine + offset_y * sine + pivot_x;   // 计算点在局部矩形中的X坐标
  const float local_y = -offset_x * sine + offset_y * cosine + pivot_y;  // 计算点在局部矩形中的Y坐标

  return local_x >= std::min(0.0f, rectangle.width) &&   // 判断点在局部矩形中的X坐标是否大于等于矩形的最小X边界
         local_x <= std::max(0.0f, rectangle.width) &&   // 判断点在局部矩形中的X坐标是否小于等于矩形的最大X边界
         local_y >= std::min(0.0f, rectangle.height) &&  // 判断点在局部矩形中的Y坐标是否大于等于矩形的最小Y边界
         local_y <= std::max(0.0f, rectangle.height);    // 判断点在局部矩形中的Y坐标是否小于等于矩形的最大Y边界
}

/**
 * @brief 表示渲染帧的结构体。
 * @details 包含渲染帧的背景颜色信息，以及渲染帧中精灵的相关信息。
 */
struct RenderFrame {
  Color background{0.06f, 0.09f, 0.16f, 1.0f};
  std::vector<Sprite> sprites;     // 渲染帧中的精灵信息
  int viewport_width = 0;          // 视口的宽度
  int viewport_height = 0;         // 视口的高度
  Camera2D camera;                 // 二维相机信息
  SortMode sort = SortMode::Layer;  // 精灵排序模式，默认按层级稳定排序

  /**
   * @brief 重置渲染帧的内容。
   * @details 将背景颜色、相机和排序模式重置为默认值，并清空渲染帧中的所有精灵信息（保留容量以避免重复分配）。
   */
  void reset() {
    background = Color{0.06f, 0.09f, 0.16f, 1.0f};
    sprites.clear();         // 清空渲染帧中的所有精灵信息
    camera = Camera2D{};     // 重置二维相机信息
    sort = SortMode::Layer;  // 重置排序模式
  }

  /**
   * @brief 在渲染帧中绘制一个精灵。
   * @details 根据提供的纹理句柄、目标矩形、UV坐标矩形、颜色信息、坐标空间和层级，将精灵添加到渲染帧中，并返回刚刚添加的精灵的引用。
   *
   * @param texture 精灵的纹理句柄，用于标识和访问纹理资源。
   * @param destination 精灵在渲染帧中的目标矩形，包含位置、尺寸和颜色信息。
   * @param uv 精灵的UV坐标矩形，默认为整个纹理区域。
   * @param tint 精灵的颜色信息，用于对精灵进行着色，默认为白色。
   * @param space 坐标空间，默认为世界坐标。
   * @param layer 精灵的层级，默认为0。
   * @return Sprite& 返回刚刚添加到渲染帧中的精灵的引用。
   */
  Sprite& drawSprite(TextureHandle texture, const Rectangle& destination, UvRect uv = {}, Color tint = {1.0f, 1.0f, 1.0f, 1.0f}, CoordinateSpace space = CoordinateSpace::World, std::int32_t layer = 0) {
    return sprites.emplace_back(Sprite{
      texture,
      {destination.x, destination.y, destination.width, destination.height, tint},
      uv,
      space,
      0.0f, 0.5f, 0.5f,
      layer});  // 直接在容器末尾构造精灵并返回引用
  }

  /**
   * @brief 在渲染帧中绘制一个矩形。
   * @details 根据提供的矩形信息、坐标空间和层级，将矩形添加到渲染帧中，使用默认的纹理句柄。
   *
   * @param rectangle 矩形的位置信息、尺寸和颜色信息。
   * @param space 坐标空间，默认为世界坐标。
   * @param layer 矩形的层级，默认为0。
   */
  void drawRectangle(const Rectangle& rectangle, CoordinateSpace space = CoordinateSpace::World, std::int32_t layer = 0) {
    drawSprite(TextureHandle{0}, rectangle, {}, rectangle.color, space, layer);  // 使用默认纹理句柄绘制矩形
  }

  /**
   * @brief 在渲染帧中根据屏幕坐标拾取精灵。
   * @details 遍历渲染帧中的所有精灵，在所有命中的精灵中返回按当前排序模式最后绘制（即最上层）的精灵索引，如果没有命中则返回空值。
   *
   * @param screen 屏幕空间中的坐标点。
   * @return std::optional<std::size_t> 返回命中的精灵的索引，如果没有命中则返回空值。
   */
  std::optional<std::size_t> pick(Point2D screen) const {
    std::optional<std::size_t> best;  // 当前找到的最上层精灵索引
    std::uint64_t best_key = 0;       // 当前最上层精灵的绘制顺序键
    for (std::size_t index = sprites.size(); index > 0; --index) {  // 从后往前遍历，键相同时后提交的精灵优先
      const auto& sprite = sprites[index - 1];
      const auto key = drawOrderKey(sprite, sort);
      if (best && key <= best_key) continue;  // 绘制顺序不高于已命中的精灵，无需做碰撞测试
      if (hitTest(sprite, screen, camera)) {
        best = index - 1;  // 记录命中的精灵的索引
        best_key = key;    // 记录命中的精灵的绘制顺序键
        if (sort == SortMode::None) break;  // 不排序时第一个命中的就是最上层
      }
    }
    return best;  // 返回命中的精灵索引，如果没有命中的精灵则为空值
  }
};
}  // namespace engine