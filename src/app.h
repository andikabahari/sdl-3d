#pragma once

#include "defines.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include "gfx.h"

struct App_State {
    SDL_Window *window;
    Gfx_Context gfx;

    // Time in milliseconds.
    u64 last_time = 0;
    u64 current_time = 0;

    f32 rotate = 0.0f;
    glm::mat4 proj  = glm::mat4(1.0f);
    glm::mat4 model = glm::mat4(1.0f);

    Gfx_Model sample_model;
};
