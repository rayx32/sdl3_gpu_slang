#include "SpriteBatch.h"
#include "GraphicsDevice.h"
#include "SpriteShaders.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

// --- Static Color Constants ---

const Color Color::White       = {1.0f, 1.0f, 1.0f, 1.0f};
const Color Color::Black       = {0.0f, 0.0f, 0.0f, 1.0f};
const Color Color::Red         = {1.0f, 0.0f, 0.0f, 1.0f};
const Color Color::Green       = {0.0f, 1.0f, 0.0f, 1.0f};
const Color Color::Blue        = {0.0f, 0.0f, 1.0f, 1.0f};
const Color Color::Yellow      = {1.0f, 1.0f, 0.0f, 1.0f};
const Color Color::Transparent = {0.0f, 0.0f, 0.0f, 0.0f};

// --- Matrix4x4 Implementation ---

Matrix4x4 Matrix4x4::Identity() {
    return Matrix4x4{};
}

Matrix4x4 Matrix4x4::CreateOrthographicOffCenter(float left, float right, float bottom, float top, float zNear, float zFar) {
    Matrix4x4 result = {};
    // Column-major representation for SDL3 GPU / Slang
    // In SDL3 GPU NDC: top is +1.0, bottom is -1.0, left is -1.0, right is +1.0
    // Screen coords: top is 0, bottom is height, left is 0, right is width
    result.m[0]  = 2.0f / (right - left);
    result.m[5]  = 2.0f / (top - bottom); // top=0, bottom=h => 2.0 / -h = -2.0 / h
    result.m[10] = 1.0f / (zFar - zNear);
    result.m[12] = -(right + left) / (right - left);
    result.m[13] = -(bottom + top) / (bottom - top); // -(h + 0) / (h - 0) = -1.0 -> but top is 0, so -(h) / (h) = -1? Wait, (bottom+top)/(bottom-top) = +1.0
    result.m[13] = 1.0f; // Directly map top (y=0) to +1.0
    result.m[14] = -zNear / (zFar - zNear);
    result.m[15] = 1.0f;
    return result;
}

Matrix4x4 Matrix4x4::Multiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 r = {};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

// --- Texture2D Implementation ---

Texture2D Texture2D::Load(SDL_GPUDevice* device, const char* filepath) {
    Texture2D tex = {};
    if (!device || !filepath) return tex;

    SDL_Surface* surface = IMG_Load(filepath);
    if (!surface) {
        SDL_Log("SpriteBatch: Failed to load image '%s': %s", filepath, SDL_GetError());
        return tex;
    }

    SDL_Surface* rgbaSurface = surface;
    bool converted = false;
    if (surface->format != SDL_PIXELFORMAT_RGBA32) {
        rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        converted = true;
    }

    if (!rgbaSurface) {
        SDL_Log("SpriteBatch: Failed to convert surface to RGBA32: %s", SDL_GetError());
        SDL_DestroySurface(surface);
        return tex;
    }

    tex.device = device;
    tex.width = rgbaSurface->w;
    tex.height = rgbaSurface->h;

    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texInfo.width = (Uint32)tex.width;
    texInfo.height = (Uint32)tex.height;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;

    tex.handle = SDL_CreateGPUTexture(device, &texInfo);
    if (!tex.handle) {
        SDL_Log("SpriteBatch: Failed to create GPU texture: %s", SDL_GetError());
        if (converted) SDL_DestroySurface(rgbaSurface);
        SDL_DestroySurface(surface);
        return tex;
    }

    Uint32 dataSize = (Uint32)(tex.width * tex.height * 4);
    SDL_GPUTransferBufferCreateInfo tbInfo = {};
    tbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size = dataSize;

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbInfo);
    if (transferBuffer) {
        void* mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        if (mapped) {
            std::memcpy(mapped, rgbaSurface->pixels, dataSize);
            SDL_UnmapGPUTransferBuffer(device, transferBuffer);

            SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
            if (cmd) {
                SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
                if (copy) {
                    SDL_GPUTextureTransferInfo transferInfo = {};
                    transferInfo.transfer_buffer = transferBuffer;
                    transferInfo.offset = 0;
                    transferInfo.pixels_per_row = (Uint32)tex.width;
                    transferInfo.rows_per_layer = (Uint32)tex.height;

                    SDL_GPUTextureRegion dstRegion = {};
                    dstRegion.texture = tex.handle;
                    dstRegion.w = (Uint32)tex.width;
                    dstRegion.h = (Uint32)tex.height;
                    dstRegion.d = 1;

                    SDL_UploadToGPUTexture(copy, &transferInfo, &dstRegion, false);
                    SDL_EndGPUCopyPass(copy);
                }
                SDL_SubmitGPUCommandBuffer(cmd);
            }
        }
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    }

    if (converted) SDL_DestroySurface(rgbaSurface);
    SDL_DestroySurface(surface);

    return tex;
}

void Texture2D::Destroy() {
    Destroy(device);
}

void Texture2D::Destroy(SDL_GPUDevice* inDevice) {
    SDL_GPUDevice* dev = inDevice ? inDevice : device;
    if (dev && handle) {
        SDL_ReleaseGPUTexture(dev, handle);
        handle = nullptr;
        width = 0;
        height = 0;
        device = nullptr;
    }
}

// --- SpriteBatch Implementation ---

SpriteBatch::SpriteBatch(GraphicsDevice& graphicsDevice, uint32_t maxSprites)
    : m_graphicsDevice(&graphicsDevice),
      m_device(graphicsDevice.GetRawDevice()),
      m_targetFormat(graphicsDevice.GetTargetFormat()),
      m_maxSprites(maxSprites) {
    InitShadersAndPipelines();
    InitBuffers();
    InitSamplers();
}

SpriteBatch::SpriteBatch(SDL_GPUDevice* device, SDL_GPUTextureFormat targetFormat, uint32_t maxSprites)
    : m_device(device), m_targetFormat(targetFormat), m_maxSprites(maxSprites) {
    InitShadersAndPipelines();
    InitBuffers();
    InitSamplers();
}

SpriteBatch::SpriteBatch(SDL_GPUDevice* device, SDL_Window* window, uint32_t maxSprites)
    : m_device(device),
      m_targetFormat(SDL_GetGPUSwapchainTextureFormat(device, window)),
      m_maxSprites(maxSprites) {
    InitShadersAndPipelines();
    InitBuffers();
    InitSamplers();
}

SpriteBatch::~SpriteBatch() {
    ReleaseResources();
}

void SpriteBatch::InitShadersAndPipelines() {
    // 1. Create Vertex Shader (SPIR-V compiled from Slang)
    SDL_GPUShaderCreateInfo vsInfo = {};
    vsInfo.code = SpriteShaders::SpriteVertexShaderSPIRV;
    vsInfo.code_size = SpriteShaders::SpriteVertexShaderSPIRV_Size;
    vsInfo.entrypoint = "main";
    vsInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    vsInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vsInfo.num_uniform_buffers = 1; // set 1, binding 0

    m_vertexShader = SDL_CreateGPUShader(m_device, &vsInfo);
    if (!m_vertexShader) {
        SDL_Log("SpriteBatch: Failed to create vertex shader: %s", SDL_GetError());
    }

    // 2. Create Fragment Shader (SPIR-V compiled from Slang)
    SDL_GPUShaderCreateInfo fsInfo = {};
    fsInfo.code = SpriteShaders::SpriteFragmentShaderSPIRV;
    fsInfo.code_size = SpriteShaders::SpriteFragmentShaderSPIRV_Size;
    fsInfo.entrypoint = "main";
    fsInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    fsInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fsInfo.num_samplers = 1; // set 2, binding 0

    m_fragmentShader = SDL_CreateGPUShader(m_device, &fsInfo);
    if (!m_fragmentShader) {
        SDL_Log("SpriteBatch: Failed to create fragment shader: %s", SDL_GetError());
    }

    // 3. Define Vertex Layout
    SDL_GPUVertexBufferDescription vbDesc = {};
    vbDesc.slot = 0;
    vbDesc.pitch = sizeof(SpriteVertex);
    vbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attributes[3] = {};
    // location 0: position (float2)
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(SpriteVertex, x);

    // location 1: uv (float2)
    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(SpriteVertex, u);

    // location 2: color (float4)
    attributes[2].location = 2;
    attributes[2].buffer_slot = 0;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[2].offset = offsetof(SpriteVertex, r);

    // Common Pipeline Base Info
    SDL_GPUGraphicsPipelineCreateInfo baseInfo = {};
    baseInfo.vertex_shader = m_vertexShader;
    baseInfo.fragment_shader = m_fragmentShader;
    baseInfo.vertex_input_state.vertex_buffer_descriptions = &vbDesc;
    baseInfo.vertex_input_state.num_vertex_buffers = 1;
    baseInfo.vertex_input_state.vertex_attributes = attributes;
    baseInfo.vertex_input_state.num_vertex_attributes = 3;
    baseInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    baseInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    baseInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    baseInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    baseInfo.target_info.num_color_targets = 1;

    // --- Pipeline 1: AlphaBlend (Standard Non-Premultiplied Alpha) ---
    {
        SDL_GPUColorTargetDescription targetDesc = {};
        targetDesc.format = m_targetFormat;
        targetDesc.blend_state.enable_blend = true;
        targetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        targetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        targetDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        targetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        targetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        targetDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        baseInfo.target_info.color_target_descriptions = &targetDesc;
        m_pipelineAlphaBlend = SDL_CreateGPUGraphicsPipeline(m_device, &baseInfo);
    }

    // --- Pipeline 2: Additive Blend ---
    {
        SDL_GPUColorTargetDescription targetDesc = {};
        targetDesc.format = m_targetFormat;
        targetDesc.blend_state.enable_blend = true;
        targetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        targetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        targetDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        targetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        targetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        targetDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        baseInfo.target_info.color_target_descriptions = &targetDesc;
        m_pipelineAdditive = SDL_CreateGPUGraphicsPipeline(m_device, &baseInfo);
    }

    // --- Pipeline 3: Opaque ---
    {
        SDL_GPUColorTargetDescription targetDesc = {};
        targetDesc.format = m_targetFormat;
        targetDesc.blend_state.enable_blend = false;

        baseInfo.target_info.color_target_descriptions = &targetDesc;
        m_pipelineOpaque = SDL_CreateGPUGraphicsPipeline(m_device, &baseInfo);
    }
}

void SpriteBatch::InitBuffers() {
    uint32_t indexCount = m_maxSprites * 6;
    uint32_t indexBufferSize = indexCount * sizeof(uint16_t);

    // Create Index Buffer (16-bit indices)
    SDL_GPUBufferCreateInfo ibInfo = {};
    ibInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ibInfo.size = indexBufferSize;
    m_indexBuffer = SDL_CreateGPUBuffer(m_device, &ibInfo);

    // Pre-populate Static Quad Indices (0, 1, 2, 2, 3, 0 pattern)
    SDL_GPUTransferBufferCreateInfo ibTransferInfo = {};
    ibTransferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ibTransferInfo.size = indexBufferSize;
    SDL_GPUTransferBuffer* ibTransfer = SDL_CreateGPUTransferBuffer(m_device, &ibTransferInfo);

    if (ibTransfer && m_indexBuffer) {
        uint16_t* indices = (uint16_t*)SDL_MapGPUTransferBuffer(m_device, ibTransfer, false);
        if (indices) {
            for (uint32_t i = 0; i < m_maxSprites; ++i) {
                uint16_t base = (uint16_t)(i * 4);
                indices[i * 6 + 0] = base + 0;
                indices[i * 6 + 1] = base + 1;
                indices[i * 6 + 2] = base + 2;
                indices[i * 6 + 3] = base + 2;
                indices[i * 6 + 4] = base + 3;
                indices[i * 6 + 5] = base + 0;
            }
            SDL_UnmapGPUTransferBuffer(m_device, ibTransfer);

            SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
            if (cmd) {
                SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
                if (copy) {
                    SDL_GPUTransferBufferLocation src = { ibTransfer, 0 };
                    SDL_GPUBufferRegion dst = { m_indexBuffer, 0, indexBufferSize };
                    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
                    SDL_EndGPUCopyPass(copy);
                }
                SDL_SubmitGPUCommandBuffer(cmd);
            }
        }
        SDL_ReleaseGPUTransferBuffer(m_device, ibTransfer);
    }

    // Dynamic Vertex Buffer & Staging Transfer Buffer
    uint32_t vertexBufferSize = m_maxSprites * 4 * sizeof(SpriteVertex);

    SDL_GPUBufferCreateInfo vbInfo = {};
    vbInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbInfo.size = vertexBufferSize;
    m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &vbInfo);

    SDL_GPUTransferBufferCreateInfo vbTransferInfo = {};
    vbTransferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    vbTransferInfo.size = vertexBufferSize;
    m_vertexTransferBuffer = SDL_CreateGPUTransferBuffer(m_device, &vbTransferInfo);

    m_commands.reserve(m_maxSprites);
    m_vertices.reserve(m_maxSprites * 4);
}

void SpriteBatch::InitSamplers() {
    // LinearClamp
    SDL_GPUSamplerCreateInfo linearInfo = {};
    linearInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    linearInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    linearInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    linearInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linearInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linearInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_samplerLinearClamp = SDL_CreateGPUSampler(m_device, &linearInfo);

    // PointClamp (for crisp pixel art)
    SDL_GPUSamplerCreateInfo pointInfo = {};
    pointInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    pointInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    pointInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    pointInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    pointInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    pointInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_samplerPointClamp = SDL_CreateGPUSampler(m_device, &pointInfo);
}

void SpriteBatch::ReleaseResources() {
    if (m_pipelineAlphaBlend) SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipelineAlphaBlend);
    if (m_pipelineAdditive) SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipelineAdditive);
    if (m_pipelineOpaque) SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipelineOpaque);

    if (m_vertexShader) SDL_ReleaseGPUShader(m_device, m_vertexShader);
    if (m_fragmentShader) SDL_ReleaseGPUShader(m_device, m_fragmentShader);

    if (m_samplerLinearClamp) SDL_ReleaseGPUSampler(m_device, m_samplerLinearClamp);
    if (m_samplerPointClamp) SDL_ReleaseGPUSampler(m_device, m_samplerPointClamp);

    if (m_vertexBuffer) SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
    if (m_indexBuffer) SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
    if (m_vertexTransferBuffer) SDL_ReleaseGPUTransferBuffer(m_device, m_vertexTransferBuffer);
}

void SpriteBatch::Begin(SpriteSortMode sortMode,
                        BlendMode blendMode,
                        SamplerMode samplerMode,
                        const Matrix4x4* transformMatrix) {
    if (!m_graphicsDevice) {
        SDL_Log("SpriteBatch: Begin() called without GraphicsDevice context! Pass cmdBuffer and viewport.");
        return;
    }
    Begin(m_graphicsDevice->GetCurrentCommandBuffer(),
          m_graphicsDevice->GetBackbufferWidth(),
          m_graphicsDevice->GetBackbufferHeight(),
          sortMode, blendMode, samplerMode, transformMatrix);
}

void SpriteBatch::Begin(SDL_GPUCommandBuffer* cmdBuffer,
                        int viewportWidth,
                        int viewportHeight,
                        SpriteSortMode sortMode,
                        BlendMode blendMode,
                        SamplerMode samplerMode,
                        const Matrix4x4* transformMatrix) {
    m_inBeginEnd = true;
    m_commandBuffer = cmdBuffer;
    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
    m_currentSortMode = sortMode;
    m_currentBlendMode = blendMode;
    m_currentSamplerMode = samplerMode;

    Matrix4x4 proj = Matrix4x4::CreateOrthographicOffCenter(0.0f, (float)viewportWidth, (float)viewportHeight, 0.0f);
    if (transformMatrix) {
        m_transformMatrix = Matrix4x4::Multiply(proj, *transformMatrix);
    } else {
        m_transformMatrix = proj;
    }

    m_commands.clear();
    m_vertices.clear();
    m_batches.clear();
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, Color color) {
    Draw(texture, position, nullptr, color, 0.0f, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture, Vector2 position, const Rectangle* sourceRectangle, Color color) {
    Draw(texture, position, sourceRectangle, color, 0.0f, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture, Rectangle destinationRectangle, Color color) {
    Draw(texture, destinationRectangle, nullptr, color, 0.0f, Vector2(0.0f, 0.0f), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture, Rectangle destinationRectangle, const Rectangle* sourceRectangle, Color color) {
    Draw(texture, destinationRectangle, sourceRectangle, color, 0.0f, Vector2(0.0f, 0.0f), SpriteEffects::None, 0.0f);
}

void SpriteBatch::Draw(const Texture2D& texture,
                       Vector2 position,
                       const Rectangle* sourceRectangle,
                       Color color,
                       float rotation,
                       Vector2 origin,
                       float scale,
                       SpriteEffects effects,
                       float layerDepth) {
    Draw(texture, position, sourceRectangle, color, rotation, origin, Vector2(scale, scale), effects, layerDepth);
}

void SpriteBatch::Draw(const Texture2D& texture,
                       Vector2 position,
                       const Rectangle* sourceRectangle,
                       Color color,
                       float rotation,
                       Vector2 origin,
                       Vector2 scale,
                       SpriteEffects effects,
                       float layerDepth) {
    if (!texture.IsValid() || m_commands.size() >= m_maxSprites) return;

    Rectangle src = sourceRectangle ? *sourceRectangle : Rectangle{0.0f, 0.0f, (float)texture.width, (float)texture.height};
    Rectangle dst = {position.x, position.y, src.width * scale.x, src.height * scale.y};

    DrawCommand cmd;
    cmd.texture = texture.handle;
    cmd.texWidth = texture.width;
    cmd.texHeight = texture.height;
    cmd.source = src;
    cmd.destination = dst;
    cmd.color = color;
    cmd.rotation = rotation;
    cmd.origin = origin;
    cmd.scale = {1.0f, 1.0f}; // already folded into dst dimensions
    cmd.effects = effects;
    cmd.layerDepth = layerDepth;

    m_commands.push_back(cmd);
}

void SpriteBatch::Draw(const Texture2D& texture,
                       Rectangle destinationRectangle,
                       const Rectangle* sourceRectangle,
                       Color color,
                       float rotation,
                       Vector2 origin,
                       SpriteEffects effects,
                       float layerDepth) {
    if (!texture.IsValid() || m_commands.size() >= m_maxSprites) return;

    Rectangle src = sourceRectangle ? *sourceRectangle : Rectangle{0.0f, 0.0f, (float)texture.width, (float)texture.height};

    DrawCommand cmd;
    cmd.texture = texture.handle;
    cmd.texWidth = texture.width;
    cmd.texHeight = texture.height;
    cmd.source = src;
    cmd.destination = destinationRectangle;
    cmd.color = color;
    cmd.rotation = rotation;
    cmd.origin = origin;
    cmd.scale = {1.0f, 1.0f};
    cmd.effects = effects;
    cmd.layerDepth = layerDepth;

    m_commands.push_back(cmd);
}

void SpriteBatch::End() {
    if (!m_graphicsDevice) {
        SDL_Log("SpriteBatch: End() called without GraphicsDevice context! Pass renderTarget.");
        return;
    }
    End(m_graphicsDevice->GetCurrentBackbuffer());
}

void SpriteBatch::End(SDL_GPUTexture* renderTarget) {
    if (!m_inBeginEnd) return;
    m_inBeginEnd = false;

    Flush(renderTarget);
}

void SpriteBatch::Flush(SDL_GPUTexture* renderTarget) {
    if (m_commands.empty() || !m_commandBuffer || !renderTarget) return;

    // 1. Sort commands based on SpriteSortMode
    switch (m_currentSortMode) {
        case SpriteSortMode::Texture:
            std::stable_sort(m_commands.begin(), m_commands.end(), [](const DrawCommand& a, const DrawCommand& b) {
                return a.texture < b.texture;
            });
            break;
        case SpriteSortMode::BackToFront:
            std::stable_sort(m_commands.begin(), m_commands.end(), [](const DrawCommand& a, const DrawCommand& b) {
                return a.layerDepth > b.layerDepth; // Higher layer depth rendered first
            });
            break;
        case SpriteSortMode::FrontToBack:
            std::stable_sort(m_commands.begin(), m_commands.end(), [](const DrawCommand& a, const DrawCommand& b) {
                return a.layerDepth < b.layerDepth; // Lower layer depth rendered first
            });
            break;
        case SpriteSortMode::Deferred:
        default:
            // Keep draw call order
            break;
    }

    // 2. Generate Quad Vertices
    m_vertices.clear();
    m_batches.clear();

    SDL_GPUTexture* currentTexture = nullptr;
    uint32_t currentBatchStartIndex = 0;
    uint32_t currentBatchIndexCount = 0;

    for (const auto& cmd : m_commands) {
        if (cmd.texture != currentTexture) {
            if (currentBatchIndexCount > 0) {
                m_batches.push_back({currentTexture, currentBatchStartIndex, currentBatchIndexCount});
            }
            currentTexture = cmd.texture;
            currentBatchStartIndex = (uint32_t)m_vertices.size() / 4 * 6;
            currentBatchIndexCount = 0;
        }

        // Texture UVs (normalized [0, 1])
        float tw = (cmd.texWidth > 0) ? (float)cmd.texWidth : 1.0f;
        float th = (cmd.texHeight > 0) ? (float)cmd.texHeight : 1.0f;

        float u0 = cmd.source.x / tw;
        float v0 = cmd.source.y / th;
        float u1 = (cmd.source.x + cmd.source.width) / tw;
        float v1 = (cmd.source.y + cmd.source.height) / th;

        // Apply SpriteEffects
        if (cmd.effects & SpriteEffects::FlipHorizontally) {
            std::swap(u0, u1);
        }
        if (cmd.effects & SpriteEffects::FlipVertically) {
            std::swap(v0, v1);
        }

        // Local corners relative to origin
        float ox = cmd.origin.x;
        float oy = cmd.origin.y;
        float w = cmd.destination.width;
        float h = cmd.destination.height;

        float c0x = -ox,     c0y = -oy;
        float c1x = w - ox,  c1y = -oy;
        float c2x = w - ox,  c2y = h - oy;
        float c3x = -ox,     c3y = h - oy;

        float x0, y0, x1, y1, x2, y2, x3, y3;

        if (cmd.rotation == 0.0f) {
            x0 = cmd.destination.x + c0x; y0 = cmd.destination.y + c0y;
            x1 = cmd.destination.x + c1x; y1 = cmd.destination.y + c1y;
            x2 = cmd.destination.x + c2x; y2 = cmd.destination.y + c2y;
            x3 = cmd.destination.x + c3x; y3 = cmd.destination.y + c3y;
        } else {
            float cosR = std::cos(cmd.rotation);
            float sinR = std::sin(cmd.rotation);

            x0 = cmd.destination.x + (c0x * cosR - c0y * sinR);
            y0 = cmd.destination.y + (c0x * sinR + c0y * cosR);

            x1 = cmd.destination.x + (c1x * cosR - c1y * sinR);
            y1 = cmd.destination.y + (c1x * sinR + c1y * cosR);

            x2 = cmd.destination.x + (c2x * cosR - c2y * sinR);
            y2 = cmd.destination.y + (c2x * sinR + c2y * cosR);

            x3 = cmd.destination.x + (c3x * cosR - c3y * sinR);
            y3 = cmd.destination.y + (c3x * sinR + c3y * cosR);
        }

        // Quad Vertices: 0=Top-Left, 1=Top-Right, 2=Bottom-Right, 3=Bottom-Left
        m_vertices.push_back({x0, y0, u0, v0, cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a});
        m_vertices.push_back({x1, y1, u1, v0, cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a});
        m_vertices.push_back({x2, y2, u1, v1, cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a});
        m_vertices.push_back({x3, y3, u0, v1, cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a});

        currentBatchIndexCount += 6;
    }

    if (currentBatchIndexCount > 0) {
        m_batches.push_back({currentTexture, currentBatchStartIndex, currentBatchIndexCount});
    }

    // 3. Upload Vertex Data via Copy Pass
    uint32_t uploadBytes = (uint32_t)(m_vertices.size() * sizeof(SpriteVertex));
    void* mapped = SDL_MapGPUTransferBuffer(m_device, m_vertexTransferBuffer, true);
    if (mapped) {
        std::memcpy(mapped, m_vertices.data(), uploadBytes);
        SDL_UnmapGPUTransferBuffer(m_device, m_vertexTransferBuffer);

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(m_commandBuffer);
        if (copyPass) {
            SDL_GPUTransferBufferLocation src = { m_vertexTransferBuffer, 0 };
            SDL_GPUBufferRegion dst = { m_vertexBuffer, 0, uploadBytes };
            SDL_UploadToGPUBuffer(copyPass, &src, &dst, true);
            SDL_EndGPUCopyPass(copyPass);
        }
    }

    // 4. Render Batches via Render Pass
    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = renderTarget;
    colorTarget.load_op = SDL_GPU_LOADOP_LOAD; // Preserve existing frame contents
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(m_commandBuffer, &colorTarget, 1, nullptr);
    if (renderPass) {
        // Select pipeline based on BlendMode
        SDL_GPUGraphicsPipeline* pipeline = m_pipelineAlphaBlend;
        switch (m_currentBlendMode) {
            case BlendMode::Additive: pipeline = m_pipelineAdditive; break;
            case BlendMode::Opaque:   pipeline = m_pipelineOpaque;   break;
            case BlendMode::AlphaBlend:
            case BlendMode::NonPremultiplied:
            default:                  pipeline = m_pipelineAlphaBlend; break;
        }

        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

        // Bind Vertex & Index Buffers
        SDL_GPUBufferBinding vbBinding = { m_vertexBuffer, 0 };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);

        SDL_GPUBufferBinding ibBinding = { m_indexBuffer, 0 };
        SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        // Push Projection / Transform Matrix Uniform (set 1, slot 0)
        SDL_PushGPUVertexUniformData(m_commandBuffer, 0, m_transformMatrix.m, sizeof(m_transformMatrix.m));

        // Select Sampler
        SDL_GPUSampler* sampler = (m_currentSamplerMode == SamplerMode::PointClamp)
                                      ? m_samplerPointClamp
                                      : m_samplerLinearClamp;

        // Draw Batches
        for (const auto& batch : m_batches) {
            if (!batch.texture) continue;

            SDL_GPUTextureSamplerBinding binding = { batch.texture, sampler };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &binding, 1);

            SDL_DrawGPUIndexedPrimitives(renderPass, batch.indexCount, 1, batch.firstIndex, 0, 0);
        }

        SDL_EndGPURenderPass(renderPass);
    }

    m_commands.clear();
    m_vertices.clear();
    m_batches.clear();
}
