#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "camera.h"
#include "demo_types.h"
#include "text_overlay.h"
#include "vulkan_renderer.h"

struct App
{
    ~App();

    void Run();

    void Initialize();
    void Shutdown();
    void HandleEvent(const SDL_Event& event);
    void Update(float deltaSeconds);
    void UpdateOverlayText();
    void UpdateRepulsors(float deltaSeconds);
    void BuildUi();
    void SyncRendererSize();
    FrameState BuildFrameState() const;

    SDL_Window* m_window = nullptr;
    bool m_running = true;
    bool m_captureMouse = false;
    std::uint32_t m_windowWidth = 0;
    std::uint32_t m_windowHeight = 0;
    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;
    float m_smoothedFps = 0.0f;
    float m_overlayRefreshSeconds = 0.0f;
    float m_elapsedSeconds = 0.0f;
    bool m_sizeDirty = true;

    Camera m_camera;
    DemoSettings m_settings;
    std::string m_settingsStatus;
    TextOverlay m_overlay;
    VulkanRenderer m_renderer;
    std::vector<GrassBladeGpu> m_blades;
    std::array<RepulsorState, kMaxRepulsors> m_repulsors = {};
};
