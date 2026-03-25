#pragma once

#include <array>
#include <cstdint>

#include "math_types.h"

constexpr std::uint32_t kMaxBladeCount = 131072;
constexpr std::uint32_t kBladeSegmentCount = 4;
constexpr std::uint32_t kVerticesPerBlade = kBladeSegmentCount * 6;
constexpr std::uint32_t kMaxRepulsors = 4;
constexpr std::uint32_t kOverlayBufferWidth = 512;
constexpr std::uint32_t kOverlayBufferHeight = 128;
constexpr std::uint32_t kOverlayPixelCount = kOverlayBufferWidth * kOverlayBufferHeight;

struct alignas(16) GrassBladeGpu
{
    Float4 rootHeight;
    Float4 params;
};

struct alignas(16) RepulsorGpu
{
    Float4 centerRadius;
    Float4 velocityStrength;
};

struct alignas(16) SceneUniforms
{
    Mat4 viewProjection;
    Float4 cameraPositionTime;
    Float4 sunDirectionIntensity;
    Float4 sunColorAmbient;
    Float4 windA;
    Float4 windB;
    Float4 windC;
    Float4 grassShape;
    Float4 grassMotion;
    Float4 grassColorBase;
    Float4 grassColorTip;
    Float4 groundColor;
    Float4 repulsorLightInfo;
    UInt4 counts;
};

struct alignas(16) PresentUniforms
{
    Float4 overlayInfo;
    Float4 windowInfo;
};

struct DemoSettings
{
    float renderScale = 0.8f;
    float grassDensity = 22.0f;
    float fieldExtent = 28.0f;
    float bladeHeight = 1.1f;
    float bladeWidth = 0.085f;
    float flex = 1.25f;
    float curvature = 1.15f;
    float rootStiffness = 0.18f;
    float staticLean = 0.07f;
    float windYawDegrees = 34.0f;
    float windStrength = 1.0f;
    float windTimeScale = 0.85f;
    float windNoiseScale = 0.075f;
    float windDetailNoiseScale = 0.20f;
    float windDetailStrength = 0.35f;
    float windWarpStrength = 0.80f;
    float windAdvection = 1.80f;
    float windCross = 0.16f;
    float windGust = 0.60f;
    float sunYawDegrees = 28.0f;
    float sunPitchDegrees = 42.0f;
    float sunIntensity = 0.10f;
    float ambient = 0.34f;
    float groundBrightness = 0.9f;
    float repulsorRadius = 2.2f;
    float repulsorStrength = 1.4f;
    float repulsorSpeed = 0.8f;
    bool repulsorLights = true;
    float repulsorLightStrength = 0.45f;
    float repulsorLightRadius = 2.4f;
    bool animateRepulsors = true;
    int repulsorCount = 3;
    bool showImGuiDemo = false;
    std::array<float, 3> grassBaseColor = {0.10f, 0.24f, 0.07f};
    std::array<float, 3> grassTipColor = {0.46f, 0.68f, 0.24f};
};

struct RepulsorState
{
    float orbitFactor = 0.0f;
    float orbitAngle = 0.0f;
    float speed = 0.0f;
    float height = 0.0f;
    Float3 center = {};
    Float3 velocity = {};
};

struct FrameState
{
    SceneUniforms scene = {};
    PresentUniforms present = {};
    std::array<RepulsorGpu, kMaxRepulsors> repulsors = {};
    std::uint32_t activeBladeCount = 0;
    std::uint32_t activeRepulsorCount = 0;
};
