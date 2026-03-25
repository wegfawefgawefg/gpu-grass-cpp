#include "settings_io.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
float ComputeFieldArea(float fieldExtent)
{
    const float extent = fieldExtent < 0.0f ? 0.0f : fieldExtent;
    const float sideLength = extent * 2.0f;
    return sideLength * sideLength;
}

bool ExtractRawValue(std::string_view text, std::string_view key, std::string& rawValue)
{
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = text.find(needle);
    if (keyPosition == std::string_view::npos)
    {
        return false;
    }

    const std::size_t colonPosition = text.find(':', keyPosition + needle.size());
    if (colonPosition == std::string_view::npos)
    {
        return false;
    }

    std::size_t valueStart = colonPosition + 1;
    while (valueStart < text.size() &&
           std::isspace(static_cast<unsigned char>(text[valueStart])) != 0)
    {
        ++valueStart;
    }

    if (valueStart >= text.size())
    {
        return false;
    }

    if (text[valueStart] == '[')
    {
        const std::size_t valueEnd = text.find(']', valueStart);
        if (valueEnd == std::string_view::npos)
        {
            return false;
        }
        rawValue.assign(text.substr(valueStart, valueEnd - valueStart + 1));
        return true;
    }

    std::size_t valueEnd = valueStart;
    while (valueEnd < text.size() && text[valueEnd] != ',' && text[valueEnd] != '}' &&
           text[valueEnd] != '\n' && text[valueEnd] != '\r')
    {
        ++valueEnd;
    }

    rawValue.assign(text.substr(valueStart, valueEnd - valueStart));
    while (!rawValue.empty() && std::isspace(static_cast<unsigned char>(rawValue.back())) != 0)
    {
        rawValue.pop_back();
    }
    return !rawValue.empty();
}

bool ParseFloat(std::string_view text, std::string_view key, float& value)
{
    std::string rawValue;
    if (!ExtractRawValue(text, key, rawValue))
    {
        return false;
    }

    try
    {
        value = std::stof(rawValue);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ParseUInt(std::string_view text, std::string_view key, std::uint32_t& value)
{
    std::string rawValue;
    if (!ExtractRawValue(text, key, rawValue))
    {
        return false;
    }

    try
    {
        value = static_cast<std::uint32_t>(std::stoul(rawValue));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ParseInt(std::string_view text, std::string_view key, int& value)
{
    std::string rawValue;
    if (!ExtractRawValue(text, key, rawValue))
    {
        return false;
    }

    try
    {
        value = std::stoi(rawValue);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ParseBool(std::string_view text, std::string_view key, bool& value)
{
    std::string rawValue;
    if (!ExtractRawValue(text, key, rawValue))
    {
        return false;
    }

    if (rawValue == "true")
    {
        value = true;
        return true;
    }
    if (rawValue == "false")
    {
        value = false;
        return true;
    }
    return false;
}

bool ParseColor(std::string_view text, std::string_view key, std::array<float, 3>& value)
{
    std::string rawValue;
    if (!ExtractRawValue(text, key, rawValue))
    {
        return false;
    }

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (std::sscanf(rawValue.c_str(), "[%f,%f,%f]", &r, &g, &b) != 3 &&
        std::sscanf(rawValue.c_str(), "[ %f , %f , %f ]", &r, &g, &b) != 3)
    {
        return false;
    }

    value = {r, g, b};
    return true;
}
} // namespace

bool SaveDemoSettings(
    const DemoSettings& settings,
    std::string_view path,
    std::string& errorMessage
)
{
    std::ofstream file(path.data(), std::ios::trunc);
    if (!file.is_open())
    {
        errorMessage = "Failed to open settings file for writing.";
        return false;
    }

    file << "{\n"
         << "  \"renderScale\": " << settings.renderScale << ",\n"
         << "  \"grassDensity\": " << settings.grassDensity << ",\n"
         << "  \"fieldExtent\": " << settings.fieldExtent << ",\n"
         << "  \"bladeHeight\": " << settings.bladeHeight << ",\n"
         << "  \"bladeWidth\": " << settings.bladeWidth << ",\n"
         << "  \"flex\": " << settings.flex << ",\n"
         << "  \"curvature\": " << settings.curvature << ",\n"
         << "  \"rootStiffness\": " << settings.rootStiffness << ",\n"
         << "  \"staticLean\": " << settings.staticLean << ",\n"
         << "  \"windYawDegrees\": " << settings.windYawDegrees << ",\n"
         << "  \"windStrength\": " << settings.windStrength << ",\n"
         << "  \"windTimeScale\": " << settings.windTimeScale << ",\n"
         << "  \"windNoiseScale\": " << settings.windNoiseScale << ",\n"
         << "  \"windDetailNoiseScale\": " << settings.windDetailNoiseScale << ",\n"
         << "  \"windDetailStrength\": " << settings.windDetailStrength << ",\n"
         << "  \"windCross\": " << settings.windCross << ",\n"
         << "  \"windGust\": " << settings.windGust << ",\n"
         << "  \"sunYawDegrees\": " << settings.sunYawDegrees << ",\n"
         << "  \"sunPitchDegrees\": " << settings.sunPitchDegrees << ",\n"
         << "  \"sunIntensity\": " << settings.sunIntensity << ",\n"
         << "  \"ambient\": " << settings.ambient << ",\n"
         << "  \"groundBrightness\": " << settings.groundBrightness << ",\n"
         << "  \"repulsorRadius\": " << settings.repulsorRadius << ",\n"
         << "  \"repulsorStrength\": " << settings.repulsorStrength << ",\n"
         << "  \"repulsorSpeed\": " << settings.repulsorSpeed << ",\n"
         << "  \"repulsorLights\": " << (settings.repulsorLights ? "true" : "false") << ",\n"
         << "  \"repulsorLightStrength\": " << settings.repulsorLightStrength << ",\n"
         << "  \"repulsorLightRadius\": " << settings.repulsorLightRadius << ",\n"
         << "  \"animateRepulsors\": " << (settings.animateRepulsors ? "true" : "false") << ",\n"
         << "  \"repulsorCount\": " << settings.repulsorCount << ",\n"
         << "  \"grassBaseColor\": [" << settings.grassBaseColor[0] << ", "
         << settings.grassBaseColor[1] << ", " << settings.grassBaseColor[2] << "],\n"
         << "  \"grassTipColor\": [" << settings.grassTipColor[0] << ", "
         << settings.grassTipColor[1] << ", " << settings.grassTipColor[2] << "]\n"
         << "}\n";

    if (!file.good())
    {
        errorMessage = "Failed while writing settings JSON.";
        return false;
    }

    errorMessage.clear();
    return true;
}

bool LoadDemoSettings(DemoSettings& settings, std::string_view path, std::string& errorMessage)
{
    std::ifstream file(path.data());
    if (!file.is_open())
    {
        errorMessage = "Settings JSON not found yet.";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    DemoSettings loaded = settings;

    ParseFloat(text, "renderScale", loaded.renderScale);
    ParseFloat(text, "fieldExtent", loaded.fieldExtent);
    const bool hasGrassDensity = ParseFloat(text, "grassDensity", loaded.grassDensity);
    if (!hasGrassDensity)
    {
        std::uint32_t legacyBladeCount = 0;
        if (ParseUInt(text, "bladeCount", legacyBladeCount))
        {
            const float fieldArea = ComputeFieldArea(loaded.fieldExtent);
            if (fieldArea > 0.0f)
            {
                loaded.grassDensity = static_cast<float>(legacyBladeCount) / fieldArea;
            }
        }
    }
    ParseFloat(text, "bladeHeight", loaded.bladeHeight);
    ParseFloat(text, "bladeWidth", loaded.bladeWidth);
    ParseFloat(text, "flex", loaded.flex);
    ParseFloat(text, "curvature", loaded.curvature);
    ParseFloat(text, "rootStiffness", loaded.rootStiffness);
    ParseFloat(text, "staticLean", loaded.staticLean);
    ParseFloat(text, "windYawDegrees", loaded.windYawDegrees);
    ParseFloat(text, "windStrength", loaded.windStrength);
    ParseFloat(text, "windTimeScale", loaded.windTimeScale);
    ParseFloat(text, "windNoiseScale", loaded.windNoiseScale);
    ParseFloat(text, "windDetailNoiseScale", loaded.windDetailNoiseScale);
    ParseFloat(text, "windDetailStrength", loaded.windDetailStrength);
    ParseFloat(text, "windCross", loaded.windCross);
    ParseFloat(text, "windGust", loaded.windGust);
    ParseFloat(text, "sunYawDegrees", loaded.sunYawDegrees);
    ParseFloat(text, "sunPitchDegrees", loaded.sunPitchDegrees);
    ParseFloat(text, "sunIntensity", loaded.sunIntensity);
    ParseFloat(text, "ambient", loaded.ambient);
    ParseFloat(text, "groundBrightness", loaded.groundBrightness);
    ParseFloat(text, "repulsorRadius", loaded.repulsorRadius);
    ParseFloat(text, "repulsorStrength", loaded.repulsorStrength);
    ParseFloat(text, "repulsorSpeed", loaded.repulsorSpeed);
    ParseBool(text, "repulsorLights", loaded.repulsorLights);
    ParseFloat(text, "repulsorLightStrength", loaded.repulsorLightStrength);
    ParseFloat(text, "repulsorLightRadius", loaded.repulsorLightRadius);
    ParseBool(text, "animateRepulsors", loaded.animateRepulsors);
    ParseInt(text, "repulsorCount", loaded.repulsorCount);
    ParseColor(text, "grassBaseColor", loaded.grassBaseColor);
    ParseColor(text, "grassTipColor", loaded.grassTipColor);

    settings = loaded;
    errorMessage.clear();
    return true;
}
