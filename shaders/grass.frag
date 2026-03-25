#version 460

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

layout(std430, binding = 2) readonly buffer RepulsorBuffer
{
    Repulsor repulsors[];
};

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in float vBladeT;
layout(location = 3) in float vVariation;

layout(location = 0) out vec4 outColor;

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
    vec3 viewDirection = normalize(params.cameraPositionTime.xyz - vWorldPos);
    vec3 normal = normalize(vNormal);
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }

    vec3 lightDirection = normalize(params.sunDirectionIntensity.xyz);
    float lambert = max(dot(normal, lightDirection), 0.0);
    float backLighting = pow(max(dot(-lightDirection, viewDirection), 0.0), 4.0) *
                         (1.0 - vBladeT) * params.grassColorTip.w;
    float specular = pow(max(dot(reflect(-lightDirection, normal), viewDirection), 0.0), 28.0) * 0.12;

    vec3 baseColor = mix(params.grassColorBase.rgb, params.grassColorTip.rgb, clamp(vBladeT + vVariation * 0.2, 0.0, 1.0));
    baseColor *= 0.88 + vVariation * 0.24;

    vec3 litColor = baseColor * (params.sunColorAmbient.w + lambert * params.sunDirectionIntensity.w);
    litColor += params.sunColorAmbient.rgb * backLighting * params.sunDirectionIntensity.w * 0.45;
    litColor += vec3(specular);

    if (params.repulsorLightInfo.x > 0.5)
    {
        for (uint index = 0u; index < params.counts.y; ++index)
        {
            Repulsor repulsor = repulsors[index];
            vec3 lightCenter =
                repulsor.centerRadius.xyz + vec3(0.0, repulsor.centerRadius.w * 0.55 + 0.25, 0.0);
            vec3 toLight = lightCenter - vWorldPos;
            float lightDistance = length(toLight);
            vec3 localLightDirection = toLight / max(lightDistance, 0.001);
            float attenuation = max(
                1.0 - lightDistance / max(repulsor.centerRadius.w * params.repulsorLightInfo.z, 0.001),
                0.0
            );
            attenuation *= attenuation;
            float localDiffuse = max(dot(normal, localLightDirection), 0.0);
            litColor += repulsorLightColor(index) * localDiffuse * attenuation *
                        params.repulsorLightInfo.y * 0.75;
        }
    }

    float fog = clamp(length(vWorldPos - params.cameraPositionTime.xyz) / 80.0, 0.0, 1.0);
    vec3 fogColor = vec3(0.62, 0.77, 0.96);
    vec3 finalColor = mix(litColor, fogColor, fog * fog);
    finalColor = pow(clamp(finalColor, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
    outColor = vec4(finalColor, 1.0);
}
