# gpu-grass-cpp

Standalone Vulkan grass-field demo built with SDL3, SDL3_ttf, and ImGui.

The current demo renders a lit procedural field of grass blades, drives wind with layered trigonometric noise, and animates moving repulsor objects that push the grass away as they travel across the plane. A small `SDL3_ttf` overlay shows live frame stats, while ImGui exposes the grass, wind, light, repulsor, and render-scale controls.

## Features

- Vulkan raster scene pass with offscreen render scaling
- Procedural instanced grass blades with camera-facing ribbons
- Real-time directional lighting for ground and grass
- Moving repulsors that bend nearby grass away from their path
- `SDL3_ttf` text overlay for frame timing and resolution info
- ImGui controls for render scale, grass shape, wind, light, and repulsors

## Build

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j4
./build/debug/gpu-grass-cpp
```

## Controls

- `Right Mouse`: hold to capture mouse and look around
- `W A S D`: move
- `Space` / `Shift`: move up / down
- `F1`: 1.0 render scale
- `F2`: 0.75 render scale
- `F3`: 0.5 render scale
- `Escape`: quit

## Notes

- The project vendors `SDL3`, `SDL3_ttf`, and `imgui` through CMake `FetchContent`.
- Shaders are compiled with `glslangValidator` during the build.
- The reference `gpu-raytracer` repo was only used as a Vulkan architecture reference. This repo is standalone.
