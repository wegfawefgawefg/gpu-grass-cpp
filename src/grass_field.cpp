#include "grass_field.h"

#include <algorithm>
#include <cmath>
#include <random>

std::vector<GrassBladeGpu> BuildGrassField(std::uint32_t bladeCount)
{
    std::vector<GrassBladeGpu> blades;
    blades.reserve(bladeCount);

    const std::uint32_t gridSize = static_cast<std::uint32_t>(std::ceil(std::sqrt(bladeCount)));
    const float cellSize = 2.0f / static_cast<float>(std::max(gridSize, 1u));

    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    for (std::uint32_t index = 0; index < bladeCount; ++index)
    {
        const std::uint32_t cellX = index % gridSize;
        const std::uint32_t cellZ = index / gridSize;

        const float jitterX = (unit(rng) - 0.5f) * cellSize * 0.9f;
        const float jitterZ = (unit(rng) - 0.5f) * cellSize * 0.9f;
        const float normalizedX = -1.0f + (static_cast<float>(cellX) + 0.5f) * cellSize + jitterX;
        const float normalizedZ = -1.0f + (static_cast<float>(cellZ) + 0.5f) * cellSize + jitterZ;

        blades.push_back({
            .rootHeight = {normalizedX, 0.0f, normalizedZ, unit(rng)},
            .params =
                {
                    0.65f + unit(rng) * 0.7f,
                    unit(rng) * 6.28318530718f,
                    unit(rng),
                    unit(rng),
                },
        });
    }

    // Shuffle so lower blade counts reduce density across the whole field instead of
    // truncating one edge of the grid.
    std::shuffle(blades.begin(), blades.end(), rng);

    return blades;
}
