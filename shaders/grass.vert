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

float hash12(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise2(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 4; ++octave)
    {
        value += noise2(p) * amplitude;
        p = p * 2.03 + vec2(17.3, 9.1);
        amplitude *= 0.5;
    }
    return value;
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
    float primaryNoise = fbm(
        vec2(
            flowCoords.x * params.windB.x - time * 0.65 + blade.params.z * 4.0,
            flowCoords.y * params.windB.x * 0.45 + blade.params.w * 5.0
        )
    );
    float detailNoise = fbm(
        vec2(
            flowCoords.x * params.windB.y - time * 1.45 + blade.params.y * 0.5,
            flowCoords.y * params.windB.y + blade.params.z * 7.0
        )
    );
    float crossNoise = fbm(
        vec2(
            flowCoords.y * params.windB.y * 0.9 + time * 0.75 + blade.params.w * 3.0,
            flowCoords.x * params.windB.x * 0.55 - time * 0.35
        )
    );
    float gustNoise = fbm(
        vec2(
            flowCoords.x * params.windB.x * 0.35 - time * 0.22 + 19.7,
            flowCoords.y * params.windB.x * 0.18 + 3.1
        )
    );

    float bendWeight = pow(
        saturate((t - params.grassMotion.x) / max(1.0 - params.grassMotion.x, 0.001)),
        0.55 + params.grassShape.w
    ) * params.grassShape.z;
    float gustMultiplier = mix(0.55, 1.0 + params.grassMotion.y, gustNoise);
    float downwindCarrier = params.windA.z * gustMultiplier * mix(0.72, 1.28, primaryNoise);
    float alongTurbulence =
        (detailNoise * 2.0 - 1.0) * params.grassMotion.z * params.windA.z * 0.30;
    float crossTurbulence = (crossNoise * 2.0 - 1.0) * params.windB.z * params.windA.z * 0.42;

    vec3 bend = windDirection * (downwindCarrier + alongTurbulence) * bendWeight;
    bend += crossDirection * crossTurbulence * bendWeight;

    vec3 bladeDirection = vec3(sin(blade.params.y), 0.0, cos(blade.params.y));
    float shapeVariation = (blade.params.z * 2.0 - 1.0) * params.grassMotion.w * 0.16;
    vec3 restLean =
        windDirection * params.grassMotion.w * (0.55 + blade.params.z * 0.45) * bendWeight;
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
