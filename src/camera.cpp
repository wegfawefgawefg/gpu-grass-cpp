#include "camera.h"

#include <algorithm>

#include <SDL3/SDL_scancode.h>

namespace
{
constexpr Float3 kWorldUp = {0.0f, 1.0f, 0.0f};
constexpr float kDegreesToRadians = 3.1415926535f / 180.0f;
} // namespace

Camera::Camera()
{
    position = {0.0f, 1.6f, 8.5f};
}

void Camera::UpdateLook(float deltaX, float deltaY)
{
    yawDegrees += deltaX * lookSensitivity;
    pitchDegrees -= deltaY * lookSensitivity;
    pitchDegrees = std::clamp(pitchDegrees, -89.0f, 89.0f);
}

bool Camera::UpdateMovement(const bool* keys, float deltaSeconds)
{
    const Float3 forward = GetForward();
    const Float3 right = GetRight();
    Float3 movement = {};

    if (keys[SDL_SCANCODE_W])
    {
        movement += forward;
    }
    if (keys[SDL_SCANCODE_S])
    {
        movement += forward * -1.0f;
    }
    if (keys[SDL_SCANCODE_D])
    {
        movement += right;
    }
    if (keys[SDL_SCANCODE_A])
    {
        movement += right * -1.0f;
    }
    if (keys[SDL_SCANCODE_SPACE])
    {
        movement += kWorldUp;
    }
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
    {
        movement += kWorldUp * -1.0f;
    }

    if (Length(movement) <= 0.0f)
    {
        return false;
    }

    position += Normalize(movement) * (moveSpeed * deltaSeconds);
    return true;
}

Mat4 Camera::BuildViewProjection(std::uint32_t width, std::uint32_t height) const
{
    const float aspect =
        static_cast<float>(std::max(width, 1u)) / static_cast<float>(std::max(height, 1u));
    const Mat4 projection =
        PerspectiveMatrix(verticalFovDegrees * kDegreesToRadians, aspect, 0.05f, 300.0f);
    const Float3 forward = GetForward();
    const Mat4 view = LookAtMatrix(position, position + forward, kWorldUp);
    return Multiply(projection, view);
}

Float3 Camera::GetForward() const
{
    const float yaw = yawDegrees * kDegreesToRadians;
    const float pitch = pitchDegrees * kDegreesToRadians;

    return Normalize({
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch),
    });
}

Float3 Camera::GetRight() const
{
    return Normalize(Cross(GetForward(), kWorldUp));
}
