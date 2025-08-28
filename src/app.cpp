#include "app.h"

static void upload_buffer_ex(SDL_GPUDevice *device, u32 src_offset, SDL_GPUTransferBuffer *src_buffer, u32 size, u32 dst_offset, SDL_GPUBuffer *dst_buffer, bool cyclic) {
    auto command_buffer = SDL_AcquireGPUCommandBuffer(device);
    auto copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    SDL_GPUTransferBufferLocation copy_src{};
    copy_src.transfer_buffer = src_buffer;
    copy_src.offset = src_offset;

    SDL_GPUBufferRegion copy_dst{};
    copy_dst.buffer = dst_buffer;
    copy_dst.offset = dst_offset;
    copy_dst.size = size;
    
    SDL_UploadToGPUBuffer(copy_pass, &copy_src, &copy_dst, cyclic);

    SDL_EndGPUCopyPass(copy_pass);
    ASSERT(SDL_SubmitGPUCommandBuffer(command_buffer));
}

static void upload_buffer(SDL_GPUDevice *device, SDL_GPUTransferBuffer *src_buffer, u32 size, SDL_GPUBuffer *dst_buffer) {
    upload_buffer_ex(device, 0, src_buffer, size, 0, dst_buffer, false);
}

struct Uniform_Block {
    glm::mat4 mvp;
};

struct Vertex_Data {
    glm::vec3 position;
    glm::vec4 color;
};

static Vertex_Data vertices[] = {
    {glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)},
    {glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)},
    {glm::vec3( 0.0f,  0.5f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)},
};

struct App_State {
    SDL_Window *window        = NULL;
    SDL_GPUDevice *gpu_device = NULL;

    SDL_GPUGraphicsPipeline *graphics_pipeline = NULL;
    SDL_GPUBuffer *vertex_buffer = NULL;

    // Time in milliseconds.
    u64 last_time    = 0;
    u64 current_time = 0;

    f32 rotate = 0.0f;
    glm::mat4 proj  = glm::mat4(1.0f);
    glm::mat4 model = glm::mat4(1.0f);
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    static App_State state{};

    // 
    // Subsystems
    //

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    ASSERT(SDL_Init(SDL_INIT_VIDEO));

    state.window = SDL_CreateWindow("sdl-3d", 1280, 720, 0);
    ASSERT(state.window != NULL);

    state.gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    ASSERT(state.window != NULL);

    ASSERT(SDL_ClaimWindowForGPUDevice(state.gpu_device, state.window));

    // 
    // V-Sync option
    //

#if 0
    SDL_GPUPresentMode present_mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(state.gpu_device, state.window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    } else if (SDL_WindowSupportsGPUPresentMode(state.gpu_device, state.window, SDL_GPU_PRESENTMODE_MAILBOX)) {
        present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    SDL_SetGPUSwapchainParameters(state.gpu_device, state.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode);
#endif

    // 
    // Graphics pipeline
    //
    
    {
        //
        // Shaders
        //
        
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
        vertex_info.num_uniform_buffers  = 1;
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

        // 
        // Vertex input state
        //

        SDL_GPUVertexBufferDescription description0{};
        description0.slot               = 0;
        description0.pitch              = sizeof(Vertex_Data);
        description0.input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        description0.instance_step_rate = 0;

        SDL_GPUVertexBufferDescription vertex_buffer_descriptions[] = {
            description0,
        };

        SDL_GPUVertexAttribute attribute0{};
        attribute0.location    = 0;
        attribute0.buffer_slot = 0;
        attribute0.format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attribute0.offset      = offsetof(Vertex_Data, position);

        SDL_GPUVertexAttribute attribute1{};
        attribute1.location    = 1;
        attribute1.buffer_slot = 0;
        attribute1.format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attribute1.offset      = offsetof(Vertex_Data, color);

        SDL_GPUVertexAttribute vertex_attributes[] = {
            attribute0,
            attribute1,
        };

        SDL_GPUVertexInputState vertex_input_state{};
        vertex_input_state.vertex_buffer_descriptions = vertex_buffer_descriptions;
        vertex_input_state.num_vertex_buffers         = ARRAY_COUNT(vertex_buffer_descriptions);
        vertex_input_state.vertex_attributes          = vertex_attributes;
        vertex_input_state.num_vertex_attributes      = ARRAY_COUNT(vertex_attributes);

        // 
        // Graphics pipeline
        //

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
        pipeline_info.vertex_input_state = vertex_input_state;

        state.graphics_pipeline = SDL_CreateGPUGraphicsPipeline(state.gpu_device, &pipeline_info);

        SDL_ReleaseGPUShader(state.gpu_device, fragment_shader);
        SDL_ReleaseGPUShader(state.gpu_device, vertex_shader);
    }

    // 
    // Vertex buffer
    //
    
    {
        SDL_GPUBufferCreateInfo vertex_info{};
        vertex_info.size  = sizeof(vertices);
        vertex_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        state.vertex_buffer = SDL_CreateGPUBuffer(state.gpu_device, &vertex_info);

        SDL_GPUTransferBufferCreateInfo transfer_info{};
        transfer_info.size  = sizeof(vertices);
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        auto transfer_buffer = SDL_CreateGPUTransferBuffer(state.gpu_device, &transfer_info);

        auto data = SDL_MapGPUTransferBuffer(state.gpu_device, transfer_buffer, false);
        SDL_memcpy(data, vertices, sizeof(vertices));
        SDL_UnmapGPUTransferBuffer(state.gpu_device, transfer_buffer);

        upload_buffer(state.gpu_device, transfer_buffer, sizeof(vertices), state.vertex_buffer);

        SDL_ReleaseGPUTransferBuffer(state.gpu_device, transfer_buffer);
    }

    // 
    // Projection matrix
    //
    
    {
        s32 _w, _h;
        ASSERT(SDL_GetWindowSizeInPixels(state.window, &_w, &_h));
        f32 width  = static_cast<f32>(_w);
        f32 height = static_cast<f32>(_h);

        f32 aspect = width / height;
        state.proj = glm::perspective(glm::radians(70.0f), aspect, 0.0001f, 1000.0f);
    }

    // 
    // Frame time
    //

    state.last_time = SDL_GetTicks();

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

    state->current_time = SDL_GetTicks();
    auto delta_time     = static_cast<f32>(state->current_time - state->last_time) / 1000.f;
    state->last_time    = state->current_time;

    auto command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);

    SDL_GPUTexture *swapchain_texture;
    ASSERT(SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, state->window, &swapchain_texture, NULL, NULL));
    if (swapchain_texture != NULL) {
        state->rotate += glm::radians(90.f * delta_time);
        state->model = glm::mat4(1.0f);
        state->model = glm::translate(state->model, glm::vec3(0.0f, 0.0f, -5.0f));
        state->model = glm::rotate(state->model, state->rotate, glm::vec3(0.0f, 1.0f, 0.0f));

        Uniform_Block uniform_block{};
        uniform_block.mvp = state->proj * state->model;

        SDL_GPUColorTargetInfo color_target{};
        color_target.clear_color = {1.0f, 1.0f, 1.0f, 1.0f};
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.texture     = swapchain_texture;

        SDL_GPUBufferBinding binding0{};
        binding0.buffer = state->vertex_buffer;
        binding0.offset = 0;

        SDL_GPUBufferBinding buffer_bindings[] = {
            binding0,
        };

        auto render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, NULL);

        SDL_BindGPUGraphicsPipeline(render_pass, state->graphics_pipeline);
        SDL_BindGPUVertexBuffers(render_pass, 0, buffer_bindings, ARRAY_COUNT(buffer_bindings));
        SDL_PushGPUVertexUniformData(command_buffer, 0, &uniform_block, sizeof(Uniform_Block));
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);

        SDL_EndGPURenderPass(render_pass);
    }

    ASSERT(SDL_SubmitGPUCommandBuffer(command_buffer));

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    //
}
