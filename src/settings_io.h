#pragma once

#include <string>
#include <string_view>

#include "demo_types.h"

bool SaveDemoSettings(
    const DemoSettings& settings,
    std::string_view path,
    std::string& errorMessage
);

bool LoadDemoSettings(DemoSettings& settings, std::string_view path, std::string& errorMessage);
