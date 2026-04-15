#pragma once

#include <expected>
#include <string>
#include <vector>

#include "core/openapi_builder.hpp"

clam::OpenApiSpecConfig makeCatLogOpenApiConfig();
std::expected<rfl::Generic, std::string> buildOpenApiSpec();
std::vector<std::string> validateOpenApiDemoSpec(const rfl::Generic &spec);
