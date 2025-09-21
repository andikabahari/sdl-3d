#pragma once

#include <stddef.h>
#include <stdint.h>


// Basic types.

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s8  b8;
typedef s16 b16;
typedef s32 b32;
typedef s64 b64;

typedef float  f32;
typedef double f64;

typedef ptrdiff_t ssize;
typedef size_t    usize;

#define cast(T) (T)


// Defer cheat.
// Source: https://gist.github.com/andrewrk/ffb272748448174e6cdb4958dae9f3d8

#define _GLUE(x,y) x##y
#define GLUE(x,y)  _GLUE(x,y)

template<typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda) : lambda(lambda) {}
    ~ExitScope() { lambda(); }
    ExitScope(const ExitScope&);
  private:
    ExitScope& operator =(const ExitScope&);
};
 
class ExitScopeHelp {
  public:
    template<typename T>
        ExitScope<T> operator+(T t){ return t; }
};
 
#define defer const auto& GLUE(defer__, __LINE__) = ExitScopeHelp() + [&]()


// Misc.

#define ARRAY_COUNT(a) (sizeof((a)) / sizeof((a)[0]))

#define ASSERT(expr) SDL_assert((expr))
