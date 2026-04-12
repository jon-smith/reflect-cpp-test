#pragma once

#include <functional>
#include <string>
#include <vector>

#include <httplib.h>

#include "openapi_builder.hpp"

using CatApiRouteHandler = std::function<void(const httplib::Request &, httplib::Response &)>;

struct CatApiRoute
{
  std::string method;
  std::string openApiPath;
  std::string httplibPattern;
  OpenApiOperation operation;
  CatApiRouteHandler handler;
};

std::vector<CatApiRoute> makeCatApiRoutes();
