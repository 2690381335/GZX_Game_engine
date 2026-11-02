# 最低 CMake 版本：Windows 侧用 vcpkg + find_package(CONFIG)，3.25 足够
cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20) # 设置C++标准为C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 强制要求使用指定的C++标准
set(CMAKE_CXX_EXTENSIONS OFF) # 禁用编译器特有的扩展

# 定义项目，指定项目名称为mydemo，使用C++语言
# Linux 下可沿用原编译器：cmake -B build -DCMAKE_CXX_COMPILER=/home/system/LLVM-23.1.2-Linux-X64/bin/clang++
project(mydemo LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE) # 默认构建类型为Debug，除非用户另行指定，用户可以通过命令行或CMake GUI修改此值
endif() # 如果未指定构建类型，则默认设置为Debug

# 添加可执行文件，指定源文件为main.cpp
add_executable(main
    ${CMAKE_SOURCE_DIR}/examples/main.cpp
)

# 添加引擎库，指定源文件列表，着色器需手动编译：glslc -O -mfmt=num shaders/sprite.vert -o shaders/sprite.vert.inc（frag 同理）
add_library(engine SHARED
    ${CMAKE_SOURCE_DIR}/src/engine.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/window.cpp
    ${CMAKE_SOURCE_DIR}/src/assets/assets.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio.cpp
    ${CMAKE_SOURCE_DIR}/src/physics/physics.cpp
    ${CMAKE_SOURCE_DIR}/src/text/bitmap_font.cpp
    # 对外 C 接口，头文件为 include/gzx.h，供 Python、Lua、Java 等语言通过 FFI 调用
    ${CMAKE_SOURCE_DIR}/src/capi/gzx.cpp    ${CMAKE_SOURCE_DIR}/src/render/renderer.cpp
    # Vulkan 实例、设备与交换链
    ${CMAKE_SOURCE_DIR}/src/render/vulkan/vulkan_context.cpp
    ${CMAKE_SOURCE_DIR}/src/render/vulkan/device_selection.cpp
    ${CMAKE_SOURCE_DIR}/src/render/vulkan/vulkan_device.cpp
    ${CMAKE_SOURCE_DIR}/src/render/vulkan/swapchain.cpp
    # GPU 内存与缓冲区
    ${CMAKE_SOURCE_DIR}/src/render/memory/vma.cpp
    ${CMAKE_SOURCE_DIR}/src/render/memory/gpu_allocator.cpp
    ${CMAKE_SOURCE_DIR}/src/render/memory/staging_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/render/memory/upload_batch.cpp
    ${CMAKE_SOURCE_DIR}/src/render/memory/instance_buffer.cpp
    # 纹理、采样器与描述符
    ${CMAKE_SOURCE_DIR}/src/render/texture/texture_2d.cpp
    ${CMAKE_SOURCE_DIR}/src/render/texture/sampler.cpp
    ${CMAKE_SOURCE_DIR}/src/render/texture/texture_set_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/render/texture/texture_descriptors.cpp
    # 渲染通道、着色器与管线
    ${CMAKE_SOURCE_DIR}/src/render/pipeline/render_pass.cpp
    ${CMAKE_SOURCE_DIR}/src/render/pipeline/shader_module.cpp
    ${CMAKE_SOURCE_DIR}/src/render/pipeline/sprite_pipeline.cpp
    # 帧资源与帧绘制
    ${CMAKE_SOURCE_DIR}/src/render/frame/render_targets.cpp
    ${CMAKE_SOURCE_DIR}/src/render/frame/frame_resources.cpp
    ${CMAKE_SOURCE_DIR}/src/render/frame/draw_frame.cpp
)

if(WIN32)
    # ============================ Windows（vcpkg 依赖） ============================
    # 依赖由 vcpkg 提供，配置命令：
    #   cmake -B build -G Ninja
    #     -DCMAKE_CXX_COMPILER=<clang++路径>
    #     -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
    #     -DVCPKG_TARGET_TRIPLET=x64-windows
    find_package(SDL3 CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)
    find_package(SDL3_mixer CONFIG REQUIRED)
    find_package(SDL3_ttf CONFIG REQUIRED)
    find_package(Vulkan REQUIRED)  # Vulkan SDK（LunarG）不提供 CMake config，走 CMake 自带的 FindVulkan 模块，用 VULKAN_SDK 定位

    target_link_libraries(engine PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        Vulkan::Vulkan
    )

    # Windows DLL 默认不导出符号（Linux 的 .so 默认全导出），
    # 让 CMake 自动生成 .def 导出全部符号，保证 main.exe 与 FFI 调用都能链接到 engine.dll
    set_target_properties(engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

    # Windows 没有 RPATH 机制，依赖 DLL 统一拷贝到 exe 所在目录（见部署步骤）
else()
    # ============================ Linux（原 threelib 依赖） ============================
    # 指定包含目录，告诉编译器去哪里寻找头文件，SYSTEM表示这些目录下的头文件是系统头文件，
    # 编译器在处理警告时会有所区别，通常会减少这些头文件的警告信息。
    target_include_directories(engine SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3-3.4.16/include
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3_image-3.4.6/include
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3_mixer-3.2.4/include
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3_ttf-3.2.2/include
        ${CMAKE_SOURCE_DIR}/src/threelib/vulkan-1.4.357.1/include
    )

    # 指定链接目录，告诉链接器在链接阶段去哪里寻找库文件
    target_link_directories(engine PRIVATE
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3-3.4.16/build
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3_image-3.4.6/build
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3_mixer-3.2.4/build
        ${CMAKE_SOURCE_DIR}/src/threelib/SDL3_ttf-3.2.2/build
        ${CMAKE_SOURCE_DIR}/src/threelib/vulkan-1.4.357.1/a/VulkanLoader/lib
    )

    # 指定链接库，告诉链接器在链接阶段需要链接哪些库
    target_link_libraries(engine PRIVATE
        SDL3
        SDL3_image
        SDL3_mixer
        SDL3_ttf
        vulkan
    )

    # $ORIGIN代表程序自身所在目录，后面冒号分割多个搜索目录，运行时动态链接器按从左到右顺序查找共享库
    set_target_properties(main PROPERTIES
        BUILD_RPATH "$ORIGIN"   # BUILD_RPATH "$ORIGIN:$ORIGIN/lib:/opt/mysoft/lib"
    )
endif()

# 指定main可执行文件依赖engine库
target_link_libraries(main PRIVATE engine)

# ------------------------------ 编译选项 ------------------------------
# 公共警告选项（clang 在 Windows/MSVC ABI 与 Linux 下均可用）
set(PROJECT_WARN_FLAGS
    # 开启基础常用警告，捕获大部分常见代码错误（变量未初始化、类型隐式转换等）
    -Wall
    # 在-Wall基础上，开启更多额外警告，检测无符号/有符号对比、多余括号等隐患
    -Wextra
    # 严格遵循C++标准，禁止编译器扩展语法，保证代码标准可移植
    -Wpedantic
    # Clang专属：编译日志开启彩色文字，错误红色、警告黄色，阅读更直观
    $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
)

target_compile_options(main PRIVATE ${PROJECT_WARN_FLAGS})

if(WIN32)
    target_compile_options(engine PRIVATE ${PROJECT_WARN_FLAGS})
else()
    target_compile_options(engine PRIVATE
        ${PROJECT_WARN_FLAGS}
        # 生成位置无关代码，适用于共享库和静态库（仅 Linux 需要；Windows 的 MSVC ABI 无此概念）
        -fPIC
    )
endif()
