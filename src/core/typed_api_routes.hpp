#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <httplib.h>

#include "core/openapi_builder.hpp"

namespace clam
{

enum class HttpMethod
{
  get,
  post,
};

using ApiRouteHandler = std::function<void(const httplib::Request &, httplib::Response &)>;

struct ApiRoute
{
  HttpMethod method;
  std::string openApiPath;
  std::string httplibPattern;
  OpenApiOperation operation;
  std::vector<OpenApiSchemaRegistrar> schemaRegistrations;
  ApiRouteHandler handler;
};

struct NoRequest
{
};

template <int StatusCode, class T> struct TypedResponse
{
  static constexpr int statusCode = StatusCode;
  using Payload = T;
};

template <typename ErrorT> struct TypedErrorResponseSpec
{
  int status;
  std::string description;
  std::function<ErrorT(const std::string &detail)> makePayload;
  std::string contentType = "application/json";
};

template <typename ErrorT> struct TypedRouteError
{
  int status;
  ErrorT payload;
  std::string contentType = "application/json";
};

template <typename SuccessT, class ErrorT> using TypedRouteResult = std::variant<SuccessT, TypedRouteError<ErrorT>>;

template <typename Handler, class ErrorT, class SuccessT>
concept TypedNoRequestHandler = std::same_as<std::invoke_result_t<Handler, const httplib::Request &>,
                                             TypedRouteResult<typename SuccessT::Payload, ErrorT>>;

template <typename Handler, class RequestT, class ErrorT, class SuccessT>
concept TypedRequestHandler = std::same_as<std::invoke_result_t<Handler, const httplib::Request &, const RequestT &>,
                                           TypedRouteResult<typename SuccessT::Payload, ErrorT>>;

struct TypedRouteMetadata
{
  HttpMethod method;
  std::string openApiPath;
  std::string httplibPattern;
  std::string summary;
  std::string operationId;
  std::vector<OpenApiParameter> parameters;
};

template <typename ErrorT, class SuccessResponseT> struct TypedGetRouteDefinition
{
  TypedRouteMetadata metadata;
  std::string successDescription;
  std::vector<TypedErrorResponseSpec<ErrorT>> errorResponses;
};

template <typename RequestT, class ErrorT, class SuccessResponseT> struct TypedBodyRouteDefinition
{
  TypedRouteMetadata metadata;
  bool requestBodyRequired = true;
  std::string successDescription;
  TypedErrorResponseSpec<ErrorT> parseErrorResponse;
  std::vector<TypedErrorResponseSpec<ErrorT>> errorResponses;
};

OpenApiOperation MakeOpenApiOperation(const TypedRouteMetadata &metadata, std::optional<OpenApiRequestBody> requestBody,
                                      std::vector<OpenApiResponse> responses);

std::string_view ToOpenApiMethod(HttpMethod method);

void WriteSerializedJsonResponse(httplib::Response &response, int status, std::string body,
                                 const std::string &contentType = "application/json");

template <typename ErrorT>
OpenApiResponse MakeTypedErrorOpenApiResponse(const TypedErrorResponseSpec<ErrorT> &errorResponse)
{
  return OpenApiResponse::fromType<ErrorT>(std::to_string(errorResponse.status), errorResponse.description,
                                           errorResponse.contentType);
}

template <typename ErrorT>
TypedRouteError<ErrorT> MakeTypedRouteError(const TypedErrorResponseSpec<ErrorT> &errorResponse,
                                            const std::string &detail = "")
{
  return TypedRouteError<ErrorT>{
      .status = errorResponse.status,
      .payload = errorResponse.makePayload(detail),
      .contentType = errorResponse.contentType,
  };
}

template <typename ErrorT, class SuccessResponseT>
ApiRoute MakeTypedGetRoute(const TypedGetRouteDefinition<ErrorT, SuccessResponseT> &definition,
                           TypedNoRequestHandler<ErrorT, SuccessResponseT> auto handler)
{
  std::vector<OpenApiResponse> responses = {
      OpenApiResponse::fromType<typename SuccessResponseT::Payload>(std::to_string(SuccessResponseT::statusCode),
                                                                    definition.successDescription),
  };

  for (const auto &errorResponse : definition.errorResponses)
  {
    responses.push_back(MakeTypedErrorOpenApiResponse(errorResponse));
  }

  std::vector<OpenApiSchemaRegistrar> schemaRegistrations = {
      MakeOpenApiSchemaRegistration<typename SuccessResponseT::Payload>(),
  };

  if (!definition.errorResponses.empty())
  {
    schemaRegistrations.push_back(MakeOpenApiSchemaRegistration<ErrorT>());
  }

  return ApiRoute{
      .method = definition.metadata.method,
      .openApiPath = definition.metadata.openApiPath,
      .httplibPattern = definition.metadata.httplibPattern,
      .operation = MakeOpenApiOperation(definition.metadata, std::nullopt, std::move(responses)),
      .schemaRegistrations = std::move(schemaRegistrations),
      .handler =
          [handler = std::move(handler)](const httplib::Request &request, httplib::Response &response)
      {
        const auto result = handler(request);
        if (std::holds_alternative<typename SuccessResponseT::Payload>(result))
        {
          WriteSerializedJsonResponse(response, SuccessResponseT::statusCode,
                                      rfl::json::write(std::get<typename SuccessResponseT::Payload>(result)));
          return;
        }

        const auto &error = std::get<TypedRouteError<ErrorT>>(result);
        WriteSerializedJsonResponse(response, error.status, rfl::json::write(error.payload), error.contentType);
      },
  };
}

template <typename RequestT, class ErrorT, class SuccessResponseT>
ApiRoute MakeTypedBodyRoute(const TypedBodyRouteDefinition<RequestT, ErrorT, SuccessResponseT> &definition,
                            TypedRequestHandler<RequestT, ErrorT, SuccessResponseT> auto handler)
{
  std::vector<OpenApiResponse> responses = {
      OpenApiResponse::fromType<typename SuccessResponseT::Payload>(std::to_string(SuccessResponseT::statusCode),
                                                                    definition.successDescription),
      MakeTypedErrorOpenApiResponse(definition.parseErrorResponse),
  };

  for (const auto &errorResponse : definition.errorResponses)
  {
    responses.push_back(MakeTypedErrorOpenApiResponse(errorResponse));
  }

  std::vector<OpenApiSchemaRegistrar> schemaRegistrations = {
      MakeOpenApiSchemaRegistration<RequestT>(),
      MakeOpenApiSchemaRegistration<typename SuccessResponseT::Payload>(),
      MakeOpenApiSchemaRegistration<ErrorT>(),
  };

  return ApiRoute{
      .method = definition.metadata.method,
      .openApiPath = definition.metadata.openApiPath,
      .httplibPattern = definition.metadata.httplibPattern,
      .operation = MakeOpenApiOperation(definition.metadata,
                                        OpenApiRequestBody::fromType<RequestT>(definition.requestBodyRequired),
                                        std::move(responses)),
      .schemaRegistrations = std::move(schemaRegistrations),
      .handler =
          [definition, handler = std::move(handler)](const httplib::Request &request, httplib::Response &response)
      {
        if (request.body.empty() && !definition.requestBodyRequired)
        {
          if constexpr (std::default_initializable<RequestT>)
          {
            const auto result = handler(request, RequestT{});
            if (std::holds_alternative<typename SuccessResponseT::Payload>(result))
            {
              WriteSerializedJsonResponse(response, SuccessResponseT::statusCode,
                                          rfl::json::write(std::get<typename SuccessResponseT::Payload>(result)));
              return;
            }

            const auto &error = std::get<TypedRouteError<ErrorT>>(result);
            WriteSerializedJsonResponse(response, error.status, rfl::json::write(error.payload), error.contentType);
            return;
          }
        }

        const auto parsed = rfl::json::read<RequestT>(request.body);
        if (!parsed)
        {
          const auto error = MakeTypedRouteError(definition.parseErrorResponse, parsed.error().what());
          WriteSerializedJsonResponse(response, error.status, rfl::json::write(error.payload), error.contentType);
          return;
        }

        const auto result = handler(request, parsed.value());
        if (std::holds_alternative<typename SuccessResponseT::Payload>(result))
        {
          WriteSerializedJsonResponse(response, SuccessResponseT::statusCode,
                                      rfl::json::write(std::get<typename SuccessResponseT::Payload>(result)));
          return;
        }

        const auto &error = std::get<TypedRouteError<ErrorT>>(result);
        WriteSerializedJsonResponse(response, error.status, rfl::json::write(error.payload), error.contentType);
      },
  };
}

std::vector<clam::OpenApiPathItem> BuildOpenApiPaths(const std::vector<clam::ApiRoute> &apiRoutes);

std::vector<clam::OpenApiSchemaRegistrar> BuildOpenApiSchemaRegistrations(const std::vector<clam::ApiRoute> &apiRoutes);

}
