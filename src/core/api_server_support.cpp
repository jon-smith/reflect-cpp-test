#include "core/api_server_support.hpp"

#include <utility>

#include <rfl/json.hpp>

namespace clam
{

namespace
{

OpenApiJson makeOpenApiGenerationErrorBody(const std::string &detail)
{
  return makeObject({
      {"error", "openapi_generation_failed"},
      {"message", "Failed to build the OpenAPI document."},
      {"detail", detail},
  });
}

}

void registerRoute(httplib::Server &server, const ApiRoute &route)
{
  if (route.method == HttpMethod::get)
  {
    server.Get(route.httplibPattern, route.handler);
    return;
  }

  if (route.method == HttpMethod::post)
  {
    server.Post(route.httplibPattern, route.handler);
  }
}

void registerRoutes(httplib::Server &server, const std::vector<ApiRoute> &routes)
{
  for (const auto &route : routes)
  {
    registerRoute(server, route);
  }
}

void registerOpenApiJsonEndpoint(httplib::Server &server, OpenApiSpecBuilder buildSpec, std::string path)
{
  server.Get(std::move(path),
             [buildSpec = std::move(buildSpec)](const httplib::Request &, httplib::Response &response)
             {
               const auto spec = buildSpec();
               if (!spec)
               {
                 response.status = 500;
                 response.set_content(rfl::json::write(makeOpenApiGenerationErrorBody(spec.error())),
                                      "application/json");
                 return;
               }

               response.status = 200;
               response.set_content(rfl::json::write(spec.value(), rfl::json::pretty), "application/json");
             });
}

}
