#pragma once

#include <expected>
#include <string>

#include "openapi_builder.hpp"

std::expected<rfl::Generic, std::string> buildOpenApiSpec();
std::expected<rfl::Generic, std::string> buildOpenApiSpec(const OpenApiSpecConfig &config);

int runChecks();
