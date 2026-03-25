#include "text_overlay.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

void TextOverlay::Initialize(std::string_view fontPath, float pointSize)
{
    if (!TTF_Init())
    {
        throw std::runtime_error("TTF_Init failed");
    }

    m_font = TTF_OpenFont(fontPath.data(), pointSize);
    if (m_font == nullptr)
    {
        throw std::runtime_error("TTF_OpenFont failed");
    }
}

void TextOverlay::Shutdown()
{
    if (m_font != nullptr)
    {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }

    TTF_Quit();
}

void TextOverlay::Update(std::string_view text)
{
    m_pixels.fill(0);
    m_width = 0;
    m_height = 0;

    if (m_font == nullptr || text.empty())
    {
        return;
    }

    const SDL_Color color = {245, 234, 212, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(m_font, text.data(), text.size(), color);
    if (surface == nullptr)
    {
        return;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);
    if (rgbaSurface == nullptr)
    {
        return;
    }

    m_width = std::min(static_cast<std::uint32_t>(rgbaSurface->w), kOverlayBufferWidth);
    m_height = std::min(static_cast<std::uint32_t>(rgbaSurface->h), kOverlayBufferHeight);

    if (m_width > 0 && m_height > 0 && SDL_LockSurface(rgbaSurface))
    {
        const auto* sourceRows = static_cast<const std::uint8_t*>(rgbaSurface->pixels);
        for (std::uint32_t row = 0; row < m_height; ++row)
        {
            const auto* source = sourceRows + row * rgbaSurface->pitch;
            auto* destination =
                reinterpret_cast<std::uint8_t*>(m_pixels.data() + row * kOverlayBufferWidth);
            std::memcpy(destination, source, m_width * sizeof(std::uint32_t));
        }
        SDL_UnlockSurface(rgbaSurface);
    }

    SDL_DestroySurface(rgbaSurface);
}

const std::array<std::uint32_t, kOverlayPixelCount>& TextOverlay::Pixels() const
{
    return m_pixels;
}

std::uint32_t TextOverlay::Width() const
{
    return m_width;
}

std::uint32_t TextOverlay::Height() const
{
    return m_height;
}
