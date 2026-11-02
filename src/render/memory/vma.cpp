#if defined(__clang__)                                           // 如果使用的是 Clang 编译器
#pragma clang diagnostic push                                    // 保存当前的诊断状态，以便后续恢复
#pragma clang diagnostic ignored "-Wnullability-extension"       // 忽略 Clang 的空指针可空性扩展警告
#pragma clang diagnostic ignored "-Wnullability-completeness"    // 忽略 Clang 的空指针可空性完整性警告
#pragma clang diagnostic ignored "-Wunused-private-field"        // 忽略 Clang 的未使用私有字段警告
#pragma clang diagnostic ignored "-Wunused-parameter"            // 忽略 Clang 的未知参数警告
#pragma clang diagnostic ignored "-Wmissing-field-initializers"  // 忽略 Clang 的缺失字段初始化警告
#pragma clang diagnostic ignored "-Wunused-variable"             // 忽略 Clang 的未使用变量警告
#endif

// 1.`#define VMA_IMPLEMENTATION` → 预处理器记下：这个宏存在  2.`#include` 粘贴 vma.h 进来 3.读到 `#ifdef VMA_IMPLEMENTATION`，判断**宏已经存在**，把实现代码一并引入。
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"  // Windows / Linux 的 Vulkan SDK 布局一致：Include/vma/vk_mem_alloc.h

/**
 * @brief a.cpp include xxx.hpp 和 b.cpp include xxx.hpp，a.cpp 和 b.cpp 都会包含 xxx.hpp全部代码，可能导致重复定义问题。
 * 解决方法是在一个.cpp文件中定义 VMA_IMPLEMENTATION，而其他.cpp文件只包含头文件，不定义 VMA_IMPLEMENTATION；或者static inline定义函数，避免重复定义（类的成员函数默认是inline的）。
 * 变量也可能导致重复定义问题，解决方法同上。
 */

#if defined(__clang__)        // 如果使用的是 Clang 编译器
#pragma clang diagnostic pop  // 恢复之前保存的诊断状态
#endif