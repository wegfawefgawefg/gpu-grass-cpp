#version 460

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

struct Repulsor
{
    vec4 centerRadius;
    vec4 velocityStrength;
};

layout(std430, binding = 2) readonly buffer RepulsorBuffer
{
    Repulsor repulsors[];
};

layout(location = 0) in vec3 vWorldPos;

layout(location = 0) out vec4 outColor;

float noise2(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

vec3 repulsorLightColor(uint index)
{
    const vec3 palette[4] = vec3[](
        vec3(1.00, 0.56, 0.18),
        vec3(0.16, 0.82, 1.00),
        vec3(0.45, 1.00, 0.30),
        vec3(1.00, 0.28, 0.72)
    );
    return palette[index % 4u];
}

void main()
{
    vec3 lightDirection = normalize(params.sunDirectionIntensity.xyz);
    float lambert = max(dot(vec3(0.0, 1.0, 0.0), lightDirection), 0.0);
    float patches = mix(0.85, 1.15, noise2(vWorldPos.xz * 0.08));
    float trails = mix(0.92, 1.12, noise2(vWorldPos.xz * 0.32 + 15.0));
    vec3 baseColor = params.groundColor.rgb * patches * trails;
    vec3 litColor = baseColor * (params.sunColorAmbient.w + lambert * params.sunDirectionIntensity.w * 0.55);

    vec3 repulsorAccent = vec3(0.0);
    for (uint index = 0u; index < params.counts.y; ++index)
    {
        Repulsor repulsor = repulsors[index];
        float radius = max(repulsor.centerRadius.w, 0.001);
        float distanceToCenter = length(vWorldPos.xz - repulsor.centerRadius.xz);
        float ring = exp(-pow((distanceToCenter - radius) / max(radius * 0.12, 0.12), 2.0));
        float core = exp(-pow(distanceToCenter / max(radius * 0.5, 0.18), 2.0));
        repulsorAccent += vec3(0.08, 0.45, 0.82) * (ring * 0.7 + core * 0.18) * params.groundColor.w;

        if (params.repulsorLightInfo.x > 0.5)
        {
            vec3 lightCenter =
                repulsor.centerRadius.xyz + vec3(0.0, repulsor.centerRadius.w * 0.55 + 0.25, 0.0);
            vec3 toLight = lightCenter - vWorldPos;
            float lightDistance = length(toLight);
            float attenuation = max(
                1.0 - lightDistance / max(radius * params.repulsorLightInfo.z, 0.001),
                0.0
            );
            attenuation *= attenuation;
            repulsorAccent += repulsorLightColor(index) * attenuation * params.repulsorLightInfo.y * 0.55;
        }
    }
    litColor += repulsorAccent;

    float horizon = clamp((length(vWorldPos.xz - params.cameraPositionTime.xz) - 12.0) / 70.0, 0.0, 1.0);
    vec3 fogColor = vec3(0.60, 0.76, 0.95);
    vec3 finalColor = mix(litColor, fogColor, horizon * horizon);
    finalColor = pow(clamp(finalColor, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
    outColor = vec4(finalColor, 1.0);
}
