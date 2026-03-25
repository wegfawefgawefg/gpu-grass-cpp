#include "app.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string_view>

#include <imgui.h>

#include "grass_field.h"

namespace
{
constexpr std::string_view kWindowTitle = "gpu-grass-cpp";
constexpr std::string_view kUiFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
constexpr int kInitialWindowWidth = 1600;
constexpr int kInitialWindowHeight = 900;
constexpr std::string_view kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";
constexpr float kOverlayRefreshPeriod = 0.12f;
constexpr float kDegreesToRadians = 3.1415926535f / 180.0f;

void LogSdlError(std::string_view context)
{
    SDL_Log("%s: %s", context.data(), SDL_GetError());
}

void CenterWindowOnPrimaryDisplay(SDL_Window* window)
{
    const SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    if (primaryDisplay == 0)
    {
        return;
    }

    SDL_Rect bounds;
    if (!SDL_GetDisplayBounds(primaryDisplay, &bounds))
    {
        return;
    }

    const int centeredX = bounds.x + (bounds.w - kInitialWindowWidth) / 2;
    const int centeredY = bounds.y + (bounds.h - kInitialWindowHeight) / 2;
    SDL_SetWindowPosition(window, centeredX, centeredY);
}

Float3 BuildSunDirection(float yawDegrees, float pitchDegrees)
{
    const float yaw = yawDegrees * kDegreesToRadians;
    const float pitch = pitchDegrees * kDegreesToRadians;
    return Normalize({
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch),
    });
}

Float3 BuildHorizontalDirection(float yawDegrees)
{
    const float yaw = yawDegrees * kDegreesToRadians;
    return Normalize({std::cos(yaw), 0.0f, std::sin(yaw)});
}
} // namespace

App::~App()
{
    Shutdown();
}

void App::Run()
{
    Initialize();

    auto previousTime = std::chrono::steady_clock::now();
    while (m_running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            HandleEvent(event);
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const std::chrono::duration<float> delta = currentTime - previousTime;
        previousTime = currentTime;

        const float deltaSeconds = delta.count();
        Update(deltaSeconds);

        if (deltaSeconds > 0.0f)
        {
            const float instantFps = 1.0f / deltaSeconds;
            if (m_smoothedFps <= 0.0f)
            {
                m_smoothedFps = instantFps;
            }
            else
            {
                m_smoothedFps = m_smoothedFps * 0.96f + instantFps * 0.04f;
            }
        }

        m_overlayRefreshSeconds -= deltaSeconds;
        if (m_overlayRefreshSeconds <= 0.0f)
        {
            UpdateOverlayText();
            m_overlayRefreshSeconds = kOverlayRefreshPeriod;
        }

        if (m_sizeDirty)
        {
            SyncRendererSize();
            m_sizeDirty = false;
        }

        m_renderer.BeginImGuiFrame();
        BuildUi();
        if (m_settings.showImGuiDemo)
        {
            ImGui::ShowDemoWindow(&m_settings.showImGuiDemo);
        }
        ImGui::Render();

        m_renderer.Render(BuildFrameState(), m_overlay.Pixels());
    }
}

void App::Initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error("SDL_Init failed");
    }

    SDL_SetHint(SDL_HINT_X11_WINDOW_TYPE, kX11DialogWindowType.data());

    m_window = SDL_CreateWindow(
        kWindowTitle.data(),
        kInitialWindowWidth,
        kInitialWindowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (m_window == nullptr)
    {
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    CenterWindowOnPrimaryDisplay(m_window);

    m_overlay.Initialize(kUiFontPath, 22.0f);
    m_blades = BuildGrassField(kMaxBladeCount);
    m_renderer.Initialize(m_window, m_blades);

    m_repulsors[0] = {.orbitRadius = 4.0f, .orbitAngle = 0.0f, .speed = 0.78f, .height = 0.55f};
    m_repulsors[1] = {.orbitRadius = 7.0f, .orbitAngle = 1.8f, .speed = -0.56f, .height = 0.45f};
    m_repulsors[2] = {.orbitRadius = 10.5f, .orbitAngle = -1.2f, .speed = 0.43f, .height = 0.65f};
    m_repulsors[3] = {.orbitRadius = 13.0f, .orbitAngle = 2.7f, .speed = -0.34f, .height = 0.50f};

    UpdateRepulsors(0.0f);
    UpdateOverlayText();
    SyncRendererSize();
}

void App::Shutdown()
{
    m_renderer.Shutdown();
    m_overlay.Shutdown();

    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void App::HandleEvent(const SDL_Event& event)
{
    m_renderer.ProcessImGuiEvent(event);

    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;

    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE)
        {
            m_running = false;
        }
        if (event.key.key == SDLK_F1)
        {
            m_settings.renderScale = 1.0f;
            m_sizeDirty = true;
        }
        if (event.key.key == SDLK_F2)
        {
            m_settings.renderScale = 0.75f;
            m_sizeDirty = true;
        }
        if (event.key.key == SDLK_F3)
        {
            m_settings.renderScale = 0.5f;
            m_sizeDirty = true;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_RIGHT && !m_renderer.WantsMouseCapture())
        {
            m_captureMouse = true;
            SDL_SetWindowRelativeMouseMode(m_window, true);
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            m_captureMouse = false;
            SDL_SetWindowRelativeMouseMode(m_window, false);
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (m_captureMouse)
        {
            m_camera.UpdateLook(event.motion.xrel, event.motion.yrel);
        }
        break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        m_sizeDirty = true;
        break;

    default:
        break;
    }
}

void App::Update(float deltaSeconds)
{
    m_elapsedSeconds += deltaSeconds;
    UpdateRepulsors(deltaSeconds);

    if (m_renderer.WantsKeyboardCapture())
    {
        return;
    }

    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys == nullptr)
    {
        LogSdlError("SDL_GetKeyboardState failed");
        return;
    }

    m_camera.UpdateMovement(keys, deltaSeconds);
}

void App::UpdateOverlayText()
{
    const float msPerFrame = m_smoothedFps > 0.0f ? 1000.0f / m_smoothedFps : 0.0f;
    char text[256] = {};
    std::snprintf(
        text,
        sizeof(text),
        "%.2f ms   %.0f fps   %ux%u -> %ux%u   blades %u   repulsors %d",
        msPerFrame,
        m_smoothedFps,
        m_renderWidth,
        m_renderHeight,
        m_windowWidth,
        m_windowHeight,
        m_settings.bladeCount,
        m_settings.repulsorCount
    );
    m_overlay.Update(text);
}

void App::UpdateRepulsors(float deltaSeconds)
{
    for (std::size_t index = 0; index < m_repulsors.size(); ++index)
    {
        RepulsorState& repulsor = m_repulsors[index];
        const Float3 previous = repulsor.center;

        if (m_settings.animateRepulsors)
        {
            repulsor.orbitAngle += repulsor.speed * m_settings.repulsorSpeed * deltaSeconds;
        }

        const float angle = repulsor.orbitAngle + static_cast<float>(index) * 0.7f;
        repulsor.center = {
            std::cos(angle) * repulsor.orbitRadius,
            repulsor.height,
            std::sin(angle * 1.15f) * repulsor.orbitRadius * 0.65f,
        };
        if (deltaSeconds > 0.0f)
        {
            repulsor.velocity = (repulsor.center - previous) / deltaSeconds;
        }
        else
        {
            repulsor.velocity = {};
        }
    }
}

void App::BuildUi()
{
    ImGui::SetNextWindowPos(ImVec2(16.0f, 52.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 520.0f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Grass Controls"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::SliderFloat("Render Scale", &m_settings.renderScale, 0.35f, 1.0f, "%.2f"))
    {
        m_sizeDirty = true;
    }

    int bladeCount = static_cast<int>(m_settings.bladeCount);
    if (ImGui::SliderInt("Blade Count", &bladeCount, 12000, static_cast<int>(kMaxBladeCount)))
    {
        m_settings.bladeCount = static_cast<std::uint32_t>(bladeCount);
    }

    ImGui::SliderFloat("Field Extent", &m_settings.fieldExtent, 12.0f, 50.0f, "%.1f m");
    ImGui::SliderFloat("Blade Height", &m_settings.bladeHeight, 0.35f, 2.4f, "%.2f");
    ImGui::SliderFloat("Blade Width", &m_settings.bladeWidth, 0.02f, 0.20f, "%.3f");
    ImGui::SliderFloat("Flex", &m_settings.flex, 0.2f, 2.5f, "%.2f");
    ImGui::SliderFloat("Curvature", &m_settings.curvature, 0.2f, 2.5f, "%.2f");
    ImGui::SliderFloat("Root Stiffness", &m_settings.rootStiffness, 0.0f, 0.65f, "%.2f");
    ImGui::SliderFloat("Static Lean", &m_settings.staticLean, 0.0f, 0.18f, "%.2f");

    ImGui::SeparatorText("Wind");
    ImGui::SliderFloat("Wind Yaw", &m_settings.windYawDegrees, -180.0f, 180.0f, "%.0f deg");
    ImGui::SliderFloat("Wind Strength", &m_settings.windStrength, 0.0f, 2.5f, "%.2f");
    ImGui::SliderFloat("Wind Time", &m_settings.windTimeScale, 0.1f, 2.5f, "%.2f");
    ImGui::SliderFloat("Noise Scale", &m_settings.windNoiseScale, 0.01f, 0.35f, "%.3f");
    ImGui::SliderFloat(
        "Detail Scale",
        &m_settings.windDetailNoiseScale,
        0.04f,
        0.9f,
        "%.3f"
    );
    ImGui::SliderFloat("Detail Strength", &m_settings.windDetailStrength, 0.0f, 1.2f, "%.2f");
    ImGui::SliderFloat("Crosswind", &m_settings.windCross, 0.0f, 0.8f, "%.2f");
    ImGui::SliderFloat("Wind Gust", &m_settings.windGust, 0.0f, 1.5f, "%.2f");

    ImGui::SeparatorText("Light");
    ImGui::SliderFloat("Sun Yaw", &m_settings.sunYawDegrees, -180.0f, 180.0f, "%.0f deg");
    ImGui::SliderFloat("Sun Pitch", &m_settings.sunPitchDegrees, 5.0f, 85.0f, "%.0f deg");
    ImGui::SliderFloat("Sun Intensity", &m_settings.sunIntensity, 0.1f, 3.0f, "%.2f");
    ImGui::SliderFloat("Ambient", &m_settings.ambient, 0.02f, 0.6f, "%.2f");
    ImGui::SliderFloat("Ground Brightness", &m_settings.groundBrightness, 0.4f, 1.4f, "%.2f");

    ImGui::SeparatorText("Repulsors");
    ImGui::Checkbox("Animate Repulsors", &m_settings.animateRepulsors);
    ImGui::SliderInt("Repulsor Count", &m_settings.repulsorCount, 0, static_cast<int>(kMaxRepulsors));
    ImGui::SliderFloat("Repulsor Radius", &m_settings.repulsorRadius, 0.3f, 5.0f, "%.2f");
    ImGui::SliderFloat("Repulsor Strength", &m_settings.repulsorStrength, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Repulsor Speed", &m_settings.repulsorSpeed, 0.0f, 2.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("Blade segments fixed at %u for now.", kBladeSegmentCount);
    ImGui::Checkbox("ImGui Demo", &m_settings.showImGuiDemo);
    ImGui::Text("Hold RMB to look around.");
    ImGui::Text("WASD + Space/Shift move.");

    ImGui::End();
}

void App::SyncRendererSize()
{
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(m_window, &pixelWidth, &pixelHeight))
    {
        LogSdlError("SDL_GetWindowSizeInPixels failed");
        return;
    }

    m_windowWidth = static_cast<std::uint32_t>(std::max(pixelWidth, 1));
    m_windowHeight = static_cast<std::uint32_t>(std::max(pixelHeight, 1));
    const float clampedScale = std::clamp(m_settings.renderScale, 0.35f, 1.0f);
    m_renderWidth =
        std::max(1u, static_cast<std::uint32_t>(static_cast<float>(m_windowWidth) * clampedScale));
    m_renderHeight = std::max(
        1u,
        static_cast<std::uint32_t>(static_cast<float>(m_windowHeight) * clampedScale)
    );
    m_renderer.Resize(m_windowWidth, m_windowHeight, m_renderWidth, m_renderHeight);
}

FrameState App::BuildFrameState() const
{
    FrameState frame = {};
    frame.activeBladeCount = std::min(m_settings.bladeCount, kMaxBladeCount);
    frame.activeRepulsorCount =
        static_cast<std::uint32_t>(std::clamp(m_settings.repulsorCount, 0, static_cast<int>(kMaxRepulsors)));

    frame.scene.viewProjection = m_camera.BuildViewProjection(m_renderWidth, m_renderHeight);
    frame.scene.cameraPositionTime = ToFloat4(m_camera.position, m_elapsedSeconds);

    const Float3 sunDirection = BuildSunDirection(m_settings.sunYawDegrees, m_settings.sunPitchDegrees);
    const Float3 windDirection = BuildHorizontalDirection(m_settings.windYawDegrees);
    frame.scene.sunDirectionIntensity = ToFloat4(sunDirection, m_settings.sunIntensity);
    frame.scene.sunColorAmbient = {1.0f, 0.95f, 0.86f, m_settings.ambient};
    frame.scene.windA = {
        windDirection.x,
        windDirection.z,
        m_settings.windStrength,
        m_settings.windTimeScale,
    };
    frame.scene.windB = {
        m_settings.windNoiseScale,
        m_settings.windDetailNoiseScale,
        m_settings.windCross,
        m_settings.fieldExtent,
    };
    frame.scene.grassShape = {
        m_settings.bladeHeight,
        m_settings.bladeWidth,
        m_settings.flex,
        m_settings.curvature,
    };
    frame.scene.grassMotion = {
        m_settings.rootStiffness,
        m_settings.windGust,
        m_settings.windDetailStrength,
        m_settings.staticLean,
    };
    frame.scene.grassColorBase = {0.08f, 0.22f, 0.05f, 0.0f};
    frame.scene.grassColorTip = {0.58f, 0.82f, 0.29f, 0.55f};
    frame.scene.groundColor = {
        0.11f * m_settings.groundBrightness,
        0.18f * m_settings.groundBrightness,
        0.09f * m_settings.groundBrightness,
        1.0f,
    };
    frame.scene.counts = {
        frame.activeBladeCount,
        frame.activeRepulsorCount,
        m_renderWidth,
        m_renderHeight,
    };

    frame.present.overlayInfo = {
        static_cast<float>(m_overlay.Width()),
        static_cast<float>(m_overlay.Height()),
        18.0f,
        16.0f,
    };
    frame.present.windowInfo = {
        static_cast<float>(m_windowWidth),
        static_cast<float>(m_windowHeight),
        0.0f,
        0.0f,
    };

    for (std::uint32_t index = 0; index < frame.activeRepulsorCount; ++index)
    {
        const RepulsorState& state = m_repulsors[index];
        frame.repulsors[index] = {
            .centerRadius = {state.center.x, state.center.y, state.center.z, m_settings.repulsorRadius},
            .velocityStrength = {
                state.velocity.x,
                state.velocity.y,
                state.velocity.z,
                m_settings.repulsorStrength,
            },
        };
    }

    return frame;
}
