#pragma once

#include <SDL3/SDL.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef s8       b8;
typedef s16      b16;
typedef s32      b32;
typedef s64      b64;

typedef float    f32;
typedef double   f64;

#define ARRAY_COUNT(a) (sizeof((a)) / sizeof((a)[0]))

#define ASSERT(expr) SDL_assert((expr))
