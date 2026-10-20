#include "GraphicsDevice.h"
#include <cstdio>

GraphicsDevice::GraphicsDevice(int width, int height, const char* title, SDL_WindowFlags windowFlags)
    : m_ownsWindow(true), m_width(width), m_height(height) {
    if (!SDL_getenv("SDL_VIDEO_DRIVER")) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11,wayland");
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        SDL_Log("GraphicsDevice: Failed to init SDL video: %s", SDL_GetError());
        return;
    }

    m_window = SDL_CreateWindow(title, width, height, windowFlags);
    if (!m_window) {
        SDL_Log("GraphicsDevice: Failed to create window: %s", SDL_GetError());
        return;
    }

    InitDevice();
}

GraphicsDevice::GraphicsDevice(SDL_Window* existingWindow)
    : m_window(existingWindow), m_ownsWindow(false) {
    if (m_window) {
        SDL_GetWindowSizeInPixels(m_window, &m_width, &m_height);
    }
    InitDevice();
}

GraphicsDevice::~GraphicsDevice() {
    ShutdownDevice();
}

void GraphicsDevice::InitDevice() {
    if (!m_window) return;

    m_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!m_device) {
        SDL_Log("GraphicsDevice: Failed to create GPU device: %s", SDL_GetError());
        return;
    }

    m_hasSwapchain = SDL_ClaimWindowForGPUDevice(m_device, m_window);
    if (m_hasSwapchain) {
        m_targetFormat = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
    } else {
        SDL_Log("GraphicsDevice: Window surface does not support presentation (offscreen fallback enabled).");
        m_targetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

        SDL_GPUTextureCreateInfo rtInfo = {};
        rtInfo.type = SDL_GPU_TEXTURETYPE_2D;
        rtInfo.format = m_targetFormat;
        rtInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        rtInfo.width = (Uint32)m_width;
        rtInfo.height = (Uint32)m_height;
        rtInfo.layer_count_or_depth = 1;
        rtInfo.num_levels = 1;
        m_offscreenTarget = SDL_CreateGPUTexture(m_device, &rtInfo);
    }
}

void GraphicsDevice::ShutdownDevice() {
    if (m_offscreenTarget) {
        SDL_ReleaseGPUTexture(m_device, m_offscreenTarget);
        m_offscreenTarget = nullptr;
    }

    if (m_hasSwapchain && m_window) {
        SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
        m_hasSwapchain = false;
    }

    if (m_device) {
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
    }

    if (m_ownsWindow && m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

const char* GraphicsDevice::GetDriverName() const {
    return m_device ? SDL_GetGPUDeviceDriver(m_device) : "None";
}

bool GraphicsDevice::BeginFrame() {
    if (!m_device) return false;

    m_currentCommandBuffer = SDL_AcquireGPUCommandBuffer(m_device);
    if (!m_currentCommandBuffer) return false;

    if (m_hasSwapchain) {
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_currentCommandBuffer, m_window, &m_currentBackbuffer, &m_backbufferWidth, &m_backbufferHeight) || !m_currentBackbuffer) {
            SDL_SubmitGPUCommandBuffer(m_currentCommandBuffer);
            m_currentCommandBuffer = nullptr;
            m_currentBackbuffer = nullptr;
            return false;
        }
    } else {
        m_currentBackbuffer = m_offscreenTarget;
        m_backbufferWidth = (Uint32)m_width;
        m_backbufferHeight = (Uint32)m_height;
    }

    return true;
}

void GraphicsDevice::Clear(const Color& color) {
    if (!m_currentCommandBuffer || !m_currentBackbuffer) return;

    SDL_GPUColorTargetInfo clearTarget = {};
    clearTarget.texture = m_currentBackbuffer;
    clearTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    clearTarget.clear_color = { color.r, color.g, color.b, color.a };
    clearTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* clearPass = SDL_BeginGPURenderPass(m_currentCommandBuffer, &clearTarget, 1, nullptr);
    if (clearPass) {
        SDL_EndGPURenderPass(clearPass);
    }
}

void GraphicsDevice::EndFrame() {
    if (m_currentCommandBuffer) {
        SDL_SubmitGPUCommandBuffer(m_currentCommandBuffer);
        m_currentCommandBuffer = nullptr;
        m_currentBackbuffer = nullptr;
    }
}

Texture2D GraphicsDevice::LoadTexture(const char* filepath) {
    return Texture2D::Load(m_device, filepath);
}

void GraphicsDevice::DestroyTexture(Texture2D& texture) {
    texture.Destroy();
}

