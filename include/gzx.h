#ifndef GZX_H
#define GZX_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define GZX_API __declspec(dllexport)
#else
#define GZX_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* 基础类型                                                                   */
/* ------------------------------------------------------------------------- */

#define GZX_INVALID_ID UINT32_MAX      /* 纹理、声音加载失败时的返回值 */
#define GZX_INVALID_HANDLE UINT64_MAX  /* 声部、刚体创建失败时的返回值 */
#define GZX_WHITE_TEXTURE 0u           /* 内置 1x1 白色纹理，用于绘制纯色矩形 */

typedef struct GzxEngine GzxEngine;  /* 引擎实例，一个实例对应一个窗口 */
typedef struct GzxFont GzxFont;      /* 位图字体 */
typedef struct GzxWorld GzxWorld;    /* 二维物理世界，可独立于引擎使用 */

/**
 * @brief 坐标空间。
 *
 */
enum {
  GZX_SPACE_WORLD = 0,  /* 世界坐标，受相机影响 */
  GZX_SPACE_SCREEN = 1  /* 屏幕坐标，不受相机影响，通常用于界面 */
};

/**
 * @brief 精灵排序模式。
 *
 */
enum {
  GZX_SORT_NONE = 0,          /* 不排序，严格按提交顺序绘制 */
  GZX_SORT_LAYER = 1,         /* 按层级稳定排序 */
  GZX_SORT_LAYER_TEXTURE = 2  /* 按层级再按纹理排序，合批效率最高 */
};

/**
 * @brief 键盘按键。
 *
 */
enum {
  GZX_KEY_LEFT, GZX_KEY_RIGHT, GZX_KEY_UP, GZX_KEY_DOWN,
  GZX_KEY_SPACE, GZX_KEY_ESCAPE, GZX_KEY_ENTER, GZX_KEY_TAB, GZX_KEY_BACKSPACE,
  GZX_KEY_LEFT_SHIFT, GZX_KEY_RIGHT_SHIFT, GZX_KEY_LEFT_CTRL, GZX_KEY_RIGHT_CTRL, GZX_KEY_LEFT_ALT, GZX_KEY_RIGHT_ALT,
  GZX_KEY_A, GZX_KEY_B, GZX_KEY_C, GZX_KEY_D, GZX_KEY_E, GZX_KEY_F, GZX_KEY_G, GZX_KEY_H, GZX_KEY_I,
  GZX_KEY_J, GZX_KEY_K, GZX_KEY_L, GZX_KEY_M, GZX_KEY_N, GZX_KEY_O, GZX_KEY_P, GZX_KEY_Q, GZX_KEY_R,
  GZX_KEY_S, GZX_KEY_T, GZX_KEY_U, GZX_KEY_V, GZX_KEY_W, GZX_KEY_X, GZX_KEY_Y, GZX_KEY_Z,
  GZX_KEY_0, GZX_KEY_1, GZX_KEY_2, GZX_KEY_3, GZX_KEY_4, GZX_KEY_5, GZX_KEY_6, GZX_KEY_7, GZX_KEY_8, GZX_KEY_9,
  GZX_KEY_F1, GZX_KEY_F2, GZX_KEY_F3, GZX_KEY_F4, GZX_KEY_F5, GZX_KEY_F6,
  GZX_KEY_F7, GZX_KEY_F8, GZX_KEY_F9, GZX_KEY_F10, GZX_KEY_F11, GZX_KEY_F12,
  GZX_KEY_COUNT
};

/**
 * @brief 鼠标按键。
 *
 */
enum {
  GZX_MOUSE_LEFT = 0,
  GZX_MOUSE_MIDDLE = 1,
  GZX_MOUSE_RIGHT = 2,
  GZX_MOUSE_X1 = 3,
  GZX_MOUSE_X2 = 4
};

/**
 * @brief 刚体运动类型。
 *
 */
enum {
  GZX_BODY_STATIC = 0,    /* 静态刚体，不移动 */
  GZX_BODY_DYNAMIC = 1,   /* 动态刚体，受重力、力和碰撞影响 */
  GZX_BODY_KINEMATIC = 2  /* 运动学刚体，按速度移动并推动动态刚体 */
};

/**
 * @brief 刚体碰撞形状。
 *
 */
enum {
  GZX_SHAPE_BOX = 0,    /* 轴对齐矩形 */
  GZX_SHAPE_CIRCLE = 1  /* 圆形 */
};

/**
 * @brief RGBA 颜色，各分量取值 0 到 1。
 *
 */
typedef struct GzxColor {
  float r, g, b, a;
} GzxColor;

/**
 * @brief 纹理 UV 区域，取值 0 到 1，左右交换可实现水平翻转。
 *
 */
typedef struct GzxUv {
  float left, top, right, bottom;
} GzxUv;

/**
 * @brief 精灵，内存布局固定为 72 字节，可整块批量提交。
 *
 */
typedef struct GzxSprite {
  uint32_t texture;               /* 纹理 ID */
  float x, y, width, height;      /* 左上角位置和尺寸 */
  float r, g, b, a;               /* 着色颜色 */
  float u0, v0, u1, v1;           /* UV 区域 */
  int32_t space;                  /* 坐标空间，GZX_SPACE_* */
  float rotation;                 /* 旋转角度（弧度） */
  float pivot_x, pivot_y;         /* 旋转中心，相对尺寸的比例，0.5 为中心 */
  int32_t layer;                  /* 层级，越大越靠上 */
} GzxSprite;

/**
 * @brief 引擎创建参数，使用前先调用 gzx_config_init 填充默认值。
 *
 */
typedef struct GzxConfig {
  const char* title;             /* 窗口标题（UTF-8） */
  int32_t width;                 /* 窗口宽度 */
  int32_t height;                /* 窗口高度 */
  int32_t resizable;             /* 窗口是否可调整大小 */
  int32_t pause_when_minimized;  /* 窗口最小化时 gzx_begin_frame 是否阻塞等待 */
} GzxConfig;

/**
 * @brief 刚体定义，使用前先调用 gzx_body_def_init 填充默认值。
 *
 */
typedef struct GzxBodyDef {
  int32_t type;               /* 运动类型，GZX_BODY_* */
  int32_t shape;              /* 碰撞形状，GZX_SHAPE_* */
  float x, y;                 /* 中心位置 */
  float half_width;           /* 矩形半宽 */
  float half_height;          /* 矩形半高 */
  float radius;               /* 圆形半径 */
  float velocity_x;           /* 初始水平速度 */
  float velocity_y;           /* 初始垂直速度 */
  float mass;                 /* 质量，仅动态刚体有效 */
  float restitution;          /* 弹性系数，0 到 1 */
  float friction;             /* 摩擦系数 */
  float linear_damping;       /* 线性阻尼 */
  float gravity_scale;        /* 重力缩放 */
  uint32_t category;          /* 碰撞类别位 */
  uint32_t mask;              /* 碰撞掩码 */
  int32_t sensor;             /* 是否为触发器，触发器只产生接触事件 */
  uint64_t user_data;         /* 用户数据 */
} GzxBodyDef;

/**
 * @brief 物理接触信息。
 *
 */
typedef struct GzxContact {
  uint64_t a, b;                /* 发生接触的两个刚体 */
  float normal_x, normal_y;     /* 接触法线 */
  float depth;                  /* 穿透深度 */
  int32_t sensor;               /* 是否有一方为触发器 */
} GzxContact;

/**
 * @brief 射线检测结果。
 *
 */
typedef struct GzxRayHit {
  uint64_t body;                /* 被命中的刚体 */
  float x, y;                   /* 命中点 */
  float normal_x, normal_y;     /* 命中点表面法线 */
  float fraction;               /* 命中点在射线上的比例，0 为起点，1 为终点 */
} GzxRayHit;

/* ------------------------------------------------------------------------- */
/* 错误                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief 获取当前线程最近一次失败调用的错误信息。
 *
 * @return const char* UTF-8 错误信息，没有错误时为空字符串，下次失败调用前有效。
 */
GZX_API const char* gzx_last_error(void);

/* ------------------------------------------------------------------------- */
/* 引擎与主循环                                                               */
/* ------------------------------------------------------------------------- */

/**
 * @brief 用默认值填充引擎创建参数。
 *
 * @param config 要填充的参数。
 */
GZX_API void gzx_config_init(GzxConfig* config);

/**
 * @brief 创建窗口、渲染器和音频混音器，必须在主线程调用。
 *
 * @param config 创建参数，为 NULL 时使用默认值。
 * @return GzxEngine* 引擎实例，失败时为 NULL。
 */
GZX_API GzxEngine* gzx_create(const GzxConfig* config);

/**
 * @brief 销毁引擎及其拥有的所有资源。
 *
 * @param engine 引擎实例，可以为 NULL。
 */
GZX_API void gzx_destroy(GzxEngine* engine);

/**
 * @brief 开始新的一帧：处理窗口事件、更新输入、计算时间增量并清空上一帧的绘制内容。
 *
 * @param engine 引擎实例。
 * @return int 1 表示继续运行，0 表示窗口已关闭或已请求退出。
 */
GZX_API int gzx_begin_frame(GzxEngine* engine);

/**
 * @brief 结束当前帧：上传新纹理并渲染本帧提交的所有内容。
 *
 * @param engine 引擎实例。
 */
GZX_API void gzx_end_frame(GzxEngine* engine);

/**
 * @brief 获取本帧与上一帧之间的时间（秒），最大为 0.25 秒。
 *
 * @param engine 引擎实例。
 * @return float 时间增量。
 */
GZX_API float gzx_delta_time(const GzxEngine* engine);

/**
 * @brief 请求退出，下次 gzx_begin_frame 返回 0。
 *
 * @param engine 引擎实例。
 */
GZX_API void gzx_request_quit(GzxEngine* engine);

/**
 * @brief 获取视口尺寸（像素）。
 *
 * @param engine 引擎实例。
 * @param width 输出宽度，可以为 NULL。
 * @param height 输出高度，可以为 NULL。
 */
GZX_API void gzx_viewport(const GzxEngine* engine, int32_t* width, int32_t* height);

/* ------------------------------------------------------------------------- */
/* 输入                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief 按键当前是否处于按下状态。
 *
 * @param engine 引擎实例。
 * @param key 按键，GZX_KEY_*。
 * @return int 1 为按下，0 为未按下。
 */
GZX_API int gzx_key_down(const GzxEngine* engine, int32_t key);

/**
 * @brief 按键是否在本帧被按下。
 *
 * @param engine 引擎实例。
 * @param key 按键，GZX_KEY_*。
 * @return int 1 为本帧按下，否则为 0。
 */
GZX_API int gzx_key_pressed(const GzxEngine* engine, int32_t key);

/**
 * @brief 按键是否在本帧被释放。
 *
 * @param engine 引擎实例。
 * @param key 按键，GZX_KEY_*。
 * @return int 1 为本帧释放，否则为 0。
 */
GZX_API int gzx_key_released(const GzxEngine* engine, int32_t key);

/**
 * @brief 鼠标按键当前是否处于按下状态。
 *
 * @param engine 引擎实例。
 * @param button 鼠标按键，GZX_MOUSE_*。
 * @return int 1 为按下，0 为未按下。
 */
GZX_API int gzx_mouse_down(const GzxEngine* engine, int32_t button);

/**
 * @brief 鼠标按键是否在本帧被按下。
 *
 * @param engine 引擎实例。
 * @param button 鼠标按键，GZX_MOUSE_*。
 * @return int 1 为本帧按下，否则为 0。
 */
GZX_API int gzx_mouse_pressed(const GzxEngine* engine, int32_t button);

/**
 * @brief 鼠标按键是否在本帧被释放。
 *
 * @param engine 引擎实例。
 * @param button 鼠标按键，GZX_MOUSE_*。
 * @return int 1 为本帧释放，否则为 0。
 */
GZX_API int gzx_mouse_released(const GzxEngine* engine, int32_t button);

/**
 * @brief 获取鼠标在窗口中的位置（像素）。
 *
 * @param engine 引擎实例。
 * @param x 输出横坐标，可以为 NULL。
 * @param y 输出纵坐标，可以为 NULL。
 */
GZX_API void gzx_mouse_position(const GzxEngine* engine, float* x, float* y);

/**
 * @brief 获取本帧鼠标滚轮的滚动量。
 *
 * @param engine 引擎实例。
 * @param x 输出水平滚动量，可以为 NULL。
 * @param y 输出垂直滚动量，可以为 NULL。
 */
GZX_API void gzx_mouse_wheel(const GzxEngine* engine, float* x, float* y);

/* ------------------------------------------------------------------------- */
/* 纹理                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief 加载图片纹理，重复加载同一路径返回同一 ID。
 *
 * @param engine 引擎实例。
 * @param path 图片路径（UTF-8）。
 * @return uint32_t 纹理 ID，失败时为 GZX_INVALID_ID。
 */
GZX_API uint32_t gzx_load_texture(GzxEngine* engine, const char* path);

/**
 * @brief 从 RGBA8 像素数据创建纹理。
 *
 * @param engine 引擎实例。
 * @param width 纹理宽度。
 * @param height 纹理高度。
 * @param rgba 像素数据，长度为 width * height * 4 字节。
 * @return uint32_t 纹理 ID，失败时为 GZX_INVALID_ID。
 */
GZX_API uint32_t gzx_create_texture(GzxEngine* engine, uint32_t width, uint32_t height, const uint8_t* rgba);

/**
 * @brief 获取纹理尺寸。
 *
 * @param engine 引擎实例。
 * @param texture 纹理 ID。
 * @param width 输出宽度，可以为 NULL。
 * @param height 输出高度，可以为 NULL。
 * @return int 成功为 1，失败为 0。
 */
GZX_API int gzx_texture_size(const GzxEngine* engine, uint32_t texture, uint32_t* width, uint32_t* height);

/**
 * @brief 计算纹理中像素区域对应的 UV。
 *
 * @param engine 引擎实例。
 * @param texture 纹理 ID。
 * @param x 区域左上角横坐标（像素）。
 * @param y 区域左上角纵坐标（像素）。
 * @param width 区域宽度（像素）。
 * @param height 区域高度（像素）。
 * @return GzxUv UV 区域，失败时为完整纹理。
 */
GZX_API GzxUv gzx_texture_region(const GzxEngine* engine, uint32_t texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/**
 * @brief 计算按网格排列的精灵表中第 frame 帧的 UV，帧按行优先编号。
 *
 * @param columns 精灵表列数。
 * @param rows 精灵表行数。
 * @param frame 帧序号。
 * @return GzxUv UV 区域。
 */
GZX_API GzxUv gzx_sheet_uv(uint32_t columns, uint32_t rows, uint32_t frame);

/* ------------------------------------------------------------------------- */
/* 绘制（在 gzx_begin_frame 与 gzx_end_frame 之间调用）                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief 设置本帧背景颜色。
 *
 * @param engine 引擎实例。
 * @param color 背景颜色。
 */
GZX_API void gzx_set_background(GzxEngine* engine, GzxColor color);

/**
 * @brief 设置本帧相机。
 *
 * @param engine 引擎实例。
 * @param x 相机左上角的世界横坐标。
 * @param y 相机左上角的世界纵坐标。
 * @param zoom 缩放比例，必须大于 0。
 */
GZX_API void gzx_set_camera(GzxEngine* engine, float x, float y, float zoom);

/**
 * @brief 设置本帧精灵排序模式。
 *
 * @param engine 引擎实例。
 * @param mode 排序模式，GZX_SORT_*。
 */
GZX_API void gzx_set_sort(GzxEngine* engine, int32_t mode);

/**
 * @brief 提交一个精灵。
 *
 * @param engine 引擎实例。
 * @param sprite 精灵。
 */
GZX_API void gzx_draw_sprite(GzxEngine* engine, const GzxSprite* sprite);

/**
 * @brief 批量提交精灵，大量精灵时应优先使用此函数以减少跨语言调用。
 *
 * @param engine 引擎实例。
 * @param sprites 连续的精灵数组。
 * @param count 精灵数量。
 */
GZX_API void gzx_draw_sprites(GzxEngine* engine, const GzxSprite* sprites, size_t count);

/**
 * @brief 提交一个纯色矩形。
 *
 * @param engine 引擎实例。
 * @param x 左上角横坐标。
 * @param y 左上角纵坐标。
 * @param width 宽度。
 * @param height 高度。
 * @param color 颜色。
 * @param space 坐标空间，GZX_SPACE_*。
 * @param layer 层级。
 */
GZX_API void gzx_draw_rect(GzxEngine* engine, float x, float y, float width, float height, GzxColor color, int32_t space, int32_t layer);

/**
 * @brief 获取本帧已提交的精灵数量。
 *
 * @param engine 引擎实例。
 * @return size_t 精灵数量。
 */
GZX_API size_t gzx_sprite_count(const GzxEngine* engine);

/**
 * @brief 拾取屏幕坐标处最上层的精灵。
 *
 * @param engine 引擎实例。
 * @param x 屏幕横坐标。
 * @param y 屏幕纵坐标。
 * @return int64_t 精灵在本帧的提交序号，未命中时为 -1。
 */
GZX_API int64_t gzx_pick(const GzxEngine* engine, float x, float y);

/**
 * @brief 按本帧相机将屏幕坐标转换为世界坐标。
 *
 * @param engine 引擎实例。
 * @param x 屏幕横坐标。
 * @param y 屏幕纵坐标。
 * @param world_x 输出世界横坐标，可以为 NULL。
 * @param world_y 输出世界纵坐标，可以为 NULL。
 */
GZX_API void gzx_screen_to_world(const GzxEngine* engine, float x, float y, float* world_x, float* world_y);

/**
 * @brief 按本帧相机将世界坐标转换为屏幕坐标。
 *
 * @param engine 引擎实例。
 * @param x 世界横坐标。
 * @param y 世界纵坐标。
 * @param screen_x 输出屏幕横坐标，可以为 NULL。
 * @param screen_y 输出屏幕纵坐标，可以为 NULL。
 */
GZX_API void gzx_world_to_screen(const GzxEngine* engine, float x, float y, float* screen_x, float* screen_y);

/* ------------------------------------------------------------------------- */
/* 字体与文本                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief 创建内置 8x8 ASCII 像素字体。
 *
 * @param engine 引擎实例。
 * @return GzxFont* 字体，失败时为 NULL，用 gzx_font_destroy 释放。
 */
GZX_API GzxFont* gzx_font_default(GzxEngine* engine);

/**
 * @brief 从按网格排列的字体图集创建字体。
 *
 * @param engine 引擎实例。
 * @param texture 字体图集纹理 ID。
 * @param cell_width 字符格子宽度（像素）。
 * @param cell_height 字符格子高度（像素）。
 * @param first 图集中第一个字符的 Unicode 编码。
 * @param count 图集中的字符数量。
 * @return GzxFont* 字体，失败时为 NULL，用 gzx_font_destroy 释放。
 */
GZX_API GzxFont* gzx_font_grid(GzxEngine* engine, uint32_t texture, uint32_t cell_width, uint32_t cell_height, uint32_t first, uint32_t count);

/**
 * @brief 将 TTF/OTF 字体烘焙为图集，支持中文等非 ASCII 字符。
 *
 * @param engine 引擎实例。
 * @param path 字体文件路径（UTF-8）。
 * @param pixel_size 字号（像素）。
 * @param characters 额外需要的字符（UTF-8），可打印 ASCII 始终包含，可以为 NULL。
 * @return GzxFont* 字体，失败时为 NULL，用 gzx_font_destroy 释放。
 */
GZX_API GzxFont* gzx_font_ttf(GzxEngine* engine, const char* path, float pixel_size, const char* characters);

/**
 * @brief 释放字体。
 *
 * @param font 字体，可以为 NULL。
 */
GZX_API void gzx_font_destroy(GzxFont* font);

/**
 * @brief 绘制一段 UTF-8 文本，支持换行符。
 *
 * @param engine 引擎实例。
 * @param font 字体。
 * @param text 文本（UTF-8）。
 * @param x 左上角横坐标。
 * @param y 左上角纵坐标。
 * @param scale 缩放比例。
 * @param color 颜色。
 * @param space 坐标空间，GZX_SPACE_*。
 * @param layer 层级。
 */
GZX_API void gzx_draw_text(GzxEngine* engine, const GzxFont* font, const char* text, float x, float y, float scale, GzxColor color, int32_t space, int32_t layer);

/**
 * @brief 测量文本绘制后的尺寸。
 *
 * @param font 字体。
 * @param text 文本（UTF-8）。
 * @param scale 缩放比例。
 * @param width 输出宽度，可以为 NULL。
 * @param height 输出高度，可以为 NULL。
 */
GZX_API void gzx_measure_text(const GzxFont* font, const char* text, float scale, float* width, float* height);

/**
 * @brief 获取字体的行高（像素）。
 *
 * @param font 字体。
 * @return float 行高。
 */
GZX_API float gzx_font_line_height(const GzxFont* font);

/* ------------------------------------------------------------------------- */
/* 音频                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief 音频设备是否可用，不可用时播放请求会被忽略。
 *
 * @param engine 引擎实例。
 * @return int 1 为可用，0 为不可用。
 */
GZX_API int gzx_audio_available(const GzxEngine* engine);

/**
 * @brief 加载并预解码声音文件（WAV/OGG/MP3/FLAC 等），重复加载同一路径返回同一 ID。
 *
 * @param engine 引擎实例。
 * @param path 声音文件路径（UTF-8）。
 * @return uint32_t 声音 ID，失败时为 GZX_INVALID_ID。
 */
GZX_API uint32_t gzx_load_sound(GzxEngine* engine, const char* path);

/**
 * @brief 从浮点采样创建声音，适用于程序化生成的音效。
 *
 * @param engine 引擎实例。
 * @param samples 交错排列的采样，取值 -1 到 1。
 * @param count 采样总数（所有声道合计）。
 * @param channels 声道数。
 * @param sample_rate 采样率。
 * @return uint32_t 声音 ID，失败时为 GZX_INVALID_ID。
 */
GZX_API uint32_t gzx_create_sound(GzxEngine* engine, const float* samples, size_t count, uint32_t channels, uint32_t sample_rate);

/**
 * @brief 播放声音。
 *
 * @param engine 引擎实例。
 * @param sound 声音 ID。
 * @param volume 音量，1 为原始音量。
 * @param pan 声像，-1 最左，0 居中，1 最右。
 * @param loop 是否循环播放。
 * @return uint64_t 声部句柄，设备不可用或声部已满时为 GZX_INVALID_HANDLE。
 */
GZX_API uint64_t gzx_play(GzxEngine* engine, uint32_t sound, float volume, float pan, int loop);

/**
 * @brief 停止声部，句柄失效时忽略。
 *
 * @param engine 引擎实例。
 * @param voice 声部句柄。
 */
GZX_API void gzx_stop(GzxEngine* engine, uint64_t voice);

/**
 * @brief 停止所有声部。
 *
 * @param engine 引擎实例。
 */
GZX_API void gzx_stop_all(GzxEngine* engine);

/**
 * @brief 声部是否仍在播放。
 *
 * @param engine 引擎实例。
 * @param voice 声部句柄。
 * @return int 1 为正在播放，否则为 0。
 */
GZX_API int gzx_playing(const GzxEngine* engine, uint64_t voice);

/**
 * @brief 设置声部音量。
 *
 * @param engine 引擎实例。
 * @param voice 声部句柄。
 * @param volume 音量。
 */
GZX_API void gzx_set_voice_volume(GzxEngine* engine, uint64_t voice, float volume);

/**
 * @brief 设置声部声像。
 *
 * @param engine 引擎实例。
 * @param voice 声部句柄。
 * @param pan 声像，-1 到 1。
 */
GZX_API void gzx_set_voice_pan(GzxEngine* engine, uint64_t voice, float pan);

/**
 * @brief 设置主音量，作用于所有声部。
 *
 * @param engine 引擎实例。
 * @param volume 主音量。
 */
GZX_API void gzx_set_master_volume(GzxEngine* engine, float volume);

/* ------------------------------------------------------------------------- */
/* 物理                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief 创建物理世界，y 轴向下为正。
 *
 * @param gravity_x 水平重力加速度。
 * @param gravity_y 垂直重力加速度。
 * @param cell_size 宽相位网格尺寸，取常见物体尺寸的 2 到 4 倍效果最好。
 * @return GzxWorld* 物理世界，失败时为 NULL，用 gzx_world_destroy 释放。
 */
GZX_API GzxWorld* gzx_world_create(float gravity_x, float gravity_y, float cell_size);

/**
 * @brief 销毁物理世界。
 *
 * @param world 物理世界，可以为 NULL。
 */
GZX_API void gzx_world_destroy(GzxWorld* world);

/**
 * @brief 用默认值填充刚体定义。
 *
 * @param definition 要填充的定义。
 */
GZX_API void gzx_body_def_init(GzxBodyDef* definition);

/**
 * @brief 创建刚体。
 *
 * @param world 物理世界。
 * @param definition 刚体定义。
 * @return uint64_t 刚体句柄，失败时为 GZX_INVALID_HANDLE。
 */
GZX_API uint64_t gzx_world_create_body(GzxWorld* world, const GzxBodyDef* definition);

/**
 * @brief 销毁刚体，句柄失效时忽略。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 */
GZX_API void gzx_world_destroy_body(GzxWorld* world, uint64_t body);

/**
 * @brief 刚体句柄是否有效。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 * @return int 1 为有效，0 为无效。
 */
GZX_API int gzx_world_valid(const GzxWorld* world, uint64_t body);

/**
 * @brief 获取刚体中心位置。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 * @param x 输出横坐标，可以为 NULL。
 * @param y 输出纵坐标，可以为 NULL。
 */
GZX_API void gzx_world_position(const GzxWorld* world, uint64_t body, float* x, float* y);

/**
 * @brief 瞬移刚体。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 * @param x 新的横坐标。
 * @param y 新的纵坐标。
 */
GZX_API void gzx_world_set_position(GzxWorld* world, uint64_t body, float x, float y);

/**
 * @brief 获取刚体速度。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 * @param x 输出水平速度，可以为 NULL。
 * @param y 输出垂直速度，可以为 NULL。
 */
GZX_API void gzx_world_velocity(const GzxWorld* world, uint64_t body, float* x, float* y);

/**
 * @brief 设置刚体速度。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 * @param x 水平速度。
 * @param y 垂直速度。
 */
GZX_API void gzx_world_set_velocity(GzxWorld* world, uint64_t body, float x, float y);

/**
 * @brief 对动态刚体施加冲量。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 * @param x 水平冲量。
 * @param y 垂直冲量。
 */
GZX_API void gzx_world_apply_impulse(GzxWorld* world, uint64_t body, float x, float y);

/**
 * @brief 获取刚体的用户数据。
 *
 * @param world 物理世界。
 * @param body 刚体句柄。
 * @return uint64_t 用户数据，句柄无效时为 0。
 */
GZX_API uint64_t gzx_world_user_data(const GzxWorld* world, uint64_t body);

/**
 * @brief 设置重力。
 *
 * @param world 物理世界。
 * @param gravity_x 水平重力加速度。
 * @param gravity_y 垂直重力加速度。
 */
GZX_API void gzx_world_set_gravity(GzxWorld* world, float gravity_x, float gravity_y);

/**
 * @brief 推进模拟。
 *
 * @param world 物理世界。
 * @param delta_time 时间步长（秒）。
 * @param substeps 子步数，越多越稳定。
 * @param iterations 每个子步的约束迭代次数。
 */
GZX_API void gzx_world_step(GzxWorld* world, float delta_time, int32_t substeps, int32_t iterations);

/**
 * @brief 获取最近一次步进产生的接触。
 *
 * @param world 物理世界。
 * @param out 输出数组，可以为 NULL。
 * @param capacity 输出数组容量。
 * @return size_t 接触总数，可能大于 capacity。
 */
GZX_API size_t gzx_world_contacts(const GzxWorld* world, GzxContact* out, size_t capacity);

/**
 * @brief 查询包含指定点的刚体。
 *
 * @param world 物理世界。
 * @param x 点的横坐标。
 * @param y 点的纵坐标。
 * @param mask 碰撞类别掩码。
 * @param out 输出刚体句柄数组，可以为 NULL。
 * @param capacity 输出数组容量。
 * @return size_t 命中总数，可能大于 capacity。
 */
GZX_API size_t gzx_world_query_point(GzxWorld* world, float x, float y, uint32_t mask, uint64_t* out, size_t capacity);

/**
 * @brief 查询与矩形区域重叠的刚体。
 *
 * @param world 物理世界。
 * @param left 左边界。
 * @param top 上边界。
 * @param right 右边界。
 * @param bottom 下边界。
 * @param mask 碰撞类别掩码。
 * @param out 输出刚体句柄数组，可以为 NULL。
 * @param capacity 输出数组容量。
 * @return size_t 命中总数，可能大于 capacity。
 */
GZX_API size_t gzx_world_query_rect(GzxWorld* world, float left, float top, float right, float bottom, uint32_t mask, uint64_t* out, size_t capacity);

/**
 * @brief 射线检测，返回最近的命中。
 *
 * @param world 物理世界。
 * @param x0 起点横坐标。
 * @param y0 起点纵坐标。
 * @param x1 终点横坐标。
 * @param y1 终点纵坐标。
 * @param mask 碰撞类别掩码。
 * @param hit 输出命中信息。
 * @return int 1 为命中，0 为未命中。
 */
GZX_API int gzx_world_raycast(const GzxWorld* world, float x0, float y0, float x1, float y1, uint32_t mask, GzxRayHit* hit);

/**
 * @brief 批量读取刚体位置，适合每帧同步大量物体的绘制坐标。
 *
 * @param world 物理世界。
 * @param bodies 刚体句柄数组。
 * @param count 刚体数量。
 * @param out_xy 输出数组，长度至少为 count * 2，按 x, y 交错排列。
 */
GZX_API void gzx_world_positions(const GzxWorld* world, const uint64_t* bodies, size_t count, float* out_xy);

/**
 * @brief 获取存活的刚体数量。
 *
 * @param world 物理世界。
 * @return size_t 刚体数量。
 */
GZX_API size_t gzx_world_body_count(const GzxWorld* world);

#ifdef __cplusplus
}
#endif

#endif /* GZX_H */
