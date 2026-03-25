#pragma once

#include <cstdint>

#include "math_types.h"

struct Camera
{
    Camera();

    void UpdateLook(float deltaX, float deltaY);
    bool UpdateMovement(const bool* keys, float deltaSeconds);
    Mat4 BuildViewProjection(std::uint32_t width, std::uint32_t height) const;
    Float3 GetForward() const;
    Float3 GetRight() const;

    Float3 position = {};
    float yawDegrees = -90.0f;
    float pitchDegrees = -8.0f;
    float verticalFovDegrees = 55.0f;
    float moveSpeed = 7.0f;
    float lookSensitivity = 0.12f;
};
