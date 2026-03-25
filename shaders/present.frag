#version 460

layout(binding = 0) uniform sampler2D sceneColor;

layout(std140, binding = 1) uniform PresentParams
{
    vec4 overlayInfo;
    vec4 windowInfo;
} params;

layout(std430, binding = 2) readonly buffer OverlayBuffer
{
    uint overlayPixels[];
};

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

vec4 unpackOverlayPixel(uint packed)
{
    return vec4(
        float(packed & 0xFFu),
        float((packed >> 8u) & 0xFFu),
        float((packed >> 16u) & 0xFFu),
        float((packed >> 24u) & 0xFFu)
    ) / 255.0;
}

void main()
{
    vec3 color = texture(sceneColor, vUv).rgb;

    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 overlaySize = ivec2(params.overlayInfo.xy);
    ivec2 overlayOrigin = ivec2(params.overlayInfo.zw);
    ivec2 overlayPixel = pixel - overlayOrigin;
    if (overlayPixel.x >= 0 && overlayPixel.y >= 0 &&
        overlayPixel.x < overlaySize.x && overlayPixel.y < overlaySize.y)
    {
        int overlayIndex = overlayPixel.y * 512 + overlayPixel.x;
        vec4 overlayColor = unpackOverlayPixel(overlayPixels[overlayIndex]);
        color = mix(color, overlayColor.rgb, overlayColor.a);
    }

    outColor = vec4(color, 1.0);
}
