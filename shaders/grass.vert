#version 460

const uint SEGMENT_COUNT = 4u;

struct GrassBlade
{
    vec4 rootHeight;
    vec4 params;
};

struct Repulsor
{
    vec4 centerRadius;
    vec4 velocityStrength;
};

layout(std140, binding = 0) uniform SceneParams
{
    mat4 viewProjection;
    vec4 cameraPositionTime;
    vec4 sunDirectionIntensity;
    vec4 sunColorAmbient;
    vec4 windA;
    vec4 windB;
    vec4 windC;
    vec4 grassShape;
    vec4 grassMotion;
    vec4 grassColorBase;
    vec4 grassColorTip;
    vec4 groundColor;
    vec4 repulsorLightInfo;
    uvec4 counts;
} params;

layout(std430, binding = 1) readonly buffer BladeBuffer
{
    GrassBlade blades[];
};

layout(std430, binding = 2) readonly buffer RepulsorBuffer
{
    Repulsor repulsors[];
};

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out float vBladeT;
layout(location = 3) out float vVariation;

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

vec3 mod289(vec3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x)
{
    return mod289(((x * 34.0) + 10.0) * x);
}

vec4 taylorInvSqrt(vec4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float simplex3(vec3 v)
{
    const vec2 c = vec2(1.0 / 6.0, 1.0 / 3.0);
    const vec4 d = vec4(0.0, 0.5, 1.0, 2.0);

    vec3 i = floor(v + dot(v, c.yyy));
    vec3 x0 = v - i + dot(i, c.xxx);

    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    vec3 x1 = x0 - i1 + c.xxx;
    vec3 x2 = x0 - i2 + c.yyy;
    vec3 x3 = x0 - d.yyy;

    i = mod289(i);
    vec4 p = permute(
        permute(
            permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y +
            vec4(0.0, i1.y, i2.y, 1.0)
        ) + i.x + vec4(0.0, i1.x, i2.x, 1.0)
    );

    float n_ = 1.0 / 7.0;
    vec3 ns = n_ * d.wyz - d.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);

    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);
    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);

    vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    vec4 m = max(
        0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)),
        0.0
    );
    m *= m;
    return 42.0 * dot(
        m * m,
        vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3))
    );
}

float simplexFbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.55;
    for (int octave = 0; octave < 3; ++octave)
    {
        value += simplex3(p) * amplitude;
        p = p * 2.03 + vec3(19.1, 7.3, 13.7);
        amplitude *= 0.5;
    }
    return value;
}

vec2 sampleFlowField(vec2 flowCoords, float time, vec2 bladeOffset)
{
    vec3 primaryP = vec3(flowCoords * params.windB.x + bladeOffset * 0.35, time * 0.18);
    vec2 warp = vec2(
        simplexFbm(primaryP * 0.72 + vec3(13.4, 2.7, 11.1)),
        simplexFbm(primaryP * 0.72 + vec3(-7.8, 19.3, 5.4))
    ) * params.windC.x;

    vec3 advectedPrimary = primaryP;
    advectedPrimary.xy += warp;

    vec2 primaryVec = vec2(
        simplexFbm(advectedPrimary),
        simplexFbm(advectedPrimary + vec3(23.7, -11.2, 17.4))
    );

    vec3 detailP = vec3(flowCoords * params.windB.y + bladeOffset * 0.75, time * 0.33);
    detailP.xy += warp * params.windC.y + primaryVec * 0.55;

    vec2 detailVec = vec2(
        simplexFbm(detailP + vec3(-15.1, 31.4, 9.2)),
        simplexFbm(detailP + vec3(27.3, 6.8, 21.7))
    );

    float gust = saturate(0.5 + 0.5 * simplexFbm(advectedPrimary * 0.55 + vec3(41.8, -17.5, 7.6)));
    gust = mix(0.5, gust, clamp(params.windC.z, 0.0, 1.8));

    float along = 0.45 + gust * (0.8 + params.grassMotion.y * 0.75);
    along += primaryVec.x * 0.32 + detailVec.x * params.grassMotion.z * 0.24;
    along = max(along, 0.05);

    float cross = (primaryVec.y * 0.55 + detailVec.y * params.grassMotion.z * 0.35) * params.windB.z;
    return vec2(along, cross);
}

vec3 safeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared < 1e-8)
    {
        return fallback;
    }
    return value * inversesqrt(lengthSquared);
}

vec3 evaluateRepulsors(vec3 basePosition, float t)
{
    vec3 offset = vec3(0.0);
    for (uint index = 0u; index < params.counts.y; ++index)
    {
        Repulsor repulsor = repulsors[index];
        vec2 delta = basePosition.xz - repulsor.centerRadius.xz;
        float distanceToRepulsor = length(delta);
        float radius = max(repulsor.centerRadius.w, 0.001);
        float influence = saturate(1.0 - distanceToRepulsor / radius);
        if (influence <= 0.0)
        {
            continue;
        }

        vec2 away = normalize(delta + repulsor.velocityStrength.xz * 0.06 + vec2(0.0001));
        offset += vec3(away.x, 0.0, away.y) * repulsor.velocityStrength.w * influence * influence *
                  params.grassShape.z * t;
    }
    return offset;
}

vec3 evaluateBladePoint(GrassBlade blade, vec3 basePosition, float t)
{
    float height = mix(0.55, 1.35, blade.rootHeight.w) * params.grassShape.x;
    vec3 windDirection =
        safeNormalize(vec3(params.windA.x, 0.0, params.windA.y), vec3(1.0, 0.0, 0.0));
    vec3 crossDirection = vec3(-windDirection.z, 0.0, windDirection.x);
    float time = params.cameraPositionTime.w * params.windA.w;
    vec2 flowCoords = vec2(dot(basePosition.xz, windDirection.xz), dot(basePosition.xz, crossDirection.xz));

    float bendWeight = pow(
        saturate((t - params.grassMotion.x) / max(1.0 - params.grassMotion.x, 0.001)),
        0.55 + params.grassShape.w
    ) * params.grassShape.z;
    vec2 flowSample = sampleFlowField(
        flowCoords,
        time,
        vec2(blade.params.y * 0.5, blade.params.z * 3.1)
    );

    vec3 localFlow = windDirection * flowSample.x + crossDirection * flowSample.y;
    vec3 flowDirection = safeNormalize(localFlow, windDirection);
    float flowMagnitude = length(flowSample);
    vec3 bend = flowDirection * params.windA.z * flowMagnitude * bendWeight;

    vec3 bladeDirection = vec3(sin(blade.params.y), 0.0, cos(blade.params.y));
    float shapeVariation = (blade.params.z * 2.0 - 1.0) * params.grassMotion.w * 0.12;
    vec3 restLean =
        flowDirection * params.grassMotion.w * (0.38 + blade.params.z * 0.32) * bendWeight;
    vec3 sideLean = bladeDirection * shapeVariation * bendWeight;
    vec3 repulsor = evaluateRepulsors(basePosition, t);

    vec3 position = basePosition + bend + restLean + sideLean + repulsor;
    position.y += height * t;
    return position;
}

void main()
{
    GrassBlade blade = blades[gl_InstanceIndex];
    vec3 basePosition = vec3(blade.rootHeight.x, 0.0, blade.rootHeight.z) * params.windB.w;

    uint segment = uint(gl_VertexIndex) / 6u;
    uint localVertex = uint(gl_VertexIndex) % 6u;

    const float startTTable[6] = float[](0.0, 0.0, 1.0, 1.0, 0.0, 1.0);
    const float sideTable[6] = float[](-1.0, 1.0, 1.0, 1.0, -1.0, -1.0);

    float segmentStart = float(segment) / float(SEGMENT_COUNT);
    float segmentEnd = float(segment + 1u) / float(SEGMENT_COUNT);
    float interpolation = mix(segmentStart, segmentEnd, startTTable[localVertex]);

    vec3 center = evaluateBladePoint(blade, basePosition, interpolation);
    vec3 nextCenter = evaluateBladePoint(blade, basePosition, min(interpolation + 0.04, 1.0));
    vec3 tangent = safeNormalize(nextCenter - center, vec3(0.0, 1.0, 0.0));
    vec3 viewDirection = safeNormalize(params.cameraPositionTime.xyz - center, vec3(0.0, 0.0, 1.0));
    vec3 side = safeNormalize(cross(viewDirection, tangent), vec3(1.0, 0.0, 0.0));

    float widthScale = mix(0.65, 1.35, blade.params.x);
    float width = params.grassShape.y * widthScale * mix(1.0, 0.14, interpolation);
    vec3 worldPosition = center + side * sideTable[localVertex] * width;
    vec3 normal = safeNormalize(cross(side, tangent), vec3(0.0, 1.0, 0.0));

    vWorldPos = worldPosition;
    vNormal = normal;
    vBladeT = interpolation;
    vVariation = blade.params.w;
    gl_Position = params.viewProjection * vec4(worldPosition, 1.0);
}
