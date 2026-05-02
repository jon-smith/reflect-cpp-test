#include "core/typed_api_routes.hpp"

#include <unordered_map>
#include <utility>

namespace clam
{

OpenApiOperation MakeOpenApiOperation(const TypedRouteMetadata &metadata, std::optional<OpenApiRequestBody> requestBody,
                                      std::vector<OpenApiResponse> responses)
{
  return OpenApiOperation{
      .summary = metadata.summary,
      .operationId = metadata.operationId,
      .parameters = metadata.parameters,
      .requestBody = std::move(requestBody),
      .responses = std::move(responses),
  };
}

std::string_view ToOpenApiMethod(const HttpMethod method)
{
  switch (method)
  {
  case HttpMethod::get:
    return "get";
  case HttpMethod::post:
    return "post";
  }

  return "get";
}

void WriteSerializedJsonResponse(httplib::Response &response, int status, std::string body,
                                 const std::string &contentType)
{
  response.status = status;
  response.set_content(std::move(body), contentType);
}

std::vector<clam::OpenApiPathItem> BuildOpenApiPaths(const std::vector<clam::ApiRoute> &apiRoutes)
{
  std::vector<clam::OpenApiPathItem> pathItems;
  std::unordered_map<std::string, std::size_t> pathIndexes;

  for (const auto &route : apiRoutes)
  {
    const auto it = pathIndexes.find(route.openApiPath);
    if (it == pathIndexes.end())
    {
      pathIndexes.emplace(route.openApiPath, pathItems.size());
      pathItems.push_back(clam::OpenApiPathItem{
          .path = route.openApiPath,
          .operations = {clam::OpenApiPathOperation{
              .method = std::string(clam::ToOpenApiMethod(route.method)),
              .operation = route.operation,
          }},
      });
      continue;
    }

    pathItems[it->second].operations.push_back(clam::OpenApiPathOperation{
        .method = std::string(clam::ToOpenApiMethod(route.method)),
        .operation = route.operation,
    });
  }

  return pathItems;
}

std::vector<clam::OpenApiSchemaRegistrar> BuildOpenApiSchemaRegistrations(const std::vector<clam::ApiRoute> &apiRoutes)
{
  std::vector<clam::OpenApiSchemaRegistrar> schemaRegistrations;

  for (const auto &route : apiRoutes)
  {
    schemaRegistrations.insert(schemaRegistrations.end(), route.schemaRegistrations.begin(),
                               route.schemaRegistrations.end());
  }

  return schemaRegistrations;
}

}
