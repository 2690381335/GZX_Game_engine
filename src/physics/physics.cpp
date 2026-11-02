#include "../../include/engine/physics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace engine {
namespace {
constexpr std::uint32_t kMaxCellsPerBody = 64;  // 单个刚体最多占用的格子数，超过则作为大刚体逐一检测
constexpr float kCellLimit = 1.0e9f;            // 格子坐标的限制范围，防止极端坐标转换为整数时溢出

/**
 * @brief 检查浮点数是否为有限值。
 *
 * @param value 要检查的值。
 * @return true 值为有限数。
 * @return false 值为无穷大或 NaN。
 */
bool finite(float value) noexcept { return std::isfinite(value); }

/**
 * @brief 将世界坐标转换为格子坐标。
 *
 * @param value 世界坐标。
 * @param inverse_cell_size 格子尺寸的倒数。
 * @return std::int64_t 格子坐标。
 */
std::int64_t cellCoordinate(float value, float inverse_cell_size) noexcept {
  return static_cast<std::int64_t>(std::floor(std::clamp(value * inverse_cell_size, -kCellLimit, kCellLimit)));
}

/**
 * @brief 将二维格子坐标编码为 64 位键。
 *
 * @param x 格子 X 坐标。
 * @param y 格子 Y 坐标。
 * @return std::uint64_t 格子键。
 */
std::uint64_t cellKey(std::int64_t x, std::int64_t y) noexcept {
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(y);
}
}  // namespace

PhysicsWorld::PhysicsWorld(float gravity_x, float gravity_y, float cell_size)
    : gravity_x_(gravity_x), gravity_y_(gravity_y), cell_size_(cell_size), inverse_cell_size_(0.0f) {
  if (!finite(gravity_x) || !finite(gravity_y)) throw std::invalid_argument("Gravity must be finite");
  if (!finite(cell_size) || cell_size <= 0.0f) throw std::invalid_argument("Cell size must be positive");
  inverse_cell_size_ = 1.0f / cell_size;
}

BodyHandle PhysicsWorld::createBody(const BodyDefinition& definition) {
  const auto& d = definition;
  if (!finite(d.x) || !finite(d.y) || !finite(d.velocity_x) || !finite(d.velocity_y)) throw std::invalid_argument("Body position and velocity must be finite");
  if (d.shape == ShapeType::Box && (!finite(d.half_width) || !finite(d.half_height) || d.half_width <= 0.0f || d.half_height <= 0.0f)) throw std::invalid_argument("Box half size must be positive");
  if (d.shape == ShapeType::Circle && (!finite(d.radius) || d.radius <= 0.0f)) throw std::invalid_argument("Circle radius must be positive");
  if (d.type == BodyType::Dynamic && (!finite(d.mass) || d.mass <= 0.0f)) throw std::invalid_argument("Dynamic body mass must be positive");
  if (!finite(d.restitution) || !finite(d.friction) || !finite(d.linear_damping) || !finite(d.gravity_scale) || d.restitution < 0.0f || d.friction < 0.0f || d.linear_damping < 0.0f) {
    throw std::invalid_argument("Invalid body material parameters");
  }

  std::uint32_t index;  // 新刚体的槽位索引
  if (!free_.empty()) {
    index = free_.back();  // 优先复用空闲槽位
    free_.pop_back();
  } else {
    if (slots_ == std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Too many physics bodies");
    index = slots_;
    const std::size_t size = std::size_t{slots_} + 1;  // 所有数组同步扩容
    generation_.resize(size, 0);
    alive_.resize(size, 0);
    type_.resize(size);
    shape_.resize(size);
    sensor_.resize(size);
    x_.resize(size);
    y_.resize(size);
    vx_.resize(size);
    vy_.resize(size);
    half_width_.resize(size);
    half_height_.resize(size);
    inverse_mass_.resize(size);
    restitution_.resize(size);
    friction_.resize(size);
    damping_.resize(size);
    gravity_scale_.resize(size);
    category_.resize(size);
    mask_.resize(size);
    user_data_.resize(size);
    boxes_.resize(size);
    ++slots_;
  }

  const bool circle = d.shape == ShapeType::Circle;
  ++generation_[index];  // 槽位代数递增，使旧句柄失效
  alive_[index] = 1;
  type_[index] = d.type;
  shape_[index] = d.shape;
  sensor_[index] = d.sensor ? 1 : 0;
  x_[index] = d.x;
  y_[index] = d.y;
  vx_[index] = d.type == BodyType::Static ? 0.0f : d.velocity_x;
  vy_[index] = d.type == BodyType::Static ? 0.0f : d.velocity_y;
  half_width_[index] = circle ? d.radius : d.half_width;
  half_height_[index] = circle ? d.radius : d.half_height;
  inverse_mass_[index] = d.type == BodyType::Dynamic ? 1.0f / d.mass : 0.0f;
  restitution_[index] = d.restitution;
  friction_[index] = d.friction;
  damping_[index] = d.linear_damping;
  gravity_scale_[index] = d.gravity_scale;
  category_[index] = d.category;
  mask_[index] = d.mask;
  user_data_[index] = d.user_data;
  return {index, generation_[index]};
}

void PhysicsWorld::destroyBody(BodyHandle body) noexcept {
  if (!valid(body)) return;
  alive_[body.index] = 0;
  free_.push_back(body.index);  // 回收槽位，free_ 容量不会超过槽位总数
}

bool PhysicsWorld::valid(BodyHandle body) const noexcept {
  return body.index < slots_ && alive_[body.index] && generation_[body.index] == body.generation;
}

std::uint32_t PhysicsWorld::slot(BodyHandle body) const {
  if (!valid(body)) throw std::invalid_argument("Invalid physics body handle");
  return body.index;
}

Point2D PhysicsWorld::position(BodyHandle body) const {
  const auto index = slot(body);
  return {x_[index], y_[index]};
}

void PhysicsWorld::setPosition(BodyHandle body, Point2D position) {
  if (!finite(position.x) || !finite(position.y)) throw std::invalid_argument("Position must be finite");
  const auto index = slot(body);
  x_[index] = position.x;
  y_[index] = position.y;
}

Point2D PhysicsWorld::velocity(BodyHandle body) const {
  const auto index = slot(body);
  return {vx_[index], vy_[index]};
}

void PhysicsWorld::setVelocity(BodyHandle body, Point2D velocity) {
  if (!finite(velocity.x) || !finite(velocity.y)) throw std::invalid_argument("Velocity must be finite");
  const auto index = slot(body);
  if (type_[index] == BodyType::Static) return;  // 静态刚体始终保持静止
  vx_[index] = velocity.x;
  vy_[index] = velocity.y;
}

void PhysicsWorld::applyImpulse(BodyHandle body, float impulse_x, float impulse_y) {
  if (!finite(impulse_x) || !finite(impulse_y)) throw std::invalid_argument("Impulse must be finite");
  const auto index = slot(body);
  vx_[index] += impulse_x * inverse_mass_[index];  // 非动态刚体的质量倒数为 0，冲量自然无效
  vy_[index] += impulse_y * inverse_mass_[index];
}

std::uint64_t PhysicsWorld::userData(BodyHandle body) const { return user_data_[slot(body)]; }

void PhysicsWorld::setGravity(float gravity_x, float gravity_y) {
  if (!finite(gravity_x) || !finite(gravity_y)) throw std::invalid_argument("Gravity must be finite");
  gravity_x_ = gravity_x;
  gravity_y_ = gravity_y;
}

void PhysicsWorld::bounds(std::uint32_t index, float& left, float& top, float& right, float& bottom) const noexcept {
  left = x_[index] - half_width_[index];
  top = y_[index] - half_height_[index];
  right = x_[index] + half_width_[index];
  bottom = y_[index] + half_height_[index];
}

bool PhysicsWorld::collide(std::uint32_t a, std::uint32_t b, Manifold& manifold) const noexcept {
  const bool circle_a = shape_[a] == ShapeType::Circle;
  const bool circle_b = shape_[b] == ShapeType::Circle;
  const float dx = x_[b] - x_[a];  // 两个中心的水平距离
  const float dy = y_[b] - y_[a];  // 两个中心的垂直距离
  manifold.a = a;
  manifold.b = b;

  if (!circle_a && !circle_b) {  // 矩形与矩形：在穿透较小的轴上分离
    const float overlap_x = half_width_[a] + half_width_[b] - std::abs(dx);
    if (overlap_x <= 0.0f) return false;
    const float overlap_y = half_height_[a] + half_height_[b] - std::abs(dy);
    if (overlap_y <= 0.0f) return false;
    if (overlap_x < overlap_y) {
      manifold.normal_x = dx < 0.0f ? -1.0f : 1.0f;
      manifold.normal_y = 0.0f;
      manifold.depth = overlap_x;
    } else {
      manifold.normal_x = 0.0f;
      manifold.normal_y = dy < 0.0f ? -1.0f : 1.0f;
      manifold.depth = overlap_y;
    }
    return true;
  }

  if (circle_a && circle_b) {  // 圆形与圆形：沿中心连线分离
    const float radius = half_width_[a] + half_width_[b];
    const float distance_squared = dx * dx + dy * dy;
    if (distance_squared >= radius * radius) return false;
    const float distance = std::sqrt(distance_squared);
    if (distance > 0.0f) {
      manifold.normal_x = dx / distance;
      manifold.normal_y = dy / distance;
    } else {
      manifold.normal_x = 0.0f;  // 中心重合时任选一个方向
      manifold.normal_y = 1.0f;
    }
    manifold.depth = radius - distance;
    return true;
  }

  const std::uint32_t box = circle_a ? b : a;     // 矩形刚体
  const std::uint32_t circle = circle_a ? a : b;  // 圆形刚体
  const float radius = half_width_[circle];
  const float local_x = x_[circle] - x_[box];  // 圆心相对矩形中心的位置
  const float local_y = y_[circle] - y_[box];
  const float closest_x = std::clamp(local_x, -half_width_[box], half_width_[box]);  // 矩形上距圆心最近的点
  const float closest_y = std::clamp(local_y, -half_height_[box], half_height_[box]);

  float normal_x, normal_y, depth;  // 从矩形指向圆形的法线和穿透深度
  if (closest_x == local_x && closest_y == local_y) {  // 圆心在矩形内部：沿最近的边推出
    const float push_x = half_width_[box] - std::abs(local_x);
    const float push_y = half_height_[box] - std::abs(local_y);
    if (push_x < push_y) {
      normal_x = local_x < 0.0f ? -1.0f : 1.0f;
      normal_y = 0.0f;
      depth = push_x + radius;
    } else {
      normal_x = 0.0f;
      normal_y = local_y < 0.0f ? -1.0f : 1.0f;
      depth = push_y + radius;
    }
  } else {
    const float offset_x = local_x - closest_x;
    const float offset_y = local_y - closest_y;
    const float distance_squared = offset_x * offset_x + offset_y * offset_y;
    if (distance_squared >= radius * radius) return false;
    const float distance = std::sqrt(distance_squared);
    normal_x = offset_x / distance;  // 圆心在矩形外部时距离必然大于 0
    normal_y = offset_y / distance;
    depth = radius - distance;
  }

  const float sign = circle_a ? -1.0f : 1.0f;  // 法线需要从 a 指向 b
  manifold.normal_x = normal_x * sign;
  manifold.normal_y = normal_y * sign;
  manifold.depth = depth;
  return true;
}

void PhysicsWorld::findContacts() {
  manifolds_.clear();
  cells_.clear();
  large_.clear();

  for (std::uint32_t index = 0; index < slots_; ++index) {
    if (!alive_[index]) continue;
    auto& box = boxes_[index];
    bounds(index, box.left, box.top, box.right, box.bottom);  // 缓存本子步的包围盒

    const auto x0 = cellCoordinate(box.left, inverse_cell_size_), x1 = cellCoordinate(box.right, inverse_cell_size_);
    const auto y0 = cellCoordinate(box.top, inverse_cell_size_), y1 = cellCoordinate(box.bottom, inverse_cell_size_);
    if ((x1 - x0 + 1) * (y1 - y0 + 1) > kMaxCellsPerBody) {
      large_.push_back(index);  // 跨越格子过多的刚体不进入网格
      continue;
    }
    for (auto y = y0; y <= y1; ++y) {
      for (auto x = x0; x <= x1; ++x) cells_.push_back({cellKey(x, y), index});
    }
  }

  std::sort(cells_.begin(), cells_.end(), [](const CellEntry& left, const CellEntry& right) {
    return left.cell != right.cell ? left.cell < right.cell : left.body < right.body;
  });  // 按格子分组，同一格子内按刚体索引排序

  const auto test = [&](std::uint32_t a, std::uint32_t b) {
    const bool sensor = sensor_[a] || sensor_[b];
    if (type_[a] != BodyType::Dynamic && type_[b] != BodyType::Dynamic && (!sensor || (type_[a] == BodyType::Static && type_[b] == BodyType::Static))) return;  // 两个非动态刚体之间只关心触发器事件
    if (!(category_[a] & mask_[b]) || !(category_[b] & mask_[a])) return;  // 碰撞过滤
    const auto& box_a = boxes_[a];
    const auto& box_b = boxes_[b];
    if (box_a.right <= box_b.left || box_b.right <= box_a.left || box_a.bottom <= box_b.top || box_b.bottom <= box_a.top) return;  // 包围盒快速排除
    Manifold manifold;
    if (collide(a, b, manifold)) {
      manifold.sensor = sensor;
      manifolds_.push_back(manifold);
    }
  };

  for (std::size_t first = 0; first < cells_.size();) {
    std::size_t last = first + 1;
    while (last < cells_.size() && cells_[last].cell == cells_[first].cell) ++last;  // 找到同一格子的条目范围

    for (std::size_t i = first; i < last; ++i) {
      const auto a = cells_[i].body;
      for (std::size_t j = i + 1; j < last; ++j) {
        const auto b = cells_[j].body;
        const auto& box_a = boxes_[a];
        const auto& box_b = boxes_[b];
        const auto owner = cellKey(cellCoordinate(std::max(box_a.left, box_b.left), inverse_cell_size_), cellCoordinate(std::max(box_a.top, box_b.top), inverse_cell_size_));
        if (owner != cells_[first].cell) continue;  // 只在重叠区域左上角所在的格子中处理，避免同一对刚体被重复检测
        test(a, b);
      }
    }
    first = last;
  }

  for (std::size_t i = 0; i < large_.size(); ++i) {
    const auto a = large_[i];
    for (std::uint32_t b = 0; b < slots_; ++b) {
      if (!alive_[b] || b == a) continue;
      const bool other_large = std::find(large_.begin(), large_.end(), b) != large_.end();
      if (other_large && b < a) continue;  // 两个大刚体之间只检测一次
      test(std::min(a, b), std::max(a, b));
    }
  }
}

void PhysicsWorld::substep(float delta_time, int iterations) {
  for (std::uint32_t index = 0; index < slots_; ++index) {
    if (!alive_[index] || type_[index] != BodyType::Dynamic) continue;
    const float damping = 1.0f / (1.0f + delta_time * damping_[index]);  // 隐式阻尼，任意时间步长下都稳定
    vx_[index] = (vx_[index] + gravity_x_ * gravity_scale_[index] * delta_time) * damping;
    vy_[index] = (vy_[index] + gravity_y_ * gravity_scale_[index] * delta_time) * damping;
  }

  findContacts();

  const float threshold = 2.0f * std::hypot(gravity_x_, gravity_y_) * delta_time + 1.0e-3f;  // 低于该相对速度时不反弹，消除静止接触的抖动
  for (int iteration = 0; iteration < iterations; ++iteration) {
    for (const auto& manifold : manifolds_) {
      if (manifold.sensor) continue;
      const auto a = manifold.a, b = manifold.b;
      const float inverse_a = inverse_mass_[a], inverse_b = inverse_mass_[b];
      const float inverse_sum = inverse_a + inverse_b;
      if (inverse_sum <= 0.0f) continue;

      const float relative_x = vx_[b] - vx_[a];
      const float relative_y = vy_[b] - vy_[a];
      const float normal_speed = relative_x * manifold.normal_x + relative_y * manifold.normal_y;
      if (normal_speed >= 0.0f) continue;  // 已经在分离

      const float restitution = -normal_speed > threshold ? std::max(restitution_[a], restitution_[b]) : 0.0f;
      const float impulse = -(1.0f + restitution) * normal_speed / inverse_sum;  // 法向冲量
      vx_[a] -= impulse * manifold.normal_x * inverse_a;
      vy_[a] -= impulse * manifold.normal_y * inverse_a;
      vx_[b] += impulse * manifold.normal_x * inverse_b;
      vy_[b] += impulse * manifold.normal_y * inverse_b;

      const float tangent_x = -manifold.normal_y;  // 二维切线方向
      const float tangent_y = manifold.normal_x;
      const float tangent_speed = (vx_[b] - vx_[a]) * tangent_x + (vy_[b] - vy_[a]) * tangent_y;
      const float limit = impulse * std::sqrt(friction_[a] * friction_[b]);                // 库仑摩擦上限
      const float friction = std::clamp(-tangent_speed / inverse_sum, -limit, limit);  // 摩擦冲量
      vx_[a] -= friction * tangent_x * inverse_a;
      vy_[a] -= friction * tangent_y * inverse_a;
      vx_[b] += friction * tangent_x * inverse_b;
      vy_[b] += friction * tangent_y * inverse_b;
    }
  }

  for (std::uint32_t index = 0; index < slots_; ++index) {
    if (!alive_[index] || type_[index] == BodyType::Static) continue;
    x_[index] += vx_[index] * delta_time;  // 半隐式欧拉积分
    y_[index] += vy_[index] * delta_time;
  }

  const float slop = cell_size_ * 0.01f;  // 允许的穿透量，避免静止接触来回修正
  for (const auto& manifold : manifolds_) {
    if (manifold.sensor) continue;
    const auto a = manifold.a, b = manifold.b;
    const float inverse_sum = inverse_mass_[a] + inverse_mass_[b];
    if (inverse_sum <= 0.0f) continue;
    const float correction = std::max(manifold.depth - slop, 0.0f) * 0.8f / inverse_sum;  // 位置修正量
    x_[a] -= correction * manifold.normal_x * inverse_mass_[a];
    y_[a] -= correction * manifold.normal_y * inverse_mass_[a];
    x_[b] += correction * manifold.normal_x * inverse_mass_[b];
    y_[b] += correction * manifold.normal_y * inverse_mass_[b];
  }

  for (const auto& manifold : manifolds_) {
    contacts_.push_back({{manifold.a, generation_[manifold.a]}, {manifold.b, generation_[manifold.b]}, manifold.normal_x, manifold.normal_y, manifold.depth, manifold.sensor});
  }
}

void PhysicsWorld::step(float delta_time, int substeps, int iterations) {
  if (!finite(delta_time) || delta_time < 0.0f) throw std::invalid_argument("Invalid physics delta time");
  if (substeps < 1 || substeps > 64 || iterations < 1 || iterations > 64) throw std::invalid_argument("Invalid physics substeps or iterations");

  contacts_.clear();
  if (delta_time == 0.0f) return;
  const float sub = delta_time / static_cast<float>(substeps);
  for (int index = 0; index < substeps; ++index) substep(sub, iterations);

  if (substeps > 1) {
    std::stable_sort(contacts_.begin(), contacts_.end(), [](const Contact& left, const Contact& right) {
      return left.a.index != right.a.index ? left.a.index < right.a.index : left.b.index < right.b.index;
    });
    contacts_.erase(std::unique(contacts_.begin(), contacts_.end(), [](const Contact& left, const Contact& right) {
                      return left.a == right.a && left.b == right.b;
                    }),
                    contacts_.end());  // 多个子步中重复出现的刚体对只保留第一次的接触
  }
}

void PhysicsWorld::queryPoint(Point2D point, std::vector<BodyHandle>& out, std::uint32_t mask) const {
  out.clear();
  for (std::uint32_t index = 0; index < slots_; ++index) {
    if (!alive_[index] || !(category_[index] & mask)) continue;
    const float dx = point.x - x_[index], dy = point.y - y_[index];
    const bool hit = shape_[index] == ShapeType::Circle ? dx * dx + dy * dy <= half_width_[index] * half_width_[index]
                                                        : std::abs(dx) <= half_width_[index] && std::abs(dy) <= half_height_[index];
    if (hit) out.push_back({index, generation_[index]});
  }
}

void PhysicsWorld::queryRect(float left, float top, float right, float bottom, std::vector<BodyHandle>& out, std::uint32_t mask) const {
  out.clear();
  if (left > right) std::swap(left, right);
  if (top > bottom) std::swap(top, bottom);
  for (std::uint32_t index = 0; index < slots_; ++index) {
    if (!alive_[index] || !(category_[index] & mask)) continue;
    bool hit;
    if (shape_[index] == ShapeType::Circle) {
      const float dx = x_[index] - std::clamp(x_[index], left, right);  // 圆心到矩形的最近距离
      const float dy = y_[index] - std::clamp(y_[index], top, bottom);
      hit = dx * dx + dy * dy <= half_width_[index] * half_width_[index];
    } else {
      hit = x_[index] - half_width_[index] <= right && x_[index] + half_width_[index] >= left && y_[index] - half_height_[index] <= bottom && y_[index] + half_height_[index] >= top;
    }
    if (hit) out.push_back({index, generation_[index]});
  }
}

std::optional<RayHit> PhysicsWorld::raycast(Point2D from, Point2D to, std::uint32_t mask) const {
  const float dx = to.x - from.x, dy = to.y - from.y;  // 射线方向（未归一化）
  std::optional<RayHit> best;
  float best_t = 1.0f;  // 当前最近命中的比例

  for (std::uint32_t index = 0; index < slots_; ++index) {
    if (!alive_[index] || sensor_[index] || !(category_[index] & mask)) continue;
    float t, normal_x = 0.0f, normal_y = 0.0f;

    if (shape_[index] == ShapeType::Circle) {  // 解射线与圆的二次方程
      const float ox = from.x - x_[index], oy = from.y - y_[index];
      const float radius = half_width_[index];
      const float a = dx * dx + dy * dy;
      const float b = ox * dx + oy * dy;
      const float c = ox * ox + oy * oy - radius * radius;
      if (c <= 0.0f) {
        t = 0.0f;  // 起点在圆内
      } else {
        const float discriminant = b * b - a * c;
        if (a == 0.0f || discriminant < 0.0f) continue;
        t = (-b - std::sqrt(discriminant)) / a;
        if (t < 0.0f) continue;
      }
      if (t > best_t) continue;
      const float hit_x = ox + dx * t, hit_y = oy + dy * t;
      const float length = std::sqrt(hit_x * hit_x + hit_y * hit_y);
      if (length > 0.0f) {
        normal_x = hit_x / length;
        normal_y = hit_y / length;
      }
    } else {  // 射线与轴对齐矩形的平板法求交
      float near = 0.0f, far = 1.0f;
      int axis = -1;  // 进入矩形时穿过的轴，-1 表示起点在矩形内
      const float origin[2]{from.x, from.y}, direction[2]{dx, dy};
      const float minimum[2]{x_[index] - half_width_[index], y_[index] - half_height_[index]};
      const float maximum[2]{x_[index] + half_width_[index], y_[index] + half_height_[index]};
      bool missed = false;
      for (int k = 0; k < 2 && !missed; ++k) {
        if (direction[k] == 0.0f) {
          missed = origin[k] < minimum[k] || origin[k] > maximum[k];  // 平行于该轴且在平板外
          continue;
        }
        const float inverse = 1.0f / direction[k];
        float t0 = (minimum[k] - origin[k]) * inverse, t1 = (maximum[k] - origin[k]) * inverse;
        if (t0 > t1) std::swap(t0, t1);
        if (t0 > near) {
          near = t0;
          axis = k;
        }
        far = std::min(far, t1);
        missed = near > far;
      }
      if (missed) continue;
      t = near;
      if (t > best_t) continue;
      if (axis == 0) normal_x = dx > 0.0f ? -1.0f : 1.0f;
      if (axis == 1) normal_y = dy > 0.0f ? -1.0f : 1.0f;
    }

    best_t = t;
    best = RayHit{{index, generation_[index]}, from.x + dx * t, from.y + dy * t, normal_x, normal_y, t};
  }
  return best;
}
}  // namespace engine
