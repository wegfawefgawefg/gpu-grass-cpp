#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <SDL3_ttf/SDL_ttf.h>

#include "demo_types.h"

struct TextOverlay
{
    void Initialize(std::string_view fontPath, float pointSize);
    void Shutdown();
    void Update(std::string_view text);

    [[nodiscard]] const std::array<std::uint32_t, kOverlayPixelCount>& Pixels() const;
    [[nodiscard]] std::uint32_t Width() const;
    [[nodiscard]] std::uint32_t Height() const;

    TTF_Font* m_font = nullptr;
    std::array<std::uint32_t, kOverlayPixelCount> m_pixels = {};
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
};
