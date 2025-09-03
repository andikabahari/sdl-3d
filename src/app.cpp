#include "app.h"

static void immediate_upload_buffer_ex(SDL_GPUDevice *device, u32 src_offset, SDL_GPUTransferBuffer *src_buffer, u32 size, u32 dst_offset, SDL_GPUBuffer *dst_buffer, bool cyclic) {
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

static void immediate_upload_buffer(SDL_GPUDevice *device, SDL_GPUTransferBuffer *src_buffer, u32 size, SDL_GPUBuffer *dst_buffer) {
    immediate_upload_buffer_ex(device, 0, src_buffer, size, 0, dst_buffer, false);
}

struct Upload_Buffer_State {
    SDL_GPUCommandBuffer *command_buffer;
    SDL_GPUCopyPass *copy_pass;
    SDL_GPUTransferBuffer *transfer_buffer;
    bool cyclic;
    bool existing_copy_pass;
    u32 offset = 0;
};

static Upload_Buffer_State *begin_upload_buffer(SDL_GPUDevice *device, SDL_GPUTransferBuffer *transfer_buffer, bool cyclic) {
    auto state = new Upload_Buffer_State{};
    state->command_buffer = SDL_AcquireGPUCommandBuffer(device);
    state->copy_pass = SDL_BeginGPUCopyPass(state->command_buffer);
    state->transfer_buffer = transfer_buffer;
    state->cyclic = cyclic;
    state->existing_copy_pass = false;
    return state;
}

static Upload_Buffer_State *begin_upload_buffer_with_pass(SDL_GPUCopyPass *copy_pass, SDL_GPUTransferBuffer *transfer_buffer, bool cyclic) {
    auto state = new Upload_Buffer_State{};
    state->copy_pass = copy_pass;
    state->transfer_buffer = transfer_buffer;
    state->cyclic = cyclic;
    state->existing_copy_pass = true;
    return state;
}

static void end_upload_buffer(Upload_Buffer_State *state) {
    if (!state->existing_copy_pass) {
        SDL_EndGPUCopyPass(state->copy_pass);
        ASSERT(SDL_SubmitGPUCommandBuffer(state->command_buffer));
    }
    delete state;
}

static void do_upload_buffer(Upload_Buffer_State *state, u32 size, u32 offset, SDL_GPUBuffer *buffer) {
    SDL_GPUTransferBufferLocation copy_src{};
    copy_src.transfer_buffer = state->transfer_buffer;
    copy_src.offset = state->offset;

    SDL_GPUBufferRegion copy_dst{};
    copy_dst.buffer = buffer;
    copy_dst.offset = offset;
    copy_dst.size = size;

    SDL_UploadToGPUBuffer(state->copy_pass, &copy_src, &copy_dst, state->cyclic);

    state->offset += size;
}

struct Uniform_Block {
    glm::mat4 mvp;
};

struct Vertex_Data {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 texcoord;
};

#define WHITE glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)

static Vertex_Data vertices[] = {
    {glm::vec3(-0.5f, -0.5f, 0.0f), WHITE, glm::vec2(0.0f, 1.0f)}, // Bottom-left
    {glm::vec3( 0.5f, -0.5f, 0.0f), WHITE, glm::vec2(1.0f, 1.0f)}, // Bottom-right
    {glm::vec3( 0.5f,  0.5f, 0.0f), WHITE, glm::vec2(1.0f, 0.0f)}, // Top-right
    {glm::vec3(-0.5f,  0.5f, 0.0f), WHITE, glm::vec2(0.0f, 0.0f)}, // Top-left
};

static u16 vertex_indices[] = {
    0, 1, 2,
    2, 3, 0,
};

struct App_State {
    SDL_Window *window        = NULL;
    SDL_GPUDevice *gpu_device = NULL;

    SDL_GPUGraphicsPipeline *graphics_pipeline = NULL;
    SDL_GPUBuffer *vertex_buffer = NULL;
    SDL_GPUBuffer *index_buffer  = NULL;
    SDL_GPUTexture *texture      = NULL;
    SDL_GPUSampler *sampler      = NULL;

    // Time in milliseconds.
    u64 last_time    = 0;
    u64 current_time = 0;

    f32 rotate = 0.0f;
    glm::mat4 proj  = glm::mat4(1.0f);
    glm::mat4 model = glm::mat4(1.0f);
};

static void app_init_window(App_State *state) {
    state->window = SDL_CreateWindow("sdl-3d", 1280, 720, 0);
    ASSERT(state->window != NULL);
}

static void app_cleanup_window(App_State *state) {
    SDL_DestroyWindow(state->window);
}

static void app_init_gpu_device(App_State *state) {
    state->gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    ASSERT(state->gpu_device != NULL);
    ASSERT(SDL_ClaimWindowForGPUDevice(state->gpu_device, state->window));
}

static void app_cleanup_gpu_device(App_State *state) {
    SDL_DestroyGPUDevice(state->gpu_device);
}

static void app_init_present_mode(App_State *state) {
    SDL_GPUPresentMode present_mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(state->gpu_device, state->window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    } else if (SDL_WindowSupportsGPUPresentMode(state->gpu_device, state->window, SDL_GPU_PRESENTMODE_MAILBOX)) {
        present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    SDL_SetGPUSwapchainParameters(state->gpu_device, state->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode);
}

static void app_init_graphics_pipeline(App_State *state) {
    // Load shaders
    // -----------------------------------------------------

    u64 vertex_code_size;
    auto vertex_code = SDL_LoadFile("res/shaders/basic.vert.spv", &vertex_code_size);
    SDL_GPUShaderCreateInfo vertex_info{};
    vertex_info.code_size    = vertex_code_size;
    vertex_info.code         = static_cast<u8 *>(vertex_code);
    vertex_info.entrypoint   = "main";
    vertex_info.format       = SDL_GPU_SHADERFORMAT_SPIRV;
    vertex_info.stage        = SDL_GPU_SHADERSTAGE_VERTEX;
    vertex_info.num_samplers = 0;
    vertex_info.num_storage_textures = 0;
    vertex_info.num_storage_buffers  = 0;
    vertex_info.num_uniform_buffers  = 1;
    auto vertex_shader = SDL_CreateGPUShader(state->gpu_device, &vertex_info);
    SDL_free(vertex_code);

    u64 fragment_code_size;
    auto fragment_code = SDL_LoadFile("res/shaders/basic.frag.spv", &fragment_code_size);
    SDL_GPUShaderCreateInfo fragment_info{};
    fragment_info.code_size    = fragment_code_size;
    fragment_info.code         = static_cast<u8 *>(fragment_code);
    fragment_info.entrypoint   = "main";
    fragment_info.format       = SDL_GPU_SHADERFORMAT_SPIRV;
    fragment_info.stage        = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fragment_info.num_samplers = 1;
    fragment_info.num_storage_textures = 0;
    fragment_info.num_storage_buffers  = 0;
    fragment_info.num_uniform_buffers  = 0;
    auto fragment_shader = SDL_CreateGPUShader(state->gpu_device, &fragment_info);
    SDL_free(fragment_code);

    // Configure vertex input state
    // -----------------------------------------------------
    
    SDL_GPUVertexBufferDescription description0{};
    description0.slot  = 0;
    description0.pitch = sizeof(Vertex_Data);
    description0.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
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

    SDL_GPUVertexAttribute attribute2{};
    attribute2.location    = 2;
    attribute2.buffer_slot = 0;
    attribute2.format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attribute2.offset      = offsetof(Vertex_Data, texcoord);

    SDL_GPUVertexAttribute vertex_attributes[] = {
        attribute0,
        attribute1,
        attribute2,
    };

    SDL_GPUVertexInputState vertex_input_state{};
    vertex_input_state.vertex_buffer_descriptions = vertex_buffer_descriptions;
    vertex_input_state.num_vertex_buffers = ARRAY_COUNT(vertex_buffer_descriptions);
    vertex_input_state.vertex_attributes  = vertex_attributes;
    vertex_input_state.num_vertex_attributes = ARRAY_COUNT(vertex_attributes);

    // Create graphics pipeline
    // -----------------------------------------------------

    SDL_GPUColorTargetDescription color_target_description{};
    color_target_description.format = SDL_GetGPUSwapchainTextureFormat(state->gpu_device, state->window);
    
    SDL_GPUGraphicsPipelineTargetInfo target_info{};
    target_info.color_target_descriptions = &color_target_description;
    target_info.num_color_targets = 1;
    
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.vertex_shader   = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.target_info     = target_info;
    pipeline_info.vertex_input_state = vertex_input_state;

    // Do the thing!
    state->graphics_pipeline = SDL_CreateGPUGraphicsPipeline(state->gpu_device, &pipeline_info);

    SDL_ReleaseGPUShader(state->gpu_device, fragment_shader);
    SDL_ReleaseGPUShader(state->gpu_device, vertex_shader);
}

static void app_cleanup_graphics_pipeline(App_State *state) {
    SDL_ReleaseGPUGraphicsPipeline(state->gpu_device, state->graphics_pipeline);
}

static void app_init_vertex_and_index_buffers(App_State *state, SDL_GPUCopyPass *copy_pass) {
    SDL_GPUBufferCreateInfo vertex_info{};
    vertex_info.size  = sizeof(vertices);
    vertex_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    state->vertex_buffer = SDL_CreateGPUBuffer(state->gpu_device, &vertex_info);

    SDL_GPUBufferCreateInfo index_info{};
    index_info.size  = sizeof(vertex_indices);
    index_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    state->index_buffer = SDL_CreateGPUBuffer(state->gpu_device, &index_info);

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.size  = vertex_info.size + index_info.size;
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    auto transfer_buffer = SDL_CreateGPUTransferBuffer(state->gpu_device, &transfer_info);

    auto data = static_cast<u8 *>(SDL_MapGPUTransferBuffer(state->gpu_device, transfer_buffer, false));
    u32 offset = 0;
    SDL_memcpy(data + offset, vertices, vertex_info.size);
    offset += vertex_info.size;
    SDL_memcpy(data + offset, vertex_indices, index_info.size);
    offset += index_info.size;
    SDL_UnmapGPUTransferBuffer(state->gpu_device, transfer_buffer);

    auto upload = begin_upload_buffer_with_pass(copy_pass, transfer_buffer, false);
    do_upload_buffer(upload, vertex_info.size, 0, state->vertex_buffer);
    do_upload_buffer(upload, index_info.size, 0, state->index_buffer);
    end_upload_buffer(upload);

    SDL_ReleaseGPUTransferBuffer(state->gpu_device, transfer_buffer);
}

static void app_cleanup_vertex_and_index_buffers(App_State *state) {
    SDL_ReleaseGPUBuffer(state->gpu_device, state->index_buffer);
    SDL_ReleaseGPUBuffer(state->gpu_device, state->vertex_buffer);
}

static void app_init_texture(App_State *state, SDL_GPUCopyPass *copy_pass) {
    static s32 img_width  = 0;
    static s32 img_height = 0;
    static auto img_data  = stbi_load("res/images/Sample.png", &img_width, &img_height, NULL, 4);
    ASSERT(img_data != NULL);

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.size  = static_cast<u32>(img_width * img_height * 4);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    auto transfer_buffer = SDL_CreateGPUTransferBuffer(state->gpu_device, &transfer_info);

    {
        auto data = SDL_MapGPUTransferBuffer(state->gpu_device, transfer_buffer, false);
        memcpy(data, img_data, transfer_info.size);
        SDL_UnmapGPUTransferBuffer(state->gpu_device, transfer_buffer);
    }

    stbi_image_free(img_data);

    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type   = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage  = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width  = static_cast<u32>(img_width);
    texture_info.height = static_cast<u32>(img_height);
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    state->texture = SDL_CreateGPUTexture(state->gpu_device, &texture_info);

    SDL_GPUSamplerCreateInfo sampler_info{};
    sampler_info.min_filter  = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter  = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    state->sampler = SDL_CreateGPUSampler(state->gpu_device, &sampler_info);

    {
        SDL_GPUTextureTransferInfo copy_src{};
        copy_src.transfer_buffer = transfer_buffer;
        copy_src.offset = 0;

        SDL_GPUTextureRegion copy_dst{};
        copy_dst.texture = state->texture;
        copy_dst.w = static_cast<u32>(img_width);
        copy_dst.h = static_cast<u32>(img_height);
        copy_dst.d = 1;

        SDL_UploadToGPUTexture(copy_pass, &copy_src, &copy_dst, false);
    }

    SDL_ReleaseGPUTransferBuffer(state->gpu_device, transfer_buffer);
}

static void app_cleanup_texture(App_State *state) {
    SDL_ReleaseGPUSampler(state->gpu_device, state->sampler);
    SDL_ReleaseGPUTexture(state->gpu_device, state->texture);
}

static void app_init_render_object(App_State *state) {
    auto command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);
    auto copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    
    app_init_vertex_and_index_buffers(state, copy_pass);
    app_init_texture(state, copy_pass);

    SDL_EndGPUCopyPass(copy_pass);
    ASSERT(SDL_SubmitGPUCommandBuffer(command_buffer));
}

static void app_cleanup_render_object(App_State *state) {
    app_cleanup_texture(state);
    app_cleanup_vertex_and_index_buffers(state);
}

static void app_init_time(App_State *state) {
    state->current_time = SDL_GetTicks();
    state->last_time = state->current_time;
}

static void app_cleanup_time(App_State *state) {
    state->current_time = 0.0;
    state->last_time = 0.0;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    
    ASSERT(SDL_Init(SDL_INIT_VIDEO));

    static App_State state{};
    app_init_window(&state);
    app_init_gpu_device(&state);
    app_init_present_mode(&state);
    app_init_graphics_pipeline(&state);
    app_init_render_object(&state); // Vertex buffer, index buffer, texture
    {
        s32 _w, _h;
        ASSERT(SDL_GetWindowSizeInPixels(state.window, &_w, &_h));
        f32 width  = static_cast<f32>(_w);
        f32 height = static_cast<f32>(_h);
        state.proj = glm::perspective(glm::radians(70.0f), width/height, 0.0001f, 1000.0f);
    }
    app_init_time(&state);

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

    // Update state
    // -----------------------------------------------------

    state->current_time = SDL_GetTicks();
    auto delta_time     = static_cast<f32>(state->current_time - state->last_time) / 1000.f;
    state->last_time    = state->current_time;

    state->rotate += glm::radians(90.f * delta_time);
    state->model = glm::mat4(1.0f);
    state->model = glm::translate(state->model, glm::vec3(0.0f, 0.0f, -5.0f));
    state->model = glm::rotate(state->model, state->rotate, glm::vec3(0.0f, 1.0f, 0.0f));

    // Draw drame
    // -----------------------------------------------------

    auto command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);

    SDL_GPUTexture *swapchain_texture;
    ASSERT(SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, state->window, &swapchain_texture, NULL, NULL));
    if (swapchain_texture != NULL) {
        Uniform_Block uniform_block{};
        uniform_block.mvp = state->proj * state->model;

        SDL_GPUColorTargetInfo color_target{};
        color_target.clear_color = {1.0f, 1.0f, 1.0f, 1.0f};
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.texture     = swapchain_texture;

        SDL_GPUBufferBinding vertex_binding{};
        vertex_binding.buffer = state->vertex_buffer;
        vertex_binding.offset = 0;

        SDL_GPUBufferBinding index_binding{};
        index_binding.buffer = state->index_buffer;
        index_binding.offset = 0;

        SDL_GPUTextureSamplerBinding texture_binding{};
        texture_binding.texture = state->texture;
        texture_binding.sampler = state->sampler;

        auto render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, NULL);

        SDL_BindGPUGraphicsPipeline(render_pass, state->graphics_pipeline);
        SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);
        SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_PushGPUVertexUniformData(command_buffer, 0, &uniform_block, sizeof(Uniform_Block));
        SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_binding, 1);
        SDL_DrawGPUIndexedPrimitives(render_pass, 6, 1, 0, 0, 0);

        SDL_EndGPURenderPass(render_pass);
    }

    ASSERT(SDL_SubmitGPUCommandBuffer(command_buffer));

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    auto state = static_cast<App_State *>(appstate);
    app_cleanup_time(state);
    app_cleanup_render_object(state); // Vertex buffer, index buffer, texture
    app_cleanup_graphics_pipeline(state);
    app_cleanup_gpu_device(state);
    app_cleanup_window(state);
}
