#pragma once

#include <vector>

#include "cat_api_types.hpp"
#include "typed_api_routes.hpp"

using CatApiRoute = ApiRoute;

template <class T> using CatRouteResult = TypedRouteResult<T, ErrorResponse>;

std::vector<ApiRoute> makeCatApiRoutes();
