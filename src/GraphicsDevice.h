#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include "SpriteBatch.h"

class GraphicsDevice {
public:
    GraphicsDevice(int width = 800, int height = 600, const char* title = "Game", SDL_WindowFlags windowFlags = 0);
    GraphicsDevice(SDL_Window* existingWindow);
    ~GraphicsDevice();

    // Non-copyable
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    bool IsValid() const { return m_device != nullptr; }

    // --- Frame Lifecycle (XNA style) ---
    bool BeginFrame();
    void Clear(const Color& color = Color(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f));
    void EndFrame();

    // --- Resource Management Helpers ---
    Texture2D LoadTexture(const char* filepath);
    void DestroyTexture(Texture2D& texture);

    // --- Accessors ---
    SDL_GPUDevice* GetRawDevice() const { return m_device; }
    SDL_GPUCommandBuffer* GetCurrentCommandBuffer() const { return m_currentCommandBuffer; }
    SDL_GPUTexture* GetCurrentBackbuffer() const { return m_currentBackbuffer; }
    SDL_GPUTextureFormat GetTargetFormat() const { return m_targetFormat; }
    SDL_Window* GetWindow() const { return m_window; }
    const char* GetDriverName() const;

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetBackbufferWidth() const { return (int)m_backbufferWidth; }
    int GetBackbufferHeight() const { return (int)m_backbufferHeight; }

private:
    SDL_Window* m_window = nullptr;
    bool m_ownsWindow = false;
    int m_width = 800;
    int m_height = 600;

    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUTextureFormat m_targetFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    bool m_hasSwapchain = false;
    SDL_GPUTexture* m_offscreenTarget = nullptr;

    // Active frame state
    SDL_GPUCommandBuffer* m_currentCommandBuffer = nullptr;
    SDL_GPUTexture* m_currentBackbuffer = nullptr;
    Uint32 m_backbufferWidth = 0;
    Uint32 m_backbufferHeight = 0;

    void InitDevice();
    void ShutdownDevice();
};

