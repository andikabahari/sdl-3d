#include "app.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>


// Settings.

#define WINDOW_TITLE  "sdl-3d"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_FLAGS  0

#define CLEAR_COLOR {1.0f, 1.0f, 1.0f, 1.0f}


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    static App_State state{};

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    ASSERT(SDL_Init(SDL_INIT_VIDEO));

    state.current_time = SDL_GetTicks();
    state.last_time    = state.current_time;

    state.window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
    ASSERT(state.window != NULL);

    gfx_init(&state.gfx, state.window);
    gfx_model_load(&state.sample_model, "res/models/sample/scene.gltf");

    *appstate = &state;
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    auto state = static_cast<App_State *>(appstate);

    gfx_model_cleanup(&state->sample_model);

    gfx_cleanup(&state->gfx);

    SDL_DestroyWindow(state->window);

    state->current_time = 0.0;
    state->last_time = 0.0;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            break;
        }
        case SDL_EVENT_KEY_UP: {
            break;
        }
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            return SDL_APP_SUCCESS;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto state = static_cast<App_State *>(appstate);

    state->current_time = SDL_GetTicks();
    auto delta_time     = static_cast<f32>(state->current_time - state->last_time) / 1000.f;
    state->last_time    = state->current_time;

    state->rotate += glm::radians(90.0f * delta_time);

    gfx_draw(&state->gfx, state->rotate, CLEAR_COLOR);

    return SDL_APP_CONTINUE;
}
