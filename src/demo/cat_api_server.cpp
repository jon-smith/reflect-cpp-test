#include "demo/cat_api_server.hpp"

#include <expected>
#include <string>

#include "core/api_server_support.hpp"
#include "demo/cat_api_routes.hpp"
#include "demo/openapi_demo.hpp"

void RegisterCatApiRoutes(httplib::Server &server)
{
  clam::RegisterOpenApiJsonEndpoint(server, [] { return BuildCatLogOpenApiSpec(); });
  clam::RegisterRoutes(server, MakeCatApiRoutes());
}

std::expected<void, std::string> ServeCatApi(const CatApiServerOptions &options)
{
  httplib::Server server;
  RegisterCatApiRoutes(server);

  if (!server.listen(options.host, options.port))
  {
    return std::unexpected("Failed to listen on http://" + options.host + ":" + std::to_string(options.port));
  }

  return {};
}
