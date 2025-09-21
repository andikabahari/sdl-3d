#include "gfx.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

struct Uniform_Block {
    glm::mat4 mvp;
};

struct Vertex_Data {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 texcoord;
};

static void init_graphics_pipeline(Gfx_Context *context);
static void init_vertex_and_index_buffers(Gfx_Context *context, SDL_GPUCopyPass *copy_pass);
static void init_texture(Gfx_Context *context, SDL_GPUCopyPass *copy_pass);

void gfx_init(Gfx_Context *context, SDL_Window *window) {
    context->window = window;

    context->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    ASSERT(context->device != NULL);
    ASSERT(SDL_ClaimWindowForGPUDevice(context->device, context->window));

    init_graphics_pipeline(context);

    {
        auto command_buffer = SDL_AcquireGPUCommandBuffer(context->device);
        defer { ASSERT(SDL_SubmitGPUCommandBuffer(command_buffer)); };

        auto copy_pass = SDL_BeginGPUCopyPass(command_buffer);
        defer { SDL_EndGPUCopyPass(copy_pass); };

        init_vertex_and_index_buffers(context, copy_pass);
        init_texture(context, copy_pass);
    }

    int _w, _h;
    ASSERT(SDL_GetWindowSizeInPixels(context->window, &_w, &_h));
    f32 width  = static_cast<f32>(_w);
    f32 height = static_cast<f32>(_h);
    context->proj = glm::perspective(glm::radians(70.0f), width/height, 0.0001f, 1000.0f);

    #if 0
    SDL_GPUPresentMode present_mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(context->device, context->window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    } else if (SDL_WindowSupportsGPUPresentMode(context->device, context->window, SDL_GPU_PRESENTMODE_MAILBOX)) {
        present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    SDL_SetGPUSwapchainParameters(context->device, context->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode);
    #endif
}

void gfx_cleanup(Gfx_Context *context) {
    SDL_ReleaseGPUSampler(context->device, context->sampler);
    SDL_ReleaseGPUTexture(context->device, context->texture);
    SDL_ReleaseGPUBuffer(context->device, context->index_buffer);
    SDL_ReleaseGPUBuffer(context->device, context->vertex_buffer);
    SDL_ReleaseGPUGraphicsPipeline(context->device, context->graphics_pipeline);
    SDL_DestroyGPUDevice(context->device);

    context->window = NULL;
}

static void init_graphics_pipeline(Gfx_Context *context) {
    u64 vertex_code_size;
    auto vertex_code = SDL_LoadFile("res/shaders/basic.vert.spv", &vertex_code_size);
    defer { SDL_free(vertex_code); };

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
    auto vertex_shader = SDL_CreateGPUShader(context->device, &vertex_info);
    defer { SDL_ReleaseGPUShader(context->device, vertex_shader); };

    u64 fragment_code_size;
    auto fragment_code = SDL_LoadFile("res/shaders/basic.frag.spv", &fragment_code_size);
    defer { SDL_free(fragment_code); };

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
    auto fragment_shader = SDL_CreateGPUShader(context->device, &fragment_info);
    defer { SDL_ReleaseGPUShader(context->device, fragment_shader); };

    // Configure vertex input state.
    
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

    // Create graphics pipeline.

    SDL_GPUColorTargetDescription color_target_description{};
    color_target_description.format = SDL_GetGPUSwapchainTextureFormat(context->device, context->window);
    
    SDL_GPUGraphicsPipelineTargetInfo target_info{};
    target_info.color_target_descriptions = &color_target_description;
    target_info.num_color_targets = 1;
    
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.vertex_shader   = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.target_info     = target_info;
    pipeline_info.vertex_input_state = vertex_input_state;

    context->graphics_pipeline = SDL_CreateGPUGraphicsPipeline(context->device, &pipeline_info);
}

static void init_vertex_and_index_buffers(Gfx_Context *context, SDL_GPUCopyPass *copy_pass) {
    const auto white = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    Vertex_Data vertices[] = {
        {glm::vec3(-0.5f, -0.5f, 0.0f), white, glm::vec2(0.0f, 1.0f)}, // Bottom-left
        {glm::vec3( 0.5f, -0.5f, 0.0f), white, glm::vec2(1.0f, 1.0f)}, // Bottom-right
        {glm::vec3( 0.5f,  0.5f, 0.0f), white, glm::vec2(1.0f, 0.0f)}, // Top-right
        {glm::vec3(-0.5f,  0.5f, 0.0f), white, glm::vec2(0.0f, 0.0f)}, // Top-left
    };

    u16 vertex_indices[] = {
        0, 1, 2,
        2, 3, 0,
    };

    SDL_GPUBufferCreateInfo vertex_info{};
    vertex_info.size  = sizeof(vertices);
    vertex_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    context->vertex_buffer = SDL_CreateGPUBuffer(context->device, &vertex_info);

    SDL_GPUBufferCreateInfo index_info{};
    index_info.size  = sizeof(vertex_indices);
    index_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    context->index_buffer = SDL_CreateGPUBuffer(context->device, &index_info);

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.size  = vertex_info.size + index_info.size;
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    auto transfer_buffer = SDL_CreateGPUTransferBuffer(context->device, &transfer_info);

    auto data = static_cast<u8 *>(SDL_MapGPUTransferBuffer(context->device, transfer_buffer, false));
    u32 offset = 0;
    SDL_memcpy(data + offset, vertices, vertex_info.size);
    offset += vertex_info.size;
    SDL_memcpy(data + offset, vertex_indices, index_info.size);
    offset += index_info.size;
    SDL_UnmapGPUTransferBuffer(context->device, transfer_buffer);

    gfx_upload_buffer_begin(context, copy_pass, transfer_buffer, false);
    gfx_upload_buffer_push(context, vertex_info.size, 0, context->vertex_buffer);
    gfx_upload_buffer_push(context, index_info.size, 0, context->index_buffer);
    gfx_upload_buffer_end(context);

    SDL_ReleaseGPUTransferBuffer(context->device, transfer_buffer);
}

static void init_texture(Gfx_Context *context, SDL_GPUCopyPass *copy_pass) {
    s32 img_width  = 0;
    s32 img_height = 0;
    auto img_data  = stbi_load("res/images/Sample.png", &img_width, &img_height, NULL, 4);
    ASSERT(img_data != NULL);
    defer { stbi_image_free(img_data); };

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.size  = static_cast<u32>(img_width * img_height * 4);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    auto transfer_buffer = SDL_CreateGPUTransferBuffer(context->device, &transfer_info);
    defer { SDL_ReleaseGPUTransferBuffer(context->device, transfer_buffer); };

    {
        auto data = SDL_MapGPUTransferBuffer(context->device, transfer_buffer, false);
        memcpy(data, img_data, transfer_info.size);
        SDL_UnmapGPUTransferBuffer(context->device, transfer_buffer);
    }

    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type   = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage  = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width  = static_cast<u32>(img_width);
    texture_info.height = static_cast<u32>(img_height);
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    context->texture = SDL_CreateGPUTexture(context->device, &texture_info);

    SDL_GPUSamplerCreateInfo sampler_info{};
    sampler_info.min_filter  = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter  = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    context->sampler = SDL_CreateGPUSampler(context->device, &sampler_info);

    {
        SDL_GPUTextureTransferInfo copy_src{};
        copy_src.transfer_buffer = transfer_buffer;
        copy_src.offset = 0;

        SDL_GPUTextureRegion copy_dst{};
        copy_dst.texture = context->texture;
        copy_dst.w = static_cast<u32>(img_width);
        copy_dst.h = static_cast<u32>(img_height);
        copy_dst.d = 1;

        SDL_UploadToGPUTexture(copy_pass, &copy_src, &copy_dst, false);
    }

}

void gfx_draw(Gfx_Context *context, f32 rotate, SDL_FColor clear_color) {
    auto command_buffer = SDL_AcquireGPUCommandBuffer(context->device);
    defer { ASSERT(SDL_SubmitGPUCommandBuffer(command_buffer)); };

    SDL_GPUTexture *swapchain_texture;
    u32 swapchain_width;
    u32 swapchain_height;

    ASSERT(SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, context->window,
                                                 &swapchain_texture, &swapchain_width, &swapchain_height));

    SDL_GPUColorTargetInfo color_target{};
    color_target.clear_color = clear_color;
    color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op    = SDL_GPU_STOREOP_STORE;
    color_target.texture     = swapchain_texture;

    if (swapchain_texture != NULL) {
        auto render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, NULL);
        defer { SDL_EndGPURenderPass(render_pass); };

        SDL_BindGPUGraphicsPipeline(render_pass, context->graphics_pipeline);

        SDL_GPUBufferBinding vertex_binding{};
        vertex_binding.buffer = context->vertex_buffer;
        vertex_binding.offset = 0;
        SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

        SDL_GPUBufferBinding index_binding{};
        index_binding.buffer = context->index_buffer;
        index_binding.offset = 0;
        SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        {
            auto model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, -5.0f));
            model = glm::rotate(model, rotate, glm::vec3(0.0f, 1.0f, 0.0f));

            Uniform_Block uniform_block{};
            uniform_block.mvp = context->proj * model;
            SDL_PushGPUVertexUniformData(command_buffer, 0, &uniform_block, sizeof(Uniform_Block));
        }

        SDL_GPUTextureSamplerBinding texture_binding{};
        texture_binding.texture = context->texture;
        texture_binding.sampler = context->sampler;
        SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_binding, 1);

        SDL_DrawGPUIndexedPrimitives(render_pass, 6, 1, 0, 0, 0);
    }
}


void gfx_immediate_upload_buffer_ex(Gfx_Context *context, u32 src_offset, SDL_GPUTransferBuffer *src_buffer, u32 size,
                                    u32 dst_offset, SDL_GPUBuffer *dst_buffer, bool cyclic) {
    auto command_buffer = SDL_AcquireGPUCommandBuffer(context->device);
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

void gfx_immediate_upload_buffer(Gfx_Context *context, SDL_GPUTransferBuffer *src_buffer, u32 size, SDL_GPUBuffer *dst_buffer) {
    gfx_immediate_upload_buffer_ex(context, 0, src_buffer, size, 0, dst_buffer, false);
}

void gfx_upload_buffer_begin(Gfx_Context *context, SDL_GPUCopyPass *copy_pass, SDL_GPUTransferBuffer *transfer_buffer, bool cyclic) {
    Gfx_Upload_Buffer *upload = &context->upload;

    if (copy_pass != NULL) {
        upload->copy_pass = copy_pass;
        upload->using_new_copy_pass = false;
    } else {
        upload->command_buffer = SDL_AcquireGPUCommandBuffer(context->device);
        upload->copy_pass = SDL_BeginGPUCopyPass(upload ->command_buffer);
        upload->using_new_copy_pass = true;
    }
    upload->transfer_buffer = transfer_buffer;
    upload->cyclic = cyclic;
}

void gfx_upload_buffer_end(Gfx_Context *context) {
    Gfx_Upload_Buffer *upload = &context->upload;

    if (upload->using_new_copy_pass) {
        SDL_EndGPUCopyPass(upload->copy_pass);
        ASSERT(SDL_SubmitGPUCommandBuffer(upload->command_buffer));
    }
}

void gfx_upload_buffer_push(Gfx_Context *context, u32 size, u32 offset, SDL_GPUBuffer *buffer) {
    Gfx_Upload_Buffer *upload = &context->upload;

    SDL_GPUTransferBufferLocation copy_src{};
    copy_src.transfer_buffer = upload->transfer_buffer;
    copy_src.offset = upload->offset;

    SDL_GPUBufferRegion copy_dst{};
    copy_dst.buffer = buffer;
    copy_dst.offset = offset;
    copy_dst.size = size;

    SDL_UploadToGPUBuffer(upload->copy_pass, &copy_src, &copy_dst, upload->cyclic);

    upload->offset += size;
}

void gfx_model_load(Gfx_Model *model, const char *file) {
    *model = {};

    cgltf_options options{};
    cgltf_data *data = NULL;

    cgltf_result result = cgltf_parse_file(&options, file, &data);
    if (result != cgltf_result_success) return;
    defer { cgltf_free(data); };

    result = cgltf_load_buffers(&options, data, file);
    if (result != cgltf_result_success) {
        cgltf_free(data);
        return;
    }

    int prim_count = 0;
    for (cgltf_size ni = 0; ni < data->nodes_count; ni++) {
        cgltf_node *node = &(data->nodes[ni]);
        cgltf_mesh *mesh = node->mesh;
        if (!mesh) continue;

        for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
            // Only support primitive triangles.
            if (mesh->primitives[pi].type == cgltf_primitive_type_triangles) prim_count++;
        }
    }

    // Get mesh count.
    int mesh_count = prim_count;
    auto meshes = cast(Gfx_Mesh *)SDL_calloc(cast(size_t)mesh_count, sizeof(Gfx_Mesh));
    if (!meshes) {
        cgltf_free(data);
        return;
    }

    defer {
        model->mesh_count = mesh_count;
        model->meshes = meshes;
    };

    // Get mesh data.
    ssize mesh_index = 0;
    for (cgltf_size ni = 0; ni < data->nodes_count; ni++) {
        cgltf_node *node = &(data->nodes[ni]);
        cgltf_mesh *mesh = node->mesh;
        if (!mesh) continue;

        // TODO: get matrix transform.

        for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
            cgltf_primitive *prim = &(mesh->primitives[pi]);

            // Only support primitive triangles.
            if (prim->type != cgltf_primitive_type_triangles) continue;

            defer { mesh_index++; };

            //
            // Load following attributes:
            // - Vertices
            // - Normals
            // - Tangents
            // - Texcoords
            // - Texcoords2
            // - Colors
            //
            for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
                cgltf_attribute *attribute = &(prim->attributes[ai]);
                cgltf_accessor *accessor = attribute->data;

                cgltf_size floats_needed = cgltf_accessor_unpack_floats(accessor, NULL, 0);
                if (floats_needed == 0) continue;

                if (attribute->type == cgltf_attribute_type_position) { // Vertices.
                    meshes[mesh_index].vertex_count = cast(int)accessor->count;
                    meshes[mesh_index].vertices = cast(f32 *)SDL_malloc(floats_needed * sizeof(f32));
                    cgltf_accessor_unpack_floats(accessor, meshes[mesh_index].vertices, floats_needed);
                } else if (attribute->type == cgltf_attribute_type_normal) {
                    meshes[mesh_index].normals = cast(f32 *)SDL_malloc(floats_needed * sizeof(f32));
                    cgltf_accessor_unpack_floats(accessor, meshes[mesh_index].normals, floats_needed);
                } else if (attribute->type == cgltf_attribute_type_tangent) {
                    // TODO: normal attribute.
                } else if (attribute->type == cgltf_attribute_type_texcoord) {
                    if (attribute->index == 0) {
                        meshes[mesh_index].texcoords = cast(f32 *)SDL_malloc(floats_needed * sizeof(f32));
                        cgltf_accessor_unpack_floats(accessor, meshes[mesh_index].texcoords, floats_needed);
                    } else if (attribute->index == 1) {
                        meshes[mesh_index].texcoords2 = cast(f32 *)SDL_malloc(floats_needed * sizeof(f32));
                        cgltf_accessor_unpack_floats(accessor, meshes[mesh_index].texcoords2, floats_needed);
                    }
                } else if (attribute->type == cgltf_attribute_type_color) {
                    // TODO: color attribute.
                }
            }

            //
            // Load primitive indices data.
            //
            {
                cgltf_accessor *accessor = prim->indices;

                cgltf_size triangle_count = accessor->count / 3;
                meshes[mesh_index].triangle_count = triangle_count;

                cgltf_size alloc_size = accessor->count * sizeof(u16);
                meshes[mesh_index].indices = cast(u16 *)SDL_malloc(alloc_size);
                cgltf_accessor_unpack_indices(accessor, meshes[mesh_index].indices, alloc_size, triangle_count);
            }
        }
    }
}

void gfx_model_cleanup(Gfx_Model *model) {
    for (int i = 0; i < model->mesh_count; i++) {
        Gfx_Mesh mesh = model->meshes[i];
        if (mesh.vertices != NULL)   SDL_free(mesh.vertices);
        if (mesh.texcoords != NULL)  SDL_free(mesh.texcoords);
        if (mesh.texcoords2 != NULL) SDL_free(mesh.texcoords2);
        if (mesh.normals != NULL)    SDL_free(mesh.normals);
        if (mesh.indices != NULL)    SDL_free(mesh.indices);
    }
    SDL_free(model->meshes);
    *model = {};
}
