#include "../../include/engine/audio.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "SDL3/SDL.h"
#include "SDL3_mixer/SDL_mixer.h"

namespace engine {
namespace {
constexpr int kMixFrames = 1024;                 // 每次混音的帧数，混音缓冲区按此大小预分配
constexpr std::size_t kDecodeChunk = 64 * 1024;  // 每次从解码器读取的浮点采样数

/**
 * @brief 检查浮点数是否为有限的非负数。
 *
 * @param value 要检查的值。
 * @return true 值有效。
 * @return false 值为负数、无穷大或 NaN。
 */
bool validGain(float value) noexcept { return std::isfinite(value) && value >= 0.0f; }
}  // namespace

/**
 * @brief 混音器内部状态，所有成员在音频流锁的保护下访问。
 *
 */
struct Audio::State {
  /**
   * @brief 表示一个正在播放的声部。
   *
   */
  struct Voice {
    std::uint32_t sound = 0;       // 声部播放的声音索引
    std::uint32_t generation = 0;  // 声部代数，用于识别失效句柄
    std::size_t frame = 0;         // 当前播放到的帧位置
    float volume = 1.0f;           // 声部音量
    float pan = 0.0f;              // 声部声像
    bool loop = false;             // 是否循环播放
    bool active = false;           // 声部是否正在使用
  };

  SDL_AudioStream* stream = nullptr;                                    // SDL 音频流，回调在其锁内执行
  bool subsystem = false;                                               // 是否成功初始化了 SDL 音频子系统
  bool decoder = false;                                                 // 是否成功初始化了 SDL_mixer 解码器
  float master = 1.0f;                                                  // 主音量
  std::vector<std::vector<float>> sounds;                               // 所有声音的交错立体声浮点采样
  std::unordered_map<std::filesystem::path, SoundHandle> cache;         // 已加载声音文件的缓存
  std::array<Voice, max_voices> voices{};                               // 所有声部
  std::array<float, static_cast<std::size_t>(kMixFrames) * channels> mix{};  // 预分配的混音缓冲区，回调中零分配

  /**
   * @brief SDL 音频流回调，在音频线程中按需混音并提交数据。
   *
   * @param userdata 指向混音器状态的指针。
   * @param stream SDL 音频流。
   * @param additional_amount 本次需要补充的字节数。
   * @param total_amount 音频流总共需要的字节数（未使用）。
   */
  static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    (void)total_amount;
    auto& state = *static_cast<State*>(userdata);
    int frames = additional_amount / static_cast<int>(sizeof(float) * channels);  // 需要补充的帧数
    while (frames > 0) {
      const int count = std::min(frames, kMixFrames);  // 本轮混音的帧数
      state.render(static_cast<std::size_t>(count));
      SDL_PutAudioStreamData(stream, state.mix.data(), count * static_cast<int>(sizeof(float) * channels));  // 提交混音结果
      frames -= count;
    }
  }

  /**
   * @brief 将所有活动声部混音到混音缓冲区。
   *
   * @param frames 要混音的帧数。
   */
  void render(std::size_t frames) noexcept {
    float* out = mix.data();
    std::fill_n(out, frames * channels, 0.0f);  // 清空混音缓冲区

    for (auto& voice : voices) {
      if (!voice.active) continue;
      const auto& samples = sounds[voice.sound];       // 声部播放的采样数据
      const std::size_t length = samples.size() / channels;  // 声音的总帧数
      if (length == 0) {
        voice.active = false;
        continue;
      }

      const float gain = voice.volume * master;                               // 声部的总增益
      const float left = gain * (voice.pan > 0.0f ? 1.0f - voice.pan : 1.0f);   // 左声道增益
      const float right = gain * (voice.pan < 0.0f ? 1.0f + voice.pan : 1.0f);  // 右声道增益

      std::size_t written = 0;  // 已为该声部混音的帧数
      while (written < frames) {
        const std::size_t count = std::min(frames - written, length - voice.frame);  // 本段可连续混音的帧数
        const float* source = samples.data() + voice.frame * channels;
        float* target = out + written * channels;
        for (std::size_t index = 0; index < count; ++index) {
          target[index * 2] += source[index * 2] * left;           // 左声道累加，循环体可被编译器向量化
          target[index * 2 + 1] += source[index * 2 + 1] * right;  // 右声道累加
        }
        written += count;
        voice.frame += count;
        if (voice.frame < length) continue;
        if (!voice.loop) {
          voice.active = false;  // 非循环声音播放完毕，回收声部
          break;
        }
        voice.frame = 0;  // 循环声音回到开头
      }
    }

    for (std::size_t index = 0; index < frames * channels; ++index) out[index] = std::clamp(out[index], -1.0f, 1.0f);  // 限幅防止削波溢出
  }

  /**
   * @brief 查找句柄对应的活动声部。
   *
   * @param handle 声部句柄。
   * @return Voice* 声部指针，句柄失效时返回空指针。
   */
  Voice* find(VoiceHandle handle) noexcept {
    if (handle.index >= voices.size()) return nullptr;
    auto& voice = voices[handle.index];
    return voice.active && voice.generation == handle.generation ? &voice : nullptr;
  }

  /**
   * @brief 将任意格式的音频数据转换为混音器格式并加入声音池。
   *
   * @param spec 源数据格式。
   * @param data 源数据。
   * @param bytes 源数据字节数。
   * @return SoundHandle 新声音的句柄。
   */
  SoundHandle append(const SDL_AudioSpec& spec, const Uint8* data, int bytes) {
    const SDL_AudioSpec target{SDL_AUDIO_F32, static_cast<int>(channels), static_cast<int>(sample_rate)};  // 混音器格式
    Uint8* converted = nullptr;
    int converted_bytes = 0;
    if (!SDL_ConvertAudioSamples(&spec, data, bytes, &target, &converted, &converted_bytes)) {
      throw std::runtime_error(std::string("Failed to convert audio: ") + SDL_GetError());
    }

    std::vector<float> samples(static_cast<std::size_t>(converted_bytes) / sizeof(float));  // 转换后的浮点采样
    std::memcpy(samples.data(), converted, samples.size() * sizeof(float));
    SDL_free(converted);
    return store(std::move(samples));
  }

  /**
   * @brief 将已经是混音器格式的采样数据加入声音池。
   *
   * @param samples 交错立体声浮点采样。
   * @return SoundHandle 新声音的句柄。
   */
  SoundHandle store(std::vector<float> samples) {
    if (sounds.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Too many sounds");
    const SoundHandle handle{static_cast<std::uint32_t>(sounds.size())};
    Lock lock(stream);  // 回调可能正在读取声音池，扩容前必须加锁
    sounds.push_back(std::move(samples));
    return handle;
  }

  /**
   * @brief 音频流锁的 RAII 封装，音频流为空时不加锁。
   *
   */
  struct Lock {
    SDL_AudioStream* stream;
    explicit Lock(SDL_AudioStream* target) noexcept : stream(target) {
      if (stream) SDL_LockAudioStream(stream);
    }
    ~Lock() {
      if (stream) SDL_UnlockAudioStream(stream);
    }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
  };
};

Audio::Audio() : state_(std::make_unique<State>()) {
  state_->decoder = MIX_Init();  // 解码器不依赖音频设备，设备不可用时仍可加载声音
  if (!state_->decoder) SDL_Log("SDL_mixer unavailable: %s", SDL_GetError());

  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    SDL_Log("Audio unavailable: %s", SDL_GetError());  // 没有音频设备时仍允许游戏运行
    return;
  }
  state_->subsystem = true;

  const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(channels), static_cast<int>(sample_rate)};                  // 混音器输出格式
  state_->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &State::callback, state_.get());  // 打开默认播放设备
  if (!state_->stream) {
    SDL_Log("Failed to open audio device: %s", SDL_GetError());
    return;
  }
  if (!SDL_ResumeAudioStreamDevice(state_->stream)) SDL_Log("Failed to resume audio device: %s", SDL_GetError());  // 设备默认处于暂停状态
}

Audio::~Audio() {
  if (state_->stream) SDL_DestroyAudioStream(state_->stream);  // 销毁音频流，确保回调不再执行
  if (state_->subsystem) SDL_QuitSubSystem(SDL_INIT_AUDIO);    // 释放 SDL 音频子系统的引用
  if (state_->decoder) MIX_Quit();                             // 释放 SDL_mixer 的引用
}

bool Audio::available() const noexcept { return state_->stream != nullptr; }

SoundHandle Audio::loadSound(const std::filesystem::path& path) {
  const auto key = std::filesystem::canonical(path);  // 获取绝对规范路径以用作缓存键
  if (const auto found = state_->cache.find(key); found != state_->cache.end()) return found->second;

  if (!state_->decoder) throw std::runtime_error("SDL_mixer is not initialized");

  const auto filename = key.u8string();
  MIX_AudioDecoder* decoder = MIX_CreateAudioDecoder(reinterpret_cast<const char*>(filename.c_str()), 0);  // 按文件内容自动识别格式
  if (!decoder) throw std::runtime_error(std::string("Failed to load sound: ") + SDL_GetError());

  const SDL_AudioSpec target{SDL_AUDIO_F32, static_cast<int>(channels), static_cast<int>(sample_rate)};  // 解码器直接输出混音器格式，省去二次转换
  std::vector<float> samples;  // 解码后的交错立体声浮点采样
  try {
    for (;;) {
      const std::size_t used = samples.size();
      samples.resize(used + kDecodeChunk);
      const int bytes = MIX_DecodeAudio(decoder, samples.data() + used, static_cast<int>(kDecodeChunk * sizeof(float)), &target);
      if (bytes < 0) throw std::runtime_error(std::string("Failed to decode sound: ") + SDL_GetError());
      samples.resize(used + static_cast<std::size_t>(bytes) / sizeof(float));
      if (bytes == 0) break;  // 返回 0 表示整个文件已解码完毕
    }
  } catch (...) {
    MIX_DestroyAudioDecoder(decoder);
    throw;
  }
  MIX_DestroyAudioDecoder(decoder);

  samples.resize(samples.size() - samples.size() % channels);  // 丢弃不完整的尾帧
  if (samples.empty()) throw std::runtime_error("Sound file contains no audio");
  samples.shrink_to_fit();

  const SoundHandle handle = state_->store(std::move(samples));
  state_->cache.emplace(key, handle);  // 缓存失败时声音仍然可用，只是不会被复用
  return handle;
}

SoundHandle Audio::createSound(std::span<const float> samples, std::uint32_t source_channels, std::uint32_t source_rate) {
  if (source_channels == 0 || source_channels > 8 || source_rate == 0 || source_rate > 384000) throw std::invalid_argument("Invalid sound format");
  if (samples.empty() || samples.size() % source_channels != 0) throw std::invalid_argument("Sample count must be a non-zero multiple of channel count");
  if (samples.size_bytes() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::length_error("Sound data is too large");

  const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(source_channels), static_cast<int>(source_rate)};
  return state_->append(spec, reinterpret_cast<const Uint8*>(samples.data()), static_cast<int>(samples.size_bytes()));
}

VoiceHandle Audio::play(SoundHandle sound, float volume, float pan, bool loop) {
  if (!validGain(volume) || !std::isfinite(pan)) throw std::invalid_argument("Invalid volume or pan");
  if (!state_->stream) return {};  // 没有音频设备时忽略播放请求

  State::Lock lock(state_->stream);
  if (sound.index >= state_->sounds.size()) throw std::out_of_range("Sound handle is out of range");

  State::Voice* chosen = nullptr;  // 选中的声部
  for (auto& voice : state_->voices) {
    if (!voice.active) {
      chosen = &voice;  // 优先使用空闲声部
      break;
    }
    if (!voice.loop && (!chosen || voice.frame > chosen->frame)) chosen = &voice;  // 声部已满时抢占播放进度最多的非循环声部
  }
  if (!chosen) return {};  // 所有声部都在循环播放

  chosen->sound = sound.index;
  chosen->generation += 1;
  chosen->frame = 0;
  chosen->volume = volume;
  chosen->pan = std::clamp(pan, -1.0f, 1.0f);
  chosen->loop = loop;
  chosen->active = true;
  return {static_cast<std::uint32_t>(chosen - state_->voices.data()), chosen->generation};
}

void Audio::stop(VoiceHandle voice) noexcept {
  State::Lock lock(state_->stream);
  if (auto* found = state_->find(voice)) found->active = false;
}

void Audio::stopAll() noexcept {
  State::Lock lock(state_->stream);
  for (auto& voice : state_->voices) voice.active = false;
}

bool Audio::playing(VoiceHandle voice) const noexcept {
  State::Lock lock(state_->stream);
  return state_->find(voice) != nullptr;
}

void Audio::setVolume(VoiceHandle voice, float volume) noexcept {
  if (!validGain(volume)) return;
  State::Lock lock(state_->stream);
  if (auto* found = state_->find(voice)) found->volume = volume;
}

void Audio::setPan(VoiceHandle voice, float pan) noexcept {
  if (!std::isfinite(pan)) return;
  State::Lock lock(state_->stream);
  if (auto* found = state_->find(voice)) found->pan = std::clamp(pan, -1.0f, 1.0f);
}

void Audio::setMasterVolume(float volume) noexcept {
  if (!validGain(volume)) return;
  State::Lock lock(state_->stream);
  state_->master = volume;
}
}  // namespace engine
