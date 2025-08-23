#include "app.h"

struct App_State {
    SDL_Window *window;
    SDL_GPUDevice *gpu_device;
    SDL_GPUGraphicsPipeline *graphics_pipeline;
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    static App_State state{};

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    ASSERT(SDL_Init(SDL_INIT_VIDEO));

    state.window = SDL_CreateWindow("sdl-3d", 1280, 720, 0);
    ASSERT(state.window != NULL);

    state.gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    ASSERT(state.window != NULL);

    ASSERT(SDL_ClaimWindowForGPUDevice(state.gpu_device, state.window));

    { // Init graphics pipeline
        u64 vertex_code_size;
        auto vertex_code = SDL_LoadFile("res/shaders/basic.vert.spv", &vertex_code_size);
        SDL_GPUShaderCreateInfo vertex_info{};
        vertex_info.code_size            = vertex_code_size;
        vertex_info.code                 = static_cast<u8 *>(vertex_code);
        vertex_info.entrypoint           = "main";
        vertex_info.format               = SDL_GPU_SHADERFORMAT_SPIRV;
        vertex_info.stage                = SDL_GPU_SHADERSTAGE_VERTEX;
        vertex_info.num_samplers         = 0;
        vertex_info.num_storage_textures = 0;
        vertex_info.num_storage_buffers  = 0;
        vertex_info.num_uniform_buffers  = 0;
        auto vertex_shader = SDL_CreateGPUShader(state.gpu_device, &vertex_info);
        SDL_free(vertex_code);

        u64 fragment_code_size;
        auto fragment_code = SDL_LoadFile("res/shaders/basic.frag.spv", &fragment_code_size);
        SDL_GPUShaderCreateInfo fragment_info{};
        fragment_info.code_size            = fragment_code_size;
        fragment_info.code                 = static_cast<u8 *>(fragment_code);
        fragment_info.entrypoint           = "main";
        fragment_info.format               = SDL_GPU_SHADERFORMAT_SPIRV;
        fragment_info.stage                = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fragment_info.num_samplers         = 0;
        fragment_info.num_storage_textures = 0;
        fragment_info.num_storage_buffers  = 0;
        fragment_info.num_uniform_buffers  = 0;
        auto fragment_shader = SDL_CreateGPUShader(state.gpu_device, &fragment_info);
        SDL_free(fragment_code);

        SDL_GPUColorTargetDescription color_target_description{};
        color_target_description.format = SDL_GetGPUSwapchainTextureFormat(state.gpu_device, state.window);

        SDL_GPUGraphicsPipelineTargetInfo target_info{};
        target_info.color_target_descriptions = &color_target_description;
        target_info.num_color_targets         = 1;

        SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.vertex_shader      = vertex_shader;
        pipeline_info.fragment_shader    = fragment_shader;
        pipeline_info.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipeline_info.target_info        = target_info;

        state.graphics_pipeline = SDL_CreateGPUGraphicsPipeline(state.gpu_device, &pipeline_info);
    }

    *appstate = &state;
    return SDL_APP_CONTINUE;
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

    auto command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);

    SDL_GPUTexture *swapchain_texture;
    ASSERT(SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, state->window, &swapchain_texture, NULL, NULL));
    if (swapchain_texture != NULL) {
        SDL_GPUColorTargetInfo color_target{};
        color_target.clear_color = {1.0f, 1.0f, 1.0f, 1.0f};
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.texture     = swapchain_texture;

        auto render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, NULL);

        SDL_BindGPUGraphicsPipeline(render_pass, state->graphics_pipeline);
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);

        SDL_EndGPURenderPass(render_pass);
    }

    ASSERT(SDL_SubmitGPUCommandBuffer(command_buffer));

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    //
}
