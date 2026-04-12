#pragma once

#include <expected>
#include <functional>
#include <string>
#include <vector>

#include <httplib.h>

#include "core/openapi_builder.hpp"
#include "core/typed_api_routes.hpp"

namespace clam
{

using OpenApiSpecBuilder = std::function<std::expected<OpenApiJson, std::string>()>;

void registerRoute(httplib::Server &server, const ApiRoute &route);

void registerRoutes(httplib::Server &server, const std::vector<ApiRoute> &routes);

void registerOpenApiJsonEndpoint(httplib::Server &server, OpenApiSpecBuilder buildSpec,
                                 std::string path = "/openapi.json");

}  // namespace clam
