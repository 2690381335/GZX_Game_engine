#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>

namespace engine {
/**
 * @brief 表示已加载声音资源的句柄。
 *
 */
struct SoundHandle {
  std::uint32_t index = std::numeric_limits<std::uint32_t>::max();  // 声音在声音池中的索引，默认值为无效索引
};

/**
 * @brief 表示正在播放的声音实例（声部）的句柄，代数用于识别已被回收的旧句柄。
 *
 */
struct VoiceHandle {
  std::uint32_t index = std::numeric_limits<std::uint32_t>::max();  // 声部索引，默认值为无效索引
  std::uint32_t generation = 0;                                     // 声部代数，声部每次被重新占用时递增
};

/**
 * @brief 基于 SDL3 音频流的软件混音器，所有声音在加载时统一转换为 48kHz 立体声浮点格式，混音线程内零分配。
 *
 */
class Audio {
 public:
  static constexpr std::uint32_t max_voices = 64;      // 同时播放的最大声部数量
  static constexpr std::uint32_t sample_rate = 48000;  // 混音器的采样率
  static constexpr std::uint32_t channels = 2;         // 混音器的声道数

  /**
   * @brief 构造音频混音器并打开默认播放设备，打开失败时混音器处于不可用状态但不会抛出异常。
   *
   */
  Audio();

  /**
   * @brief 关闭音频设备并释放所有声音资源。
   *
   */
  ~Audio();

  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;
  Audio(Audio&&) = delete;
  Audio& operator=(Audio&&) = delete;

  /**
   * @brief 检查音频设备是否可用。
   *
   * @return true 音频设备已成功打开。
   * @return false 没有可用的音频设备，播放操作将被忽略。
   */
  bool available() const noexcept;

  /**
   * @brief 通过 SDL_mixer 加载并完整预解码声音文件（WAV/OGG/MP3/FLAC 等），如果已经加载过则从缓存中返回。
   *
   * @param path 声音文件的路径。
   * @return SoundHandle 加载的声音句柄。
   */
  SoundHandle loadSound(const std::filesystem::path& path);

  /**
   * @brief 从内存中的浮点采样数据创建声音，适用于程序化生成的音效。
   *
   * @param samples 交错排列的浮点采样数据，取值范围为 -1.0 到 1.0。
   * @param source_channels 采样数据的声道数（1 或 2）。
   * @param source_rate 采样数据的采样率。
   * @return SoundHandle 创建的声音句柄。
   */
  SoundHandle createSound(std::span<const float> samples, std::uint32_t source_channels, std::uint32_t source_rate);

  /**
   * @brief 播放声音。
   *
   * @param sound 要播放的声音句柄。
   * @param volume 音量，取值范围为 0.0 到任意正数，1.0 为原始音量。
   * @param pan 声像，-1.0 为最左，0.0 为居中，1.0 为最右。
   * @param loop 是否循环播放。
   * @return VoiceHandle 播放的声部句柄，声部已满或设备不可用时返回无效句柄。
   */
  VoiceHandle play(SoundHandle sound, float volume = 1.0f, float pan = 0.0f, bool loop = false);

  /**
   * @brief 停止指定声部的播放。
   *
   * @param voice 要停止的声部句柄，句柄已失效时忽略。
   */
  void stop(VoiceHandle voice) noexcept;

  /**
   * @brief 停止所有声部的播放。
   *
   */
  void stopAll() noexcept;

  /**
   * @brief 检查声部是否仍在播放。
   *
   * @param voice 声部句柄。
   * @return true 声部仍在播放。
   * @return false 声部已播放完毕、被停止或句柄无效。
   */
  bool playing(VoiceHandle voice) const noexcept;

  /**
   * @brief 设置声部的音量。
   *
   * @param voice 声部句柄。
   * @param volume 新的音量。
   */
  void setVolume(VoiceHandle voice, float volume) noexcept;

  /**
   * @brief 设置声部的声像。
   *
   * @param voice 声部句柄。
   * @param pan 新的声像，-1.0 为最左，1.0 为最右。
   */
  void setPan(VoiceHandle voice, float pan) noexcept;

  /**
   * @brief 设置主音量，作用于所有声部。
   *
   * @param volume 新的主音量。
   */
  void setMasterVolume(float volume) noexcept;

 private:
  struct State;                   // 混音器内部状态，隐藏 SDL 依赖
  std::unique_ptr<State> state_;  // 指向混音器内部状态的指针
};
}  // namespace engine
