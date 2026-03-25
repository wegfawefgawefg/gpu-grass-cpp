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

layout(location = 0) out vec3 vWorldPos;

void main()
{
    const vec2 corners[6] = vec2[](
        vec2(-1.0, -1.0),
        vec2(1.0, -1.0),
        vec2(1.0, 1.0),
        vec2(1.0, 1.0),
        vec2(-1.0, 1.0),
        vec2(-1.0, -1.0)
    );

    float extent = params.windB.w * 1.8 + 18.0;
    vec2 corner = corners[gl_VertexIndex] * extent;
    vec3 worldPos = vec3(corner.x, 0.0, corner.y);
    vWorldPos = worldPos;
    gl_Position = params.viewProjection * vec4(worldPos, 1.0);
}
