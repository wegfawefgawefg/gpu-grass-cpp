#version 460

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
    uvec4 counts;
} params;

layout(std430, binding = 2) readonly buffer RepulsorBuffer
{
    Repulsor repulsors[];
};

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;

void main()
{
    const vec3 octahedronVertices[6] = vec3[](
        vec3(0.0, 1.0, 0.0),
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(-1.0, 0.0, 0.0),
        vec3(0.0, 0.0, -1.0),
        vec3(0.0, -1.0, 0.0)
    );
    const int triangleIndices[24] = int[](
        0, 1, 2,
        0, 2, 3,
        0, 3, 4,
        0, 4, 1,
        5, 2, 1,
        5, 3, 2,
        5, 4, 3,
        5, 1, 4
    );

    Repulsor repulsor = repulsors[gl_InstanceIndex];
    float visualRadius = max(0.18, repulsor.centerRadius.w * 0.22);
    vec3 center = repulsor.centerRadius.xyz + vec3(0.0, repulsor.centerRadius.w * 0.55 + 0.25, 0.0);
    vec3 localVertex = octahedronVertices[triangleIndices[gl_VertexIndex]] * visualRadius;
    vec3 worldPosition = center + localVertex;

    vWorldPos = worldPosition;
    vNormal = normalize(localVertex);
    gl_Position = params.viewProjection * vec4(worldPosition, 1.0);
}
