#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct Float2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Float3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct alignas(16) Float4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct alignas(16) UInt4
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
    std::uint32_t w = 0;
};

struct alignas(16) Mat4
{
    std::array<float, 16> m = {};
};

inline Float3 operator+(const Float3& a, const Float3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Float3 operator-(const Float3& a, const Float3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Float3 operator*(const Float3& v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

inline Float3 operator*(float s, const Float3& v)
{
    return v * s;
}

inline Float3 operator/(const Float3& v, float s)
{
    return {v.x / s, v.y / s, v.z / s};
}

inline Float3& operator+=(Float3& a, const Float3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline float Dot(const Float3& a, const Float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Float3 Cross(const Float3& a, const Float3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float Length(const Float3& v)
{
    return std::sqrt(Dot(v, v));
}

inline Float3 Normalize(const Float3& v)
{
    const float length = Length(v);
    if (length <= 0.0f)
    {
        return {0.0f, 0.0f, 0.0f};
    }

    return v / length;
}

inline Float3 Lerp(const Float3& a, const Float3& b, float t)
{
    return a * (1.0f - t) + b * t;
}

inline Float4 ToFloat4(const Float3& v, float w = 0.0f)
{
    return {v.x, v.y, v.z, w};
}

inline Mat4 IdentityMatrix()
{
    Mat4 result;
    result.m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    return result;
}

inline Mat4 Multiply(const Mat4& a, const Mat4& b)
{
    Mat4 result;
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sum += a.m[k * 4 + row] * b.m[column * 4 + k];
            }
            result.m[column * 4 + row] = sum;
        }
    }
    return result;
}

inline Mat4 PerspectiveMatrix(float verticalFovRadians, float aspect, float nearPlane, float farPlane)
{
    Mat4 result = {};
    const float inverseTan = 1.0f / std::tan(verticalFovRadians * 0.5f);
    result.m[0] = inverseTan / std::max(aspect, 0.001f);
    result.m[5] = -inverseTan;
    result.m[10] = farPlane / (nearPlane - farPlane);
    result.m[11] = -1.0f;
    result.m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    return result;
}

inline Mat4 LookAtMatrix(const Float3& eye, const Float3& center, const Float3& up)
{
    const Float3 forward = Normalize(center - eye);
    const Float3 right = Normalize(Cross(forward, up));
    const Float3 cameraUp = Cross(right, forward);

    Mat4 result = IdentityMatrix();
    result.m[0] = right.x;
    result.m[1] = cameraUp.x;
    result.m[2] = -forward.x;
    result.m[4] = right.y;
    result.m[5] = cameraUp.y;
    result.m[6] = -forward.y;
    result.m[8] = right.z;
    result.m[9] = cameraUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -Dot(right, eye);
    result.m[13] = -Dot(cameraUp, eye);
    result.m[14] = Dot(forward, eye);
    return result;
}
