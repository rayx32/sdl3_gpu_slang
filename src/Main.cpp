#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include "GraphicsDevice.h"
#include "SpriteBatch.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    // Initialize GraphicsDevice (creates window, initializes SDL3 GPU device and swapchain)
    GraphicsDevice graphics(800, 600, "SpriteBatch SDL3 GPU Demo");
    if (!graphics.IsValid()) {
        std::printf("Failed to initialize GraphicsDevice!\n");
        return 1;
    }

    std::printf("GPU Driver: %s\n", graphics.GetDriverName());

    // Create SpriteBatch with GraphicsDevice context
    SpriteBatch spriteBatch(graphics);

    // Load texture
    Texture2D whale = graphics.LoadTexture("assets/images/whale.png");
    SDL_assert(whale.IsValid());

    bool running = true;
    float rotation = 0.0f;
    Uint64 lastTime = SDL_GetTicks();

    const char* envFrames = SDL_getenv("DEMO_MAX_FRAMES");
    int maxFrames = envFrames ? std::atoi(envFrames) : -1;
    int frameCount = 0;

    while (running) {
        if (maxFrames > 0 && frameCount >= maxFrames) {
            break;
        }
        frameCount++;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        Uint64 currentTime = SDL_GetTicks();
        float delta = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        rotation += delta * 1.5f;

        // XNA-style frame render loop
        if (graphics.BeginFrame()) {
            // Clear screen (classic XNA Cornflower Blue)
            graphics.Clear(Color(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f));

            spriteBatch.Begin();

            // Sprite 1: Basic draw at position
            spriteBatch.Draw(whale, Vector2(50.0f, 50.0f), Color::White);

            // Sprite 2: Color tint (Red)
            spriteBatch.Draw(whale, Vector2(250.0f, 50.0f), Color::Red);

            // Sprite 3: Rotation around center origin
            Vector2 origin = { whale.width * 0.5f, whale.height * 0.5f };
            spriteBatch.Draw(whale, Vector2(550.0f, 150.0f), nullptr, Color::White, rotation, origin, 1.0f);

            // Sprite 4: Scaled & horizontally flipped
            spriteBatch.Draw(whale, Vector2(50.0f, 300.0f), nullptr, Color::Yellow, 0.0f, Vector2(0.0f, 0.0f), Vector2(1.5f, 0.8f), SpriteEffects::FlipHorizontally);

            // Sprite 5: Destination rectangle stretching
            Rectangle destRect = { 350.0f, 320.0f, 200.0f, 100.0f };
            spriteBatch.Draw(whale, destRect, Color(0.2f, 1.0f, 0.5f, 0.8f));

            spriteBatch.End();

            graphics.EndFrame();
        }
    }

    whale.Destroy();

    std::printf("Demo execution completed successfully (%d frames rendered).\n", frameCount);
    return 0;
}
