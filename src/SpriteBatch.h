#pragma once

#include <SDL3/SDL_gpu.h>
#include <vector>
#include <cstdint>

// Forward declarations
struct SDL_Window;
class GraphicsDevice;

// --- XNA-like Math & Utility Types ---

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2() = default;
    constexpr Vector2(float x, float y) : x(x), y(y) {}

    constexpr Vector2 operator+(const Vector2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vector2 operator-(const Vector2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vector2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vector2 operator*(const Vector2& o) const { return {x * o.x, y * o.y}; }
};

struct Rectangle {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr Rectangle() = default;
    constexpr Rectangle(float x, float y, float width, float height)
        : x(x), y(y), width(width), height(height) {}
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    constexpr Color() = default;
    constexpr Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    static const Color White;
    static const Color Black;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Transparent;
};

struct Matrix4x4 {
    // 4x4 matrix stored in column-major order for shader compatibility
    float m[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    static Matrix4x4 Identity();
    static Matrix4x4 CreateOrthographicOffCenter(float left, float right, float bottom, float top, float zNear = 0.0f, float zFar = 1.0f);
    static Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b);
};

// --- XNA-like Enums ---

enum class BlendMode {
    AlphaBlend,
    Additive,
    Opaque,
    NonPremultiplied
};

enum class SamplerMode {
    LinearClamp,
    PointClamp
};

enum class SpriteSortMode {
    Deferred,   // Preserves call order; contiguous sprites sharing the same texture are batched
    Texture,    // Sorts commands by texture to minimize texture switches
    BackToFront,// Sorts by layerDepth descending (painters algorithm)
    FrontToBack // Sorts by layerDepth ascending
};

enum class SpriteEffects : uint8_t {
    None = 0,
    FlipHorizontally = 1 << 0,
    FlipVertically   = 1 << 1
};

inline SpriteEffects operator|(SpriteEffects a, SpriteEffects b) {
    return static_cast<SpriteEffects>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(SpriteEffects a, SpriteEffects b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

// --- Texture Wrapper ---

struct Texture2D {
    SDL_GPUDevice* device = nullptr;
    SDL_GPUTexture* handle = nullptr;
    int width = 0;
    int height = 0;

    bool IsValid() const { return handle != nullptr; }

    static Texture2D Load(SDL_GPUDevice* device, const char* filepath);
    void Destroy();
    void Destroy(SDL_GPUDevice* device);
};

// --- Vertex Structure (Matches Slang VertexInput) ---

struct SpriteVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

// --- Draw Command ---

struct DrawCommand {
    SDL_GPUTexture* texture = nullptr;
    int texWidth = 0;
    int texHeight = 0;
    Rectangle source;
    Rectangle destination;
    Color color;
    float rotation = 0.0f;
    Vector2 origin;
    Vector2 scale = {1.0f, 1.0f};
    SpriteEffects effects = SpriteEffects::None;
    float layerDepth = 0.0f;
};

// --- SpriteBatch Class ---

class SpriteBatch
{
public:
    SpriteBatch(GraphicsDevice& graphicsDevice, uint32_t maxSprites = 8192);
    SpriteBatch(SDL_GPUDevice* device, SDL_GPUTextureFormat targetFormat, uint32_t maxSprites = 8192);
    SpriteBatch(SDL_GPUDevice* device, SDL_Window* window, uint32_t maxSprites = 8192);
    ~SpriteBatch();

    // Non-copyable
    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // High-level XNA-style Begin (uses GraphicsDevice frame context)
    void Begin(SpriteSortMode sortMode = SpriteSortMode::Deferred,
               BlendMode blendMode = BlendMode::AlphaBlend,
               SamplerMode samplerMode = SamplerMode::LinearClamp,
               const Matrix4x4* transformMatrix = nullptr);

    // Explicit Begin with manual command buffer & viewport
    void Begin(SDL_GPUCommandBuffer* cmdBuffer,
               int viewportWidth,
               int viewportHeight,
               SpriteSortMode sortMode = SpriteSortMode::Deferred,
               BlendMode blendMode = BlendMode::AlphaBlend,
               SamplerMode samplerMode = SamplerMode::LinearClamp,
               const Matrix4x4* transformMatrix = nullptr);

    void Draw(const Texture2D& texture,
              Vector2 position,
              Color color = Color::White);

    void Draw(const Texture2D& texture,
              Vector2 position,
              const Rectangle* sourceRectangle,
              Color color = Color::White);

    void Draw(const Texture2D& texture,
              Rectangle destinationRectangle,
              Color color = Color::White);

    void Draw(const Texture2D& texture,
              Rectangle destinationRectangle,
              const Rectangle* sourceRectangle,
              Color color = Color::White);

    void Draw(const Texture2D& texture,
              Vector2 position,
              const Rectangle* sourceRectangle,
              Color color,
              float rotation,
              Vector2 origin,
              Vector2 scale,
              SpriteEffects effects = SpriteEffects::None,
              float layerDepth = 0.0f);

    void Draw(const Texture2D& texture,
              Vector2 position,
              const Rectangle* sourceRectangle,
              Color color,
              float rotation,
              Vector2 origin,
              float scale,
              SpriteEffects effects = SpriteEffects::None,
              float layerDepth = 0.0f);

    void Draw(const Texture2D& texture,
              Rectangle destinationRectangle,
              const Rectangle* sourceRectangle,
              Color color,
              float rotation,
              Vector2 origin,
              SpriteEffects effects = SpriteEffects::None,
              float layerDepth = 0.0f);

    // Flushes all batched sprites.
    // High-level XNA-style End (renders to GraphicsDevice backbuffer)
    void End();

    // Explicit End with custom render target
    void End(SDL_GPUTexture* renderTarget);

private:
    struct BatchRange {
        SDL_GPUTexture* texture;
        uint32_t firstIndex;
        uint32_t indexCount;
    };

    GraphicsDevice* m_graphicsDevice = nullptr;
    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUTextureFormat m_targetFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    uint32_t m_maxSprites = 0;

    // Pipelines for each BlendMode
    SDL_GPUShader* m_vertexShader = nullptr;
    SDL_GPUShader* m_fragmentShader = nullptr;
    SDL_GPUGraphicsPipeline* m_pipelineAlphaBlend = nullptr;
    SDL_GPUGraphicsPipeline* m_pipelineAdditive = nullptr;
    SDL_GPUGraphicsPipeline* m_pipelineOpaque = nullptr;

    // Samplers
    SDL_GPUSampler* m_samplerLinearClamp = nullptr;
    SDL_GPUSampler* m_samplerPointClamp = nullptr;

    // GPU Buffers
    SDL_GPUBuffer* m_indexBuffer = nullptr;
    SDL_GPUBuffer* m_vertexBuffer = nullptr;
    SDL_GPUTransferBuffer* m_vertexTransferBuffer = nullptr;

    // Active session state
    bool m_inBeginEnd = false;
    SDL_GPUCommandBuffer* m_commandBuffer = nullptr;
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    SpriteSortMode m_currentSortMode = SpriteSortMode::Deferred;
    BlendMode m_currentBlendMode = BlendMode::AlphaBlend;
    SamplerMode m_currentSamplerMode = SamplerMode::LinearClamp;
    Matrix4x4 m_transformMatrix;

    // Queued draw commands and vertex staging
    std::vector<DrawCommand> m_commands;
    std::vector<SpriteVertex> m_vertices;
    std::vector<BatchRange> m_batches;

    void InitShadersAndPipelines();
    void InitBuffers();
    void InitSamplers();
    void ReleaseResources();

    void Flush(SDL_GPUTexture* renderTarget);
};
