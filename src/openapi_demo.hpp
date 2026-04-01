#pragma once

#include <expected>
#include <string>
#include <vector>

#include "openapi_builder.hpp"

std::expected<rfl::Generic, std::string> buildOpenApiSpec();
std::expected<rfl::Generic, std::string> buildOpenApiSpec(const OpenApiSpecConfig &config);
std::vector<std::string> validateOpenApiDemoSpec(const rfl::Generic &spec);
std::vector<std::string> validateOpenApiDemoSpec();
