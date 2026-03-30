#pragma once

#include <expected>
#include <string>

#include <rfl.hpp>

std::expected<rfl::Generic, std::string> buildOpenApiSpec();
int runChecks();
