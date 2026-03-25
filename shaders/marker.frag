#version 460

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

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 lightDirection = normalize(params.sunDirectionIntensity.xyz);
    vec3 viewDirection = normalize(params.cameraPositionTime.xyz - vWorldPos);
    vec3 normal = normalize(vNormal);

    float lambert = max(dot(normal, lightDirection), 0.0);
    float fresnel = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.6);
    float specular = pow(max(dot(reflect(-lightDirection, normal), viewDirection), 0.0), 24.0);

    vec3 baseColor = vec3(0.16, 0.92, 1.00);
    vec3 glowColor = vec3(1.00, 0.72, 0.18);
    vec3 color = baseColor * (0.35 + lambert * 1.2) + glowColor * fresnel * 0.85 + vec3(specular);
    color = pow(clamp(color, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
