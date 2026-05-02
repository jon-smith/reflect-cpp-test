#pragma once

#include <expected>
#include <string>
#include <vector>

#include "core/openapi_builder.hpp"

std::expected<rfl::Generic, std::string> BuildCatLogOpenApiSpec();
std::vector<std::string> ValidateOpenApiDemoSpec(const rfl::Generic &spec);
