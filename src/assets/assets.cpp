#include "../../include/engine/assets.hpp"

#include <SDL3_image/SDL_image.h>

#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "SDL3/SDL.h"

namespace engine {
Assets::Assets() {
  images_.push_back({1, 1, std::vector<std::byte>(4, std::byte{255})});
}

TextureHandle Assets::loadTexture(const std::filesystem::path& path) {
  const auto key = std::filesystem::canonical(path);                                                                               // 获取绝对规范路径以用作缓存键
  if (const auto found = cache_.find(key); found != cache_.end()) return found->second;                                            // 如果缓存中已有该纹理，则直接返回
  if (images_.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Exceeded maximum number of images.");  // 检查是否超过最大图像数量限制

  const auto filename = key.u8string();                                                               // 获取文件的 UTF-8 编码字符串表示
  using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;                     // 定义 SDL_Surface 的智能指针类型，自动管理资源释放
  SurfacePtr source(IMG_Load(reinterpret_cast<const char*>(filename.c_str())), &SDL_DestroySurface);  // 加载图像并使用智能指针管理其生命周期
  if (!source) throw std::runtime_error(std::string("Failed to load image: ") + SDL_GetError());      // 如果加载失败，抛出异常

  SurfacePtr surface(SDL_ConvertSurface(source.get(), SDL_PIXELFORMAT_RGBA32), &SDL_DestroySurface);    // 将图像转换为 RGBA32 格式并使用智能指针管理其生命周期
  if (!surface) throw std::runtime_error(std::string("Failed to convert surface: ") + SDL_GetError());  // 如果转换失败，抛出异常
  if (surface->w <= 0 || surface->h <= 0) throw std::runtime_error("Invalid surface dimensions.");      // 检查图像宽高是否有效

  const std::uint64_t pixels = std::uint64_t(surface->w) * std::uint64_t(surface->h);  // 计算图像的像素总数
  if (pixels > (512ull * 1024 * 1024) / 4 || pixels > std::numeric_limits<std::size_t>::max() / 4) {
    throw std::runtime_error("Surface is too large.");
  }  // 如果图像像素总数超过512MB或可分配内存限制，则抛出异常

  const std::size_t row_bytes = static_cast<std::size_t>(surface->w) * 4;  // 每行字节数，假设每个像素4字节 (RGBA32)
  if (surface->pitch < 0 || static_cast<std::size_t>(surface->pitch) < row_bytes) {
    throw std::runtime_error("Surface pitch is invalid.");
  }  // 检查每行字节数是否小于预期值

  ImageData image{static_cast<std::uint32_t>(surface->w), static_cast<std::uint32_t>(surface->h), {}};  // 初始化图像数据结构，包含宽、高和空的像素数据容器
  image.rgba.resize(static_cast<std::size_t>(pixels * 4));                                              // 每个像素4字节 (RGBA32)
  const bool locked = SDL_MUSTLOCK(surface.get());                                                      // 检查表面是否需要锁定以访问像素数据
  if (locked && !SDL_LockSurface(surface.get())) throw std::runtime_error("Failed to lock surface.");   // 如果需要锁定但锁定失败，抛出异常
  const auto* source_pixels = static_cast<const std::byte*>(surface->pixels);                           // 获取源像素数据指针
  for (std::size_t row = 0; row < image.height; ++row) {
    std::memcpy(image.rgba.data() + row * row_bytes, source_pixels + row * static_cast<std::size_t>(surface->pitch), row_bytes);  // 逐行复制像素数据
  }
  if (locked) SDL_UnlockSurface(surface.get());  // 如果之前锁定了表面，则解锁

  const TextureHandle handle{static_cast<std::uint32_t>(images_.size())};  // 使用当前图像数量作为纹理句柄
  images_.push_back(std::move(image));                                     // 将图像数据添加到图像列表中

  try {
    cache_.emplace(key, handle);  // 将纹理句柄添加到缓存中，关联对应的键
  } catch (...) {
    images_.pop_back();  // 如果发生异常，移除刚刚添加的图像数据，保持数据一致性
    throw;
  }
  return handle;
}

TextureHandle Assets::createTexture(std::uint32_t width, std::uint32_t height, std::span<const std::byte> rgba) {
  if (width == 0 || height == 0) throw std::runtime_error("Invalid texture dimensions.");  // 检查纹理宽高是否有效
  const std::uint64_t pixels = std::uint64_t(width) * height;                              // 计算纹理的像素总数
  if (pixels > (512ull * 1024 * 1024) / 4 || pixels > std::numeric_limits<std::size_t>::max() / 4) {
    throw std::runtime_error("Texture is too large.");  // 如果纹理的像素总数超过512MB或可分配内存限制，则抛出异常
  }

  const auto bytes = static_cast<std::size_t>(pixels) * 4;                                                  // 计算纹理的字节总数 (每个像素4字节)
  if (rgba.size() != bytes) throw std::runtime_error("RGBA data size does not match texture dimensions.");  // 检查提供的像素数据大小是否与纹理尺寸匹配

  if (images_.size() >= std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("Too many textures.");  // 如果图像列表的数量超过uint32_t的最大值，抛出异常
  }

  ImageData image(width, height, {});           // 初始化图像数据结构，包含宽、高和空的像素数据容器
  image.rgba.assign(rgba.begin(), rgba.end());  // 将提供的像素数据复制到图像的像素数据容器中

  const TextureHandle handle{static_cast<std::uint32_t>(images_.size())};  // 使用当前图像数量作为纹理句柄
  images_.push_back(std::move(image));                                     // 将图像数据添加到图像列表中
  return handle;                                                           // 返回纹理句柄
}

Assets::TextureSize Assets::textureSize(TextureHandle texture) const {
  if (texture.index >= images_.size()) throw std::out_of_range("Texture handle is out of range.");  // 如果纹理句柄超出图像列表范围，抛出异常
  const auto& image = images_[texture.index];                                                       // 获取对应纹理的图像数据
  return {image.width, image.height};                                                               // 返回纹理的宽度和高度信息
}

UvRect Assets::textureRegion(TextureHandle texture, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) const {
  const auto size = textureSize(texture);  // 获取纹理的尺寸信息

  if (width == 0 || height == 0) throw std::invalid_argument("Sub-region width and height must be non-zero.");  // 检查子区域的宽高是否有效

  if (x > size.width || y > size.height || width > size.width - x || height > size.height - y) {
    throw std::out_of_range("Sub-region is out of texture bounds.");  // 如果子区域超出纹理范围，抛出异常
  }

  const float texture_width = static_cast<float>(size.width);
  const float texture_height = static_cast<float>(size.height);

  return {
    static_cast<float>(x) / texture_width,
    static_cast<float>(y) / texture_height,
    static_cast<float>(x + width) / texture_width,
    static_cast<float>(y + height) / texture_height}; // 返回子区域的 UV 坐标
}

void Assets::releasePixels(std::size_t first) {
  if (first > images_.size()) throw std::out_of_range("First index is out of range.");  // 如果指定的起始索引超出图像列表范围，抛出异常

  for (std::size_t index = first; index < images_.size(); ++index) {
    std::vector<std::byte>{}.swap(images_[index].rgba);  // 释放图像的像素数据，释放内存，这个写法意思是用一个空的临时向量与原向量交换，从而清空原向量并释放其占用的内存
  }
}
}  // namespace engine