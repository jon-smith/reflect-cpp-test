#include "cat_api_server.hpp"

#include <expected>
#include <string>

#include "api_server_support.hpp"
#include "cat_api_routes.hpp"
#include "openapi_demo.hpp"

void registerCatApiRoutes(httplib::Server &server)
{
  registerOpenApiJsonEndpoint(server, [] { return buildOpenApiSpec(); });
  registerRoutes(server, makeCatApiRoutes());
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
