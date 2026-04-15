#pragma once

#include <expected>
#include <string>
#include <vector>

#include "core/openapi_builder.hpp"

std::expected<rfl::Generic, std::string> buildCatLogOpenApiSpec();
std::vector<std::string> validateOpenApiDemoSpec(const rfl::Generic &spec);
