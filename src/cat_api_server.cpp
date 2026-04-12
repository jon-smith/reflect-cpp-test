#include "cat_api_server.hpp"

#include <expected>
#include <string>

#include <rfl/json.hpp>

#include "cat_api_routes.hpp"
#include "cat_api_types.hpp"
#include "openapi_demo.hpp"

namespace
{

void registerRoute(httplib::Server &server, const CatApiRoute &route)
{
  if (route.method == "get")
  {
    server.Get(route.httplibPattern, route.handler);
    return;
  }

  if (route.method == "post")
  {
    server.Post(route.httplibPattern, route.handler);
  }
}

}  // namespace

void registerCatApiRoutes(httplib::Server &server)
{
  server.Get("/openapi.json",
             [](const httplib::Request &, httplib::Response &response)
             {
               const auto spec = buildOpenApiSpec();
               if (!spec)
               {
                 response.status = 500;
                 response.set_content(rfl::json::write(ErrorResponse{
                                          .code = "openapi_generation_failed",
                                          .message = "Failed to build the OpenAPI document.",
                                          .detail = spec.error(),
                                      }),
                                      "application/json");
                 return;
               }

               response.status = 200;
               response.set_content(rfl::json::write(spec.value(), rfl::json::pretty), "application/json");
             });

  for (const auto &route : makeCatApiRoutes())
  {
    registerRoute(server, route);
  }
}

std::expected<void, std::string> serveCatApi(const CatApiServerOptions &options)
{
  httplib::Server server;
  registerCatApiRoutes(server);

  if (!server.listen(options.host, options.port))
  {
    return std::unexpected("Failed to listen on http://" + options.host + ":" + std::to_string(options.port));
  }

  return {};
}
