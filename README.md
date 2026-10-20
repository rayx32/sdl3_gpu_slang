# SpriteBatch SDL3 GPU Demo

An XNA / MonoGame-inspired 2D **SpriteBatch** renderer built on top of the modern **SDL3 GPU API** and **C++23**, featuring SPIR-V shaders compiled from [Slang](https://shader-slang.com/).

---

## Features

- **SDL3 GPU Backend**: Built on the next-generation SDL3 GPU abstraction layer (`SDL_GPUDevice`), supporting modern graphics backends (Vulkan, Direct3D 12, Metal).
- **XNA-Style SpriteBatch**:
  - Flexible drawing overloads (position, destination rectangles, source cropping, color tints, origins, rotation, scaling, and layer depth).
  - Sprite flipping effects (`FlipHorizontally`, `FlipVertically`).
  - Sort modes: `Deferred`, `Texture`, `BackToFront`, and `FrontToBack`.
  - Blend states: `AlphaBlend`, `Additive`, `Opaque`, and `NonPremultiplied`.
  - Sampler states: `LinearClamp` and `PointClamp`.
  - Dynamic GPU buffer batching and draw-call merging to minimize state switches.
- **Slang Shader Pipeline**:
  - Shader source authored in `src/shaders/sprite.slang`.
  - Cross-platform build automation compiles shaders to SPIR-V binaries and embeds them directly into C++ bytecode (`SpriteShaders.h`).
  - Precompiled shader fallback included, allowing builds even without the Slang compiler installed.

---

## Dependencies

### Required
- **C++23 Compiler**: GCC 13+, Clang 16+, or MSVC 2022 (v17.5+).
- **CMake**: Version 3.26 or newer.
- **SDL3**: Core library (`sdl3`).
- **SDL3_image**: Image loading library (`sdl3-image`).
- **SDL3_ttf**: Font rendering library (`sdl3-ttf`).
- **GPU Driver**: Vulkan, Direct3D 12, or Metal supported hardware/drivers.
- **Slang Compiler (`slangc`)**: Included in LunarG Vulkan SDK 1.3.290+ or downloadable from [Slang Releases](https://github.com/shader-slang/slang/releases). Only needed if you modify `src/shaders/sprite.slang`.

---

## Installing Dependencies

### Linux

#### Ubuntu / Debian (24.10+)
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
    libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev
```

*(Note: On older Ubuntu/Debian releases where SDL3 packages are not yet in the default repos, install SDL3 via [official releases](https://github.com/libsdl-org/SDL/releases) or build from source).*

#### Arch Linux
```bash
sudo pacman -S base-devel cmake pkgconf \
    sdl3 sdl3_image sdl3_ttf
```

### macOS (Homebrew)
```bash
brew install cmake sdl3 sdl3_image sdl3_ttf
```

### Windows (vcpkg)
```cmd
vcpkg install sdl3:x64-windows sdl3-image:x64-windows sdl3-ttf:x64-windows
```

---

## Building the Project

### Linux & macOS

```bash
# 1. Configure the build directory
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. Build the executable
cmake --build build -j
```

### Windows (Visual Studio / MSVC)

```cmd
# 1. Configure using vcpkg toolchain (adjust vcpkg path if necessary)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 2. Build Release configuration
cmake --build build --config Release
```

---

## Running the Demo

Run the compiled executable from the project root or directly from the build directory (assets are automatically synchronized into the build directory):

### Linux & macOS
```bash
./build/game
```

### Windows
```cmd
.\build\Release\game.exe
```

### Environment Variables
- `DEMO_MAX_FRAMES=<number>`: Automatically closes the demo after rendering a specific number of frames (useful for headless testing and CI benchmarking).
  ```bash
  DEMO_MAX_FRAMES=300 ./build/game
  ```

---

## Shader Compilation

The project comes with precompiled shader bytecode checked in at `src/SpriteShaders.h`, allowing instant compilation without requiring `slangc`.

If you have the Vulkan SDK or Slang compiler installed and wish to modify the shader:

1. **Automatic recompilation**: Modifying `src/shaders/sprite.slang` will automatically trigger `slangc` and update `src/SpriteShaders.h` on your next `cmake --build build`.
2. **Manual recompilation**: Force recompile the shaders and regenerate headers at any time:
   ```bash
   cmake --build build --target shaders
   ```
3. **Custom `slangc` Path**: If `slangc` is not in your system `PATH` or standard Vulkan SDK path:
   ```bash
   cmake -B build -DSLANGC_EXECUTABLE=/path/to/slangc
   ```

---

## Project Structure

```text
├── assets/
│   ├── images/
│   │   └── whale.png          # Demo texture
│   └── shaders/
│       ├── sprite_vs.spv      # Compiled vertex shader SPIR-V
│       └── sprite_fs.spv      # Compiled fragment shader SPIR-V
├── cmake/
│   └── GenerateShaderHeader.cmake # Cross-platform CMake script to convert SPIR-V to C++
├── src/
│   ├── GraphicsDevice.h       # SDL3 GPU device and swapchain manager
│   ├── GraphicsDevice.cpp
│   ├── SpriteBatch.h          # XNA-style SpriteBatch renderer & math utilities
│   ├── SpriteBatch.cpp
│   ├── SpriteShaders.h        # Embedded C++ bytecode for shaders
│   ├── main.cpp               # Demo application entry point
│   └── shaders/
│       └── sprite.slang       # Slang shader source (HLSL-like)
├── CMakeLists.txt             # Primary CMake configuration
└── README.md                  # Project documentation
```

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

