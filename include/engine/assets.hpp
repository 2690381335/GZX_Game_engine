#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <unordered_map>
#include <vector>

#include "render_frame.hpp"

namespace engine {
/**
 * @brief 管理图像资源及其相关纹理的加载、缓存和访问。
 * 
 */
class Assets {
 public:
  /**
   * @brief 表示图像的像素数据及其宽高信息。
   *
   */
  struct ImageData {
    std::uint32_t width = 0;      // 图像的宽度
    std::uint32_t height = 0;     // 图像的高度
    std::vector<std::byte> rgba;  // 图像的像素数据，按 RGBA 顺序存储
  };

  /**
   * @brief 表示纹理的尺寸信息，包括宽度和高度。
   *
   */
  struct TextureSize {
    std::uint32_t width = 0;   // 纹理的宽度
    std::uint32_t height = 0;  // 纹理的高度
  };

  /**
   * @brief 构造一个新的 Assets 对象。
   *
   */
  Assets();

  Assets(const Assets&) = delete;
  Assets& operator=(const Assets&) = delete;
  Assets(Assets&&) = delete;
  Assets& operator=(Assets&&) = delete;

  /**
   * @brief 加载指定路径的纹理，如果已经加载过则从缓存中返回。
   *
   * @param path 纹理文件的路径。
   * @return TextureHandle 加载的纹理句柄。
   */
  TextureHandle loadTexture(const std::filesystem::path& path);

  /**
   * @brief 创建一个新的纹理对象。
   *
   * @param width 纹理的宽度。
   * @param height 纹理的高度。
   * @param rgba 纹理的像素数据，按 RGBA 顺序存储。
   * @return TextureHandle 创建的纹理句柄。
   */
  TextureHandle createTexture(std::uint32_t width, std::uint32_t height, std::span<const std::byte> rgba);

  /**
   * @brief 获取指定纹理的尺寸信息，包括宽度和高度。
   *
   * @param texture 需要获取尺寸信息的纹理句柄。
   * @return TextureSize 返回纹理的尺寸信息，包括宽度和高度。
   */
  TextureSize textureSize(TextureHandle texture) const;

  /**
   * @brief 获取指定纹理的子区域的 UV 坐标。
   *
   * @param texture 需要获取子区域的纹理句柄。
   * @param x 子区域的左上角 X 坐标。
   * @param y 子区域的左上角 Y 坐标。
   * @param width 子区域的宽度。
   * @param height 子区域的高度。
   * @return UvRect 返回子区域的 UV 坐标。
   */
  UvRect textureRegion(TextureHandle texture, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) const;

  /**
   * @brief 获取所有已加载的图像数据。
   *
   * @return std::span<const ImageData> 包含所有已加载图像数据的只读视图。
   */
  std::span<const ImageData> images() const noexcept {
    return images_;
  }

  /**
   * @brief 释放从指定索引开始的所有图像的像素数据，以节省内存。
   *
   * @param first 开始释放的图像索引，默认为 0。
   */
  void releasePixels(std::size_t first = 0);

 private:
  std::vector<ImageData> images_;                                   // 存储所有已加载的图像数据。
  std::unordered_map<std::filesystem::path, TextureHandle> cache_;  // 缓存已加载的纹理，避免重复加载。
};
}  // namespace engine