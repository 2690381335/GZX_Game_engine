#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assets.hpp"
#include "render_frame.hpp"

namespace engine {
/**
 * @brief 位图字体，每个字符对应纹理图集中的一个区域，文字以精灵形式合批绘制；TTF 字体在加载时一次性烘焙为图集，绘制时不再访问 SDL_ttf。
 *
 */
class BitmapFont {
 public:
  /**
   * @brief 使用引擎内置的 8x8 ASCII 字体创建位图字体，图集会注册到资源管理器中。
   *
   * @param assets 资源管理器，用于创建字体图集纹理。
   * @return BitmapFont 内置字体对象。
   */
  static BitmapFont createDefault(Assets& assets);

  /**
   * @brief 从按网格排列的字体图集创建位图字体。
   *
   * @param assets 资源管理器，用于查询图集纹理尺寸。
   * @param texture 字体图集纹理句柄。
   * @param cell_width 每个字符格子的宽度（像素）。
   * @param cell_height 每个字符格子的高度（像素）。
   * @param first 图集中第一个字符的编码，默认为空格。
   * @param count 图集中的字符数量，默认为 95 个可打印 ASCII 字符。
   * @return BitmapFont 字体对象。
   */
  static BitmapFont fromGrid(const Assets& assets, TextureHandle texture, std::uint32_t cell_width, std::uint32_t cell_height, char32_t first = U' ', std::uint32_t count = 95);

  /**
   * @brief 使用 SDL_ttf 将 TTF/OTF 字体中的指定字符烘焙为图集，支持比例字体和中文等非 ASCII 字符。
   *
   * @param assets 资源管理器，用于创建字体图集纹理。
   * @param path 字体文件路径。
   * @param pixel_size 字号（像素）。
   * @param characters 需要烘焙的字符（UTF-8），为空时使用 95 个可打印 ASCII 字符，可打印 ASCII 字符始终会被烘焙。
   * @return BitmapFont 字体对象。
   */
  static BitmapFont fromTtf(Assets& assets, const std::filesystem::path& path, float pixel_size, std::string_view characters = {});

  /**
   * @brief 在渲染帧中绘制一段 UTF-8 文本，支持换行符，不在字体中的字符以问号代替。
   *
   * @param frame 渲染帧。
   * @param text 要绘制的 UTF-8 文本。
   * @param x 文本左上角的 X 坐标。
   * @param y 文本左上角的 Y 坐标。
   * @param scale 文本缩放比例，1 表示按字形原始像素尺寸绘制。
   * @param color 文本颜色。
   * @param space 坐标空间。
   * @param layer 文本的层级。
   */
  void draw(RenderFrame& frame, std::string_view text, float x, float y, float scale = 1.0f, Color color = {1.0f, 1.0f, 1.0f, 1.0f}, CoordinateSpace space = CoordinateSpace::Screen, std::int32_t layer = 0) const;

  /**
   * @brief 计算文本绘制后的包围尺寸。
   *
   * @param text 要测量的 UTF-8 文本。
   * @param scale 文本缩放比例。
   * @return Point2D 文本的宽度（x）和高度（y）。
   */
  Point2D measure(std::string_view text, float scale = 1.0f) const noexcept;

  /**
   * @brief 获取字体图集的纹理句柄。
   *
   * @return TextureHandle 字体图集的纹理句柄。
   */
  TextureHandle texture() const noexcept { return texture_; }

  /**
   * @brief 获取字形的宽度（像素），等宽字体为格子宽度，TTF 字体为最大水平步进。
   *
   * @return float 字形宽度。
   */
  float glyphWidth() const noexcept { return glyph_width_; }

  /**
   * @brief 获取行高（像素）。
   *
   * @return float 行高。
   */
  float glyphHeight() const noexcept { return glyph_height_; }

 private:
  /**
   * @brief 单个字形的图集区域和排版度量，尺寸均为未缩放的像素值。
   *
   */
  struct Glyph {
    UvRect uv;              // 字形在图集中的 UV 区域
    float offset_x = 0.0f;  // 字形左上角相对笔位的 X 偏移
    float offset_y = 0.0f;  // 字形左上角相对行顶的 Y 偏移
    float width = 0.0f;     // 字形宽度，为 0 时不生成精灵
    float height = 0.0f;    // 字形高度
    float advance = 0.0f;   // 绘制后笔位的水平步进
    bool present = false;   // 字体中是否包含该字符
  };

  /**
   * @brief 构造位图字体对象。
   *
   * @param texture 字体图集纹理句柄。
   * @param glyph_width 字形宽度。
   * @param glyph_height 行高。
   * @param first 连续字形表中第一个字符的编码。
   * @param glyphs 连续字形表，按编码顺序排列。
   * @param extended 连续字形表范围之外的字形。
   */
  BitmapFont(TextureHandle texture, float glyph_width, float glyph_height, char32_t first, std::vector<Glyph> glyphs, std::unordered_map<char32_t, Glyph> extended = {});

  /**
   * @brief 查找字符对应的字形，不存在时返回问号或空指针。
   *
   * @param code 字符编码。
   * @return const Glyph* 字形指针。
   */
  const Glyph* glyph(char32_t code) const noexcept;

  /**
   * @brief 精确查找字符对应的字形，不做替代。
   *
   * @param code 字符编码。
   * @return const Glyph* 字形指针，不存在时为空。
   */
  const Glyph* find(char32_t code) const noexcept;

 private:
  TextureHandle texture_;                          // 字体图集纹理句柄
  float glyph_width_ = 0.0f;                       // 字形宽度（像素）
  float glyph_height_ = 0.0f;                      // 行高（像素）
  char32_t first_ = U' ';                          // 连续字形表中第一个字符的编码
  std::vector<Glyph> glyphs_;                      // 连续字形表，ASCII 等热点字符 O(1) 索引
  std::unordered_map<char32_t, Glyph> extended_;  // 连续范围之外的字形（如中文）
};
}  // namespace engine
