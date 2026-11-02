#include "../../include/engine/bitmap_font.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "SDL3/SDL.h"
#include "SDL3_ttf/SDL_ttf.h"

namespace engine {
namespace {
/**
 * @brief 内置 8x8 ASCII 字体数据（font8x8_basic，公有领域），覆盖 U+0020 到 U+007E，每字节一行，最低位为最左侧像素。
 *
 */
constexpr std::uint8_t kFont8x8[95][8]{
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // U+0020 (空格)
  {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},  // U+0021 (!)
  {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // U+0022 (")
  {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},  // U+0023 (#)
  {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},  // U+0024 ($)
  {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},  // U+0025 (%)
  {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},  // U+0026 (&)
  {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},  // U+0027 (')
  {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},  // U+0028 (()
  {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},  // U+0029 ())
  {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},  // U+002A (*)
  {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},  // U+002B (+)
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // U+002C (,)
  {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},  // U+002D (-)
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // U+002E (.)
  {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},  // U+002F (/)
  {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},  // U+0030 (0)
  {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},  // U+0031 (1)
  {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},  // U+0032 (2)
  {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},  // U+0033 (3)
  {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},  // U+0034 (4)
  {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},  // U+0035 (5)
  {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},  // U+0036 (6)
  {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},  // U+0037 (7)
  {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},  // U+0038 (8)
  {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},  // U+0039 (9)
  {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // U+003A (:)
  {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // U+003B (;)
  {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},  // U+003C (<)
  {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},  // U+003D (=)
  {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},  // U+003E (>)
  {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},  // U+003F (?)
  {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},  // U+0040 (@)
  {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},  // U+0041 (A)
  {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},  // U+0042 (B)
  {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},  // U+0043 (C)
  {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},  // U+0044 (D)
  {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},  // U+0045 (E)
  {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},  // U+0046 (F)
  {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},  // U+0047 (G)
  {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},  // U+0048 (H)
  {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // U+0049 (I)
  {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},  // U+004A (J)
  {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},  // U+004B (K)
  {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},  // U+004C (L)
  {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},  // U+004D (M)
  {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},  // U+004E (N)
  {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},  // U+004F (O)
  {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},  // U+0050 (P)
  {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},  // U+0051 (Q)
  {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},  // U+0052 (R)
  {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},  // U+0053 (S)
  {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // U+0054 (T)
  {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},  // U+0055 (U)
  {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // U+0056 (V)
  {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},  // U+0057 (W)
  {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},  // U+0058 (X)
  {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},  // U+0059 (Y)
  {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},  // U+005A (Z)
  {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},  // U+005B ([)
  {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},  // U+005C (反斜杠)
  {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},  // U+005D (])
  {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},  // U+005E (^)
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},  // U+005F (_)
  {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},  // U+0060 (`)
  {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},  // U+0061 (a)
  {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},  // U+0062 (b)
  {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},  // U+0063 (c)
  {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00},  // U+0064 (d)
  {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00},  // U+0065 (e)
  {0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00},  // U+0066 (f)
  {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // U+0067 (g)
  {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},  // U+0068 (h)
  {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // U+0069 (i)
  {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},  // U+006A (j)
  {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},  // U+006B (k)
  {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // U+006C (l)
  {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},  // U+006D (m)
  {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},  // U+006E (n)
  {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},  // U+006F (o)
  {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},  // U+0070 (p)
  {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},  // U+0071 (q)
  {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},  // U+0072 (r)
  {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},  // U+0073 (s)
  {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},  // U+0074 (t)
  {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},  // U+0075 (u)
  {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // U+0076 (v)
  {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},  // U+0077 (w)
  {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},  // U+0078 (x)
  {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // U+0079 (y)
  {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},  // U+007A (z)
  {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},  // U+007B ({)
  {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},  // U+007C (|)
  {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},  // U+007D (})
  {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // U+007E (~)
};

constexpr std::uint32_t kGlyphSize = 8;                                   // 内置字形的边长（像素）
constexpr std::uint32_t kCellSize = kGlyphSize + 2;                       // 图集格子边长，四周各留 1 像素透明边防止缩小采样时串色
constexpr std::uint32_t kAtlasColumns = 16;                               // 图集每行的格子数
constexpr std::uint32_t kGlyphCount = 95;                                 // 内置字符数量
constexpr std::uint32_t kAtlasRows = (kGlyphCount + kAtlasColumns - 1) / kAtlasColumns;  // 图集行数
constexpr std::uint32_t kMaxAtlasSize = 8192;                             // TTF 图集的最大边长

/**
 * @brief 从 UTF-8 文本中解码下一个码点，非法序列返回 U+FFFD 并前进 1 字节。
 *
 * @param text 文本。
 * @param position 当前读取位置，解码后前进到下一个码点。
 * @return char32_t 解码出的码点。
 */
char32_t decodeUtf8(std::string_view text, std::size_t& position) noexcept {
  const auto lead = static_cast<unsigned char>(text[position]);  // 当前码点的首字节
  if (lead < 0x80) {
    ++position;   // ASCII 单字节，最常见路径
    return lead;
  }

  const std::size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 0;  // 根据首字节推断序列长度
  if (length == 0 || position + length > text.size()) {
    ++position;
    return U'\uFFFD';  // 非法首字节或序列被截断
  }

  char32_t code = lead & (0xFF >> (length + 1));  // 提取首字节中的有效位
  for (std::size_t offset = 1; offset < length; ++offset) {
    const auto next = static_cast<unsigned char>(text[position + offset]);
    if ((next & 0xC0) != 0x80) {
      ++position;
      return U'\uFFFD';  // 后续字节不是延续字节
    }
    code = (code << 6) | (next & 0x3F);  // 拼接后续字节的 6 个有效位
  }
  position += length;
  return code;
}

/**
 * @brief SDL_ttf 引用计数的 RAII 封装。
 *
 */
struct TtfLibrary {
  TtfLibrary() {
    if (!TTF_Init()) throw std::runtime_error(std::string("Failed to initialize SDL_ttf: ") + SDL_GetError());
  }
  ~TtfLibrary() { TTF_Quit(); }
  TtfLibrary(const TtfLibrary&) = delete;
  TtfLibrary& operator=(const TtfLibrary&) = delete;
};

/**
 * @brief 烘焙过程中单个字形的裁剪后像素和布局信息。
 *
 */
struct RasterGlyph {
  char32_t code = 0;             // 字符编码
  int offset_x = 0;              // 裁剪区域相对笔位的 X 偏移
  int offset_y = 0;              // 裁剪区域相对行顶的 Y 偏移
  int width = 0;                 // 裁剪后的宽度
  int height = 0;                // 裁剪后的高度
  int advance = 0;               // 水平步进
  std::uint32_t atlas_x = 0;     // 在图集中的 X 坐标
  std::uint32_t atlas_y = 0;     // 在图集中的 Y 坐标
  std::vector<std::byte> rgba;   // 裁剪后的 RGBA8 像素
};

/**
 * @brief 渲染字形并裁剪掉透明边缘。
 *
 * @param font SDL_ttf 字体。
 * @param code 字符编码。
 * @param glyph 输出的字形，字形为空白时宽高为 0。
 */
void rasterize(TTF_Font* font, char32_t code, RasterGlyph& glyph) {
  SDL_Surface* rendered = TTF_RenderGlyph_Blended(font, code, SDL_Color{255, 255, 255, 255});  // 白色字形，绘制时由精灵颜色着色
  if (!rendered) return;                                                                         // 空白字符（如空格）没有像素
  SDL_Surface* surface = SDL_ConvertSurface(rendered, SDL_PIXELFORMAT_RGBA32);                  // 统一为字节序 RGBA
  SDL_DestroySurface(rendered);
  if (!surface) throw std::runtime_error(std::string("Failed to convert glyph surface: ") + SDL_GetError());
  std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> owner(surface, SDL_DestroySurface);

  const auto* pixels = static_cast<const std::uint8_t*>(surface->pixels);
  int left = surface->w, top = surface->h, right = -1, bottom = -1;  // 不透明像素的包围盒
  for (int y = 0; y < surface->h; ++y) {
    const auto* row = pixels + static_cast<std::ptrdiff_t>(y) * surface->pitch;
    for (int x = 0; x < surface->w; ++x) {
      if (row[x * 4 + 3] == 0) continue;
      left = std::min(left, x);
      right = std::max(right, x);
      top = std::min(top, y);
      bottom = std::max(bottom, y);
    }
  }
  if (right < 0) return;  // 字形完全透明

  glyph.offset_x = left;
  glyph.offset_y = top;
  glyph.width = right - left + 1;
  glyph.height = bottom - top + 1;
  glyph.rgba.resize(static_cast<std::size_t>(glyph.width) * glyph.height * 4);
  for (int y = 0; y < glyph.height; ++y) {
    std::memcpy(glyph.rgba.data() + static_cast<std::size_t>(y) * glyph.width * 4,
                pixels + static_cast<std::ptrdiff_t>(top + y) * surface->pitch + left * 4,
                static_cast<std::size_t>(glyph.width) * 4);
  }
}

/**
 * @brief 使用货架算法把字形打包进指定宽度的图集，字形间留 1 像素透明边。
 *
 * @param glyphs 按高度降序排列的字形指针。
 * @param width 图集宽度。
 * @return std::uint32_t 图集所需高度。
 */
std::uint32_t pack(std::span<RasterGlyph* const> glyphs, std::uint32_t width) noexcept {
  std::uint32_t x = 1, y = 1, shelf = 0;  // 当前笔位和当前货架高度
  for (auto* glyph : glyphs) {
    const auto w = static_cast<std::uint32_t>(glyph->width), h = static_cast<std::uint32_t>(glyph->height);
    if (x + w + 1 > width) {
      x = 1;  // 当前货架放不下，换到下一层
      y += shelf + 1;
      shelf = 0;
    }
    glyph->atlas_x = x;
    glyph->atlas_y = y;
    x += w + 1;
    shelf = std::max(shelf, h);
  }
  return y + shelf + 1;
}
}  // namespace

BitmapFont::BitmapFont(TextureHandle texture, float glyph_width, float glyph_height, char32_t first, std::vector<Glyph> glyphs, std::unordered_map<char32_t, Glyph> extended)
    : texture_(texture), glyph_width_(glyph_width), glyph_height_(glyph_height), first_(first), glyphs_(std::move(glyphs)), extended_(std::move(extended)) {}

BitmapFont BitmapFont::createDefault(Assets& assets) {
  constexpr std::uint32_t width = kAtlasColumns * kCellSize;  // 图集宽度
  constexpr std::uint32_t height = kAtlasRows * kCellSize;    // 图集高度

  std::vector<std::byte> rgba(std::size_t{width} * height * 4, std::byte{255});  // 默认全部为白色
  for (std::size_t pixel = 0; pixel < std::size_t{width} * height; ++pixel) rgba[pixel * 4 + 3] = std::byte{0};  // 默认全部透明

  std::vector<Glyph> glyphs;  // 每个字符的字形
  glyphs.reserve(kGlyphCount);
  for (std::uint32_t glyph = 0; glyph < kGlyphCount; ++glyph) {
    const std::uint32_t origin_x = (glyph % kAtlasColumns) * kCellSize + 1;  // 字形在图集中的左上角 X 坐标（跳过 1 像素边）
    const std::uint32_t origin_y = (glyph / kAtlasColumns) * kCellSize + 1;  // 字形在图集中的左上角 Y 坐标（跳过 1 像素边）
    for (std::uint32_t row = 0; row < kGlyphSize; ++row) {
      const auto bits = kFont8x8[glyph][row];  // 当前行的像素位图
      for (std::uint32_t column = 0; column < kGlyphSize; ++column) {
        if (!(bits & (1u << column))) continue;                                                                    // 该像素未点亮
        rgba[(std::size_t{origin_y + row} * width + origin_x + column) * 4 + 3] = std::byte{255};  // 点亮像素设置为不透明
      }
    }
    constexpr float size = static_cast<float>(kGlyphSize);
    glyphs.push_back({{static_cast<float>(origin_x) / width,
                       static_cast<float>(origin_y) / height,
                       static_cast<float>(origin_x + kGlyphSize) / width,
                       static_cast<float>(origin_y + kGlyphSize) / height},
                      0.0f, 0.0f, glyph == 0 ? 0.0f : size, size, size, true});  // 空格只步进不生成精灵
  }

  const auto texture = assets.createTexture(width, height, rgba);  // 注册字体图集纹理
  return BitmapFont(texture, static_cast<float>(kGlyphSize), static_cast<float>(kGlyphSize), U' ', std::move(glyphs));
}

BitmapFont BitmapFont::fromGrid(const Assets& assets, TextureHandle texture, std::uint32_t cell_width, std::uint32_t cell_height, char32_t first, std::uint32_t count) {
  const auto size = assets.textureSize(texture);  // 获取图集纹理尺寸
  if (cell_width == 0 || cell_height == 0 || count == 0) throw std::invalid_argument("Invalid bitmap font cell size or glyph count");
  const std::uint32_t columns = size.width / cell_width;  // 图集每行可容纳的字符数
  if (columns == 0 || std::uint64_t{count} > std::uint64_t{columns} * (size.height / cell_height)) {
    throw std::invalid_argument("Bitmap font atlas is too small for the requested glyph count");
  }  // 图集容纳不下全部字符

  std::vector<Glyph> glyphs;  // 每个字符的字形
  glyphs.reserve(count);
  const auto w = static_cast<float>(cell_width), h = static_cast<float>(cell_height);
  for (std::uint32_t glyph = 0; glyph < count; ++glyph) {
    glyphs.push_back({assets.textureRegion(texture, (glyph % columns) * cell_width, (glyph / columns) * cell_height, cell_width, cell_height), 0.0f, 0.0f, w, h, w, true});
  }
  return BitmapFont(texture, w, h, first, std::move(glyphs));
}

BitmapFont BitmapFont::fromTtf(Assets& assets, const std::filesystem::path& path, float pixel_size, std::string_view characters) {
  if (!std::isfinite(pixel_size) || pixel_size <= 0.0f || pixel_size > 1024.0f) throw std::invalid_argument("Invalid font pixel size");

  TtfLibrary library;
  const auto filename = path.u8string();
  std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)> font(TTF_OpenFont(reinterpret_cast<const char*>(filename.c_str()), pixel_size), TTF_CloseFont);  // SDL_ttf 默认 72 DPI，磅值等于像素值
  if (!font) throw std::runtime_error(std::string("Failed to open font: ") + SDL_GetError());

  std::vector<char32_t> codes;  // 需要烘焙的字符，去重并排序
  for (char32_t code = U' '; code <= U'~'; ++code) codes.push_back(code);
  for (std::size_t position = 0; position < characters.size();) {
    const char32_t code = decodeUtf8(characters, position);
    if (code >= U' ' && code != U'\x7F' && code != U'\uFFFD') codes.push_back(code);  // 跳过控制字符和非法序列
  }
  std::sort(codes.begin(), codes.end());
  codes.erase(std::unique(codes.begin(), codes.end()), codes.end());

  std::vector<RasterGlyph> raster;  // 字体中存在的字形
  raster.reserve(codes.size());
  int widest = 0;  // 最大水平步进
  for (const char32_t code : codes) {
    if (!TTF_FontHasGlyph(font.get(), code)) continue;
    auto& glyph = raster.emplace_back();
    glyph.code = code;
    if (!TTF_GetGlyphMetrics(font.get(), code, nullptr, nullptr, nullptr, nullptr, &glyph.advance)) glyph.advance = 0;
    widest = std::max(widest, glyph.advance);
    rasterize(font.get(), code, glyph);
  }

  std::vector<RasterGlyph*> order;  // 按高度降序排列的可见字形，提高货架利用率
  std::uint64_t area = 0;            // 含边距的总面积
  std::uint32_t max_width = 1;       // 最宽字形的宽度
  for (auto& glyph : raster) {
    if (glyph.width == 0) continue;
    order.push_back(&glyph);
    area += std::uint64_t(glyph.width + 1) * std::uint64_t(glyph.height + 1);
    max_width = std::max(max_width, static_cast<std::uint32_t>(glyph.width));
  }
  std::sort(order.begin(), order.end(), [](const RasterGlyph* a, const RasterGlyph* b) { return a->height > b->height; });

  std::uint32_t width = 64;  // 取不小于面积平方根的 2 的幂，使图集接近正方形
  while ((std::uint64_t{width} * width < area + area / 8 || width < max_width + 2) && width < kMaxAtlasSize) width *= 2;
  const std::uint32_t height = pack(order, width);
  if (width < max_width + 2 || height > kMaxAtlasSize) throw std::length_error("Too many glyphs for a single font atlas");

  std::vector<std::byte> rgba(std::size_t{width} * height * 4, std::byte{255});                                     // 默认全部为白色
  for (std::size_t pixel = 0; pixel < std::size_t{width} * height; ++pixel) rgba[pixel * 4 + 3] = std::byte{0};  // 默认全部透明
  for (const auto* glyph : order) {
    for (int y = 0; y < glyph->height; ++y) {
      std::memcpy(rgba.data() + ((std::size_t{glyph->atlas_y} + y) * width + glyph->atlas_x) * 4,
                  glyph->rgba.data() + static_cast<std::size_t>(y) * glyph->width * 4,
                  static_cast<std::size_t>(glyph->width) * 4);
    }
  }
  const auto texture = assets.createTexture(width, height, rgba);  // 注册字体图集纹理

  std::vector<Glyph> dense(kGlyphCount);           // 可打印 ASCII 字形表
  std::unordered_map<char32_t, Glyph> extended;  // 其余字形
  for (const auto& source : raster) {
    Glyph glyph{{static_cast<float>(source.atlas_x) / width,
                 static_cast<float>(source.atlas_y) / height,
                 static_cast<float>(source.atlas_x + source.width) / width,
                 static_cast<float>(source.atlas_y + source.height) / height},
                static_cast<float>(source.offset_x), static_cast<float>(source.offset_y),
                static_cast<float>(source.width), static_cast<float>(source.height),
                static_cast<float>(source.advance), true};
    if (source.code - U' ' < kGlyphCount) dense[source.code - U' '] = glyph;
    else extended.emplace(source.code, glyph);
  }

  int line = TTF_GetFontLineSkip(font.get());  // 行距
  if (line <= 0) line = TTF_GetFontHeight(font.get());
  return BitmapFont(texture, static_cast<float>(widest), static_cast<float>(line), U' ', std::move(dense), std::move(extended));
}

const BitmapFont::Glyph* BitmapFont::find(char32_t code) const noexcept {
  if (code >= first_ && code - first_ < glyphs_.size()) {
    const auto& glyph = glyphs_[code - first_];
    return glyph.present ? &glyph : nullptr;  // 连续表命中时不查哈希表
  }
  if (extended_.empty()) return nullptr;
  const auto found = extended_.find(code);
  return found != extended_.end() ? &found->second : nullptr;
}

const BitmapFont::Glyph* BitmapFont::glyph(char32_t code) const noexcept {
  if (const auto* found = find(code)) return found;
  return find(U'?');  // 不存在时用问号代替
}

void BitmapFont::draw(RenderFrame& frame, std::string_view text, float x, float y, float scale, Color color, CoordinateSpace space, std::int32_t layer) const {
  const float advance_y = glyph_height_ * scale;              // 每行的垂直步进
  frame.sprites.reserve(frame.sprites.size() + text.size());  // 预留空间，字节数是字符数的上限

  float cursor_x = x, cursor_y = y;  // 当前绘制位置
  for (std::size_t position = 0; position < text.size();) {
    const char32_t code = decodeUtf8(text, position);  // 解码下一个字符
    if (code == U'\n') {
      cursor_x = x;           // 换行时回到行首
      cursor_y += advance_y;  // 换行时下移一行
      continue;
    }
    const auto* glyph = this->glyph(code);
    if (!glyph) continue;
    if (glyph->width > 0.0f) {
      frame.drawSprite(texture_, {cursor_x + glyph->offset_x * scale, cursor_y + glyph->offset_y * scale, glyph->width * scale, glyph->height * scale}, glyph->uv, color, space, layer);
    }
    cursor_x += glyph->advance * scale;  // 前进到下一个字符位置
  }
}

Point2D BitmapFont::measure(std::string_view text, float scale) const noexcept {
  float line = 0.0f, widest = 0.0f;                 // 当前行宽度和最宽行宽度
  std::size_t lines = text.empty() ? 0 : 1;  // 行数
  for (std::size_t position = 0; position < text.size();) {
    const char32_t code = decodeUtf8(text, position);
    if (code == U'\n') {
      ++lines;
      line = 0.0f;
      continue;
    }
    if (const auto* glyph = this->glyph(code)) widest = std::max(widest, line += glyph->advance);
  }
  return {widest * scale, static_cast<float>(lines) * glyph_height_ * scale};
}
}  // namespace engine
