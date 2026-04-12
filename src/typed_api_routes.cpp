#include "typed_api_routes.hpp"

#include <utility>

OpenApiOperation makeOpenApiOperation(const TypedRouteMetadata &metadata, std::optional<OpenApiRequestBody> requestBody,
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

void writeSerializedJsonResponse(httplib::Response &response, int status, std::string body, const std::string &contentType)
{
  response.status = status;
  response.set_content(std::move(body), contentType);
}
