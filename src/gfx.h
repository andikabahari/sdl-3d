#pragma once

#include "defines.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Gfx_Upload_Buffer {
    SDL_GPUCommandBuffer *command_buffer;
    SDL_GPUCopyPass *copy_pass;
    SDL_GPUTransferBuffer *transfer_buffer;
    bool cyclic;
    bool using_new_copy_pass;
    u32 offset = 0;
};

struct Gfx_Context {
    SDL_Window *window;
    SDL_GPUDevice *device;
    SDL_GPUGraphicsPipeline *graphics_pipeline;
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
    Gfx_Upload_Buffer upload;

    glm::mat4 proj;
};

void gfx_init(Gfx_Context *context, SDL_Window *window);
void gfx_cleanup(Gfx_Context *context);
void gfx_draw(Gfx_Context *context, f32 rotate, SDL_FColor clear_color);

void gfx_immediate_upload_buffer_ex(Gfx_Context *context, u32 src_offset, SDL_GPUTransferBuffer *src_buffer, u32 size,
                                    u32 dst_offset, SDL_GPUBuffer *dst_buffer, bool cyclic);
void gfx_immediate_upload_buffer(Gfx_Context *context, SDL_GPUTransferBuffer *src_buffer, u32 size, SDL_GPUBuffer *dst_buffer);

void gfx_upload_buffer_begin(Gfx_Context *context, SDL_GPUCopyPass *copy_pass, SDL_GPUTransferBuffer *transfer_buffer, bool cyclic);
void gfx_upload_buffer_end(Gfx_Context *context);
void gfx_upload_buffer_push(Gfx_Context *context, u32 size, u32 offset, SDL_GPUBuffer *buffer);
