#pragma once

#include <expected>
#include <string>
#include <vector>

#include "core/openapi_builder.hpp"

clam::OpenApiSpecConfig makeCatLogOpenApiConfig();
std::expected<rfl::Generic, std::string> buildOpenApiSpec();
std::expected<rfl::Generic, std::string> buildOpenApiSpec(const clam::OpenApiSpecConfig &config);
std::vector<std::string> validateOpenApiDemoSpec(const rfl::Generic &spec);
std::vector<std::string> validateOpenApiDemoSpec();
