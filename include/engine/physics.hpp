#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "render_frame.hpp"

namespace engine {
/**
 * @brief 表示物理刚体的句柄，代数用于识别已被销毁并复用的旧句柄。
 *
 */
struct BodyHandle {
  std::uint32_t index = std::numeric_limits<std::uint32_t>::max();  // 刚体在物理世界中的槽位索引，默认值为无效索引
  std::uint32_t generation = 0;                                     // 刚体槽位的代数，槽位每次被复用时递增

  friend constexpr bool operator==(BodyHandle, BodyHandle) noexcept = default;
};

/**
 * @brief 刚体的运动类型。
 *
 */
enum class BodyType : std::uint8_t {
  Static,    // 静态刚体，不移动，质量无限大
  Dynamic,   // 动态刚体，受重力、力和碰撞影响
  Kinematic  // 运动学刚体，按速度移动，不受重力和碰撞影响，但会推动动态刚体
};

/**
 * @brief 刚体的碰撞形状类型。
 *
 */
enum class ShapeType : std::uint8_t {
  Box,    // 轴对齐矩形
  Circle  // 圆形
};

/**
 * @brief 创建刚体时使用的参数，坐标均为刚体中心点。
 *
 */
struct BodyDefinition {
  BodyType type = BodyType::Dynamic;     // 刚体的运动类型
  ShapeType shape = ShapeType::Box;      // 刚体的碰撞形状
  float x = 0.0f, y = 0.0f;              // 刚体中心的位置
  float half_width = 16.0f;              // 矩形的半宽
  float half_height = 16.0f;             // 矩形的半高
  float radius = 16.0f;                  // 圆形的半径
  float velocity_x = 0.0f;               // 初始水平速度
  float velocity_y = 0.0f;               // 初始垂直速度
  float mass = 1.0f;                     // 质量，仅对动态刚体有效
  float restitution = 0.0f;              // 弹性系数，0 表示完全非弹性，1 表示完全弹性
  float friction = 0.2f;                 // 摩擦系数
  float linear_damping = 0.0f;           // 线性阻尼，每秒衰减速度的比例
  float gravity_scale = 1.0f;            // 重力缩放系数
  std::uint32_t category = 1;            // 碰撞类别位
  std::uint32_t mask = 0xFFFFFFFFu;      // 碰撞掩码，与对方类别位按位与非零时才发生碰撞
  bool sensor = false;                   // 是否为触发器，触发器只产生接触事件而不产生碰撞响应
  std::uint64_t user_data = 0;           // 用户自定义数据，通常用于关联游戏对象
};

/**
 * @brief 表示一次步进中两个刚体之间的接触信息，法线从刚体 a 指向刚体 b。
 *
 */
struct Contact {
  BodyHandle a, b;                       // 发生接触的两个刚体
  float normal_x = 0.0f, normal_y = 0.0f;  // 接触法线
  float depth = 0.0f;                    // 穿透深度
  bool sensor = false;                   // 是否有一方为触发器
};

/**
 * @brief 表示射线检测的命中结果。
 *
 */
struct RayHit {
  BodyHandle body;                         // 被命中的刚体
  float x = 0.0f, y = 0.0f;                // 命中点坐标
  float normal_x = 0.0f, normal_y = 0.0f;  // 命中点的表面法线
  float fraction = 0.0f;                   // 命中点在射线上的比例，0 为起点，1 为终点
};

/**
 * @brief 二维物理世界，采用结构体数组（SoA）布局、均匀网格宽相位和冲量法求解，适合大量轴对齐矩形和圆形刚体。
 *
 */
class PhysicsWorld {
 public:
  /**
   * @brief 构造一个新的物理世界。
   *
   * @param gravity_x 水平重力加速度。
   * @param gravity_y 垂直重力加速度，默认向下（屏幕坐标 Y 轴向下）。
   * @param cell_size 宽相位网格的格子尺寸，建议设置为常见刚体尺寸的 1 到 2 倍。
   */
  explicit PhysicsWorld(float gravity_x = 0.0f, float gravity_y = 980.0f, float cell_size = 64.0f);

  /**
   * @brief 创建一个刚体。
   *
   * @param definition 刚体的创建参数。
   * @return BodyHandle 新刚体的句柄。
   */
  BodyHandle createBody(const BodyDefinition& definition);

  /**
   * @brief 销毁一个刚体，句柄失效时忽略。
   *
   * @param body 要销毁的刚体句柄。
   */
  void destroyBody(BodyHandle body) noexcept;

  /**
   * @brief 检查刚体句柄是否仍然有效。
   *
   * @param body 刚体句柄。
   * @return true 刚体存在。
   * @return false 刚体已被销毁或句柄无效。
   */
  bool valid(BodyHandle body) const noexcept;

  /**
   * @brief 获取刚体中心的位置。
   *
   * @param body 刚体句柄。
   * @return Point2D 刚体中心的位置。
   */
  Point2D position(BodyHandle body) const;

  /**
   * @brief 设置刚体中心的位置（瞬移）。
   *
   * @param body 刚体句柄。
   * @param position 新的位置。
   */
  void setPosition(BodyHandle body, Point2D position);

  /**
   * @brief 获取刚体的线速度。
   *
   * @param body 刚体句柄。
   * @return Point2D 刚体的线速度。
   */
  Point2D velocity(BodyHandle body) const;

  /**
   * @brief 设置刚体的线速度，对静态刚体无效。
   *
   * @param body 刚体句柄。
   * @param velocity 新的线速度。
   */
  void setVelocity(BodyHandle body, Point2D velocity);

  /**
   * @brief 对刚体施加冲量，立即改变其速度，仅对动态刚体有效。
   *
   * @param body 刚体句柄。
   * @param impulse_x 水平冲量。
   * @param impulse_y 垂直冲量。
   */
  void applyImpulse(BodyHandle body, float impulse_x, float impulse_y);

  /**
   * @brief 获取刚体的用户数据。
   *
   * @param body 刚体句柄。
   * @return std::uint64_t 用户数据。
   */
  std::uint64_t userData(BodyHandle body) const;

  /**
   * @brief 设置重力加速度。
   *
   * @param gravity_x 水平重力加速度。
   * @param gravity_y 垂直重力加速度。
   */
  void setGravity(float gravity_x, float gravity_y);

  /**
   * @brief 推进物理模拟。
   *
   * @param delta_time 时间增量（秒）。
   * @param substeps 子步数，子步越多越稳定，默认为 1。
   * @param iterations 每个子步的速度求解迭代次数，默认为 4。
   */
  void step(float delta_time, int substeps = 1, int iterations = 4);

  /**
   * @brief 获取最近一次步进中产生的所有接触，每对刚体只记录一次。
   *
   * @return std::span<const Contact> 接触列表的只读视图。
   */
  std::span<const Contact> contacts() const noexcept { return contacts_; }

  /**
   * @brief 查询包含指定点的所有刚体。
   *
   * @param point 查询点。
   * @param out 用于接收结果的容器，函数会先清空它。
   * @param mask 碰撞类别掩码，只返回类别与掩码相交的刚体。
   */
  void queryPoint(Point2D point, std::vector<BodyHandle>& out, std::uint32_t mask = 0xFFFFFFFFu) const;

  /**
   * @brief 查询与指定矩形区域重叠的所有刚体。
   *
   * @param left 区域左边界。
   * @param top 区域上边界。
   * @param right 区域右边界。
   * @param bottom 区域下边界。
   * @param out 用于接收结果的容器，函数会先清空它。
   * @param mask 碰撞类别掩码。
   */
  void queryRect(float left, float top, float right, float bottom, std::vector<BodyHandle>& out, std::uint32_t mask = 0xFFFFFFFFu) const;

  /**
   * @brief 从起点向终点发射射线，返回最近的命中结果，触发器不会被命中。
   *
   * @param from 射线起点。
   * @param to 射线终点。
   * @param mask 碰撞类别掩码。
   * @return std::optional<RayHit> 最近的命中结果，未命中时为空。
   */
  std::optional<RayHit> raycast(Point2D from, Point2D to, std::uint32_t mask = 0xFFFFFFFFu) const;

  /**
   * @brief 获取当前存活的刚体数量。
   *
   * @return std::size_t 刚体数量。
   */
  std::size_t bodyCount() const noexcept { return slots_ - free_.size(); }

 private:
  /**
   * @brief 刚体在宽相位网格中的一个格子条目。
   *
   */
  struct CellEntry {
    std::uint64_t cell = 0;   // 格子坐标编码
    std::uint32_t body = 0;   // 刚体槽位索引
  };

  /**
   * @brief 窄相位得到的碰撞流形，供求解器使用。
   *
   */
  struct Manifold {
    std::uint32_t a = 0, b = 0;              // 两个刚体的槽位索引
    float normal_x = 0.0f, normal_y = 0.0f;  // 碰撞法线
    float depth = 0.0f;                      // 穿透深度
    bool sensor = false;                     // 是否有一方为触发器
  };

  /**
   * @brief 刚体的轴对齐包围盒，每个子步缓存一次。
   *
   */
  struct Aabb {
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;  // 包围盒边界
  };

  /**
   * @brief 获取句柄对应的槽位索引，句柄无效时抛出异常。
   *
   * @param body 刚体句柄。
   * @return std::uint32_t 槽位索引。
   */
  std::uint32_t slot(BodyHandle body) const;

  /**
   * @brief 计算刚体的轴对齐包围盒。
   *
   * @param index 槽位索引。
   * @param left 输出左边界。
   * @param top 输出上边界。
   * @param right 输出右边界。
   * @param bottom 输出下边界。
   */
  void bounds(std::uint32_t index, float& left, float& top, float& right, float& bottom) const noexcept;

  /**
   * @brief 检测两个刚体是否重叠并计算碰撞信息。
   *
   * @param a 第一个刚体的槽位索引。
   * @param b 第二个刚体的槽位索引。
   * @param manifold 输出碰撞信息。
   * @return true 两个刚体重叠。
   * @return false 两个刚体不重叠。
   */
  bool collide(std::uint32_t a, std::uint32_t b, Manifold& manifold) const noexcept;

  /**
   * @brief 执行一个子步的模拟。
   *
   * @param delta_time 子步的时间增量。
   * @param iterations 速度求解迭代次数。
   */
  void substep(float delta_time, int iterations);

  /**
   * @brief 使用均匀网格宽相位查找所有可能碰撞的刚体对，并进行窄相位检测。
   *
   */
  void findContacts();

 private:
  float gravity_x_, gravity_y_;  // 重力加速度
  float cell_size_;              // 宽相位网格的格子尺寸
  float inverse_cell_size_;      // 格子尺寸的倒数，用乘法替代除法

  std::uint32_t slots_ = 0;                  // 已分配的槽位数量
  std::vector<std::uint32_t> free_;          // 空闲槽位栈
  std::vector<std::uint32_t> generation_;    // 每个槽位的代数
  std::vector<std::uint8_t> alive_;          // 槽位是否存活
  std::vector<BodyType> type_;               // 刚体运动类型
  std::vector<ShapeType> shape_;             // 刚体碰撞形状
  std::vector<std::uint8_t> sensor_;         // 是否为触发器
  std::vector<float> x_, y_;                 // 刚体中心位置
  std::vector<float> vx_, vy_;               // 刚体线速度
  std::vector<float> half_width_, half_height_;  // 矩形半尺寸（圆形时为半径）
  std::vector<float> inverse_mass_;          // 质量的倒数，静态和运动学刚体为 0
  std::vector<float> restitution_;           // 弹性系数
  std::vector<float> friction_;              // 摩擦系数
  std::vector<float> damping_;               // 线性阻尼
  std::vector<float> gravity_scale_;         // 重力缩放系数
  std::vector<std::uint32_t> category_;      // 碰撞类别位
  std::vector<std::uint32_t> mask_;          // 碰撞掩码
  std::vector<std::uint64_t> user_data_;     // 用户数据

  std::vector<CellEntry> cells_;      // 宽相位格子条目，每个子步复用容量
  std::vector<std::uint32_t> large_;  // 跨越格子过多、改为逐一检测的大刚体
  std::vector<Aabb> boxes_;           // 每个槽位的包围盒缓存
  std::vector<Manifold> manifolds_;   // 当前子步的碰撞流形，每个子步复用容量
  std::vector<Contact> contacts_;     // 最近一次步进的接触列表
};
}  // namespace engine
