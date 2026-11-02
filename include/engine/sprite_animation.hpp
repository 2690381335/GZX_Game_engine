#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "render_frame.hpp"

namespace engine {
/**
 * @brief 表示一个精灵动画，管理帧更新和用于渲染的UV坐标。
 *
 */
class SpriteAnimation {
 public:
  /**
   * @brief 构造一个新的精灵动画对象。
   *
   * @param columns 精灵表的列数。
   * @param rows 精灵表的行数。
   * @param frame_count 精灵动画的总帧数。
   * @param fps 动画的每秒帧数。
   * @param loop 是否循环播放动画，默认为true。
   */
  SpriteAnimation(std::uint32_t columns, std::uint32_t rows, std::uint32_t frame_count, float fps, bool loop = true)
      : columns_(columns), rows_(rows), frame_count_(frame_count), fps_(fps), loop_(loop) {
    if (!columns || !rows || !frame_count || frame_count > std::uint64_t(columns) * rows || !std::isfinite(fps) || fps <= 0.0f) {
      throw std::invalid_argument("Invalid sprite animation parameters");
    }  // 如果参数无效则抛出异常，isfinite意思是检查fps是否为有限数
  }

  /**
   * @brief 更新精灵动画的状态，根据时间增量推进动画相位。
   *
   * @param delta_time 自上次更新以来的时间增量，单位为秒。
   */
  void update(float delta_time) {
    if (!std::isfinite(delta_time) || delta_time < 0.0f) throw std::invalid_argument("Invalid delta time");  // 检查delta_time是否为有限数且非负

    const double advanced = phase_ + static_cast<double>(delta_time) * fps_;                                     // 计算动画相位的前进量
    phase_ = loop_ ? std::fmod(advanced, frame_count_) : std::min(advanced, static_cast<double>(frame_count_));  // 更新动画相位，根据是否循环选择不同的计算方式
  }

  /**
   * @brief 检查精灵动画是否已经播放完毕。
   *
   * @return true 如果动画已经播放完毕（非循环且相位达到总帧数）。
   * @return false 如果动画尚未播放完毕或循环播放。
   */
  bool finished() const noexcept {
    return !loop_ && phase_ >= frame_count_;
  }

  /**
   * @brief 重置精灵动画的状态，将动画相位恢复到初始状态。
   * @note 该操作不会抛出异常。
   */
  void reset() noexcept { phase_ = 0.0; }  // 重置动画相位为初始状态

  /**
   * @brief 获取当前帧的UV矩形。
   *
   * @return UvRect 当前帧的UV矩形。
   */
  UvRect uv() const noexcept {
    const auto index = std::min(static_cast<std::uint32_t>(phase_), frame_count_ - 1);  // 当前帧的索引
    const auto column = index % columns_;                                               // 当前帧所在的列
    const auto row = index / columns_;                                                  // 当前帧所在的行
    const float column_scale = 1.0f / columns_;                                         // 每列的缩放比例
    const float row_scale = 1.0f / rows_;                                               // 每行的缩放比例
    return UvRect{
      column * column_scale,        // 当前帧的左边界
      row * row_scale,              // 当前帧的上边界
      (column + 1) * column_scale,  // 当前帧的右边界
      (row + 1) * row_scale         // 当前帧的下边界
    };  // 返回当前帧的UV矩形
  }

 private:
  std::uint32_t columns_, rows_, frame_count_;  // 精灵表中的列数、行数和总帧数
  float fps_;                                   // 动画的每秒帧数
  double phase_ = 0.0;                          // 当前动画的相位，用于计算帧切换
  bool loop_;                                   // 是否循环播放动画
};
}  // namespace engine