#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <httplib.h>

#include "openapi_builder.hpp"

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

template <class ErrorT> struct TypedErrorResponseSpec
{
  int status;
  std::string description;
  std::function<ErrorT(const std::string &detail)> makePayload;
  std::string contentType = "application/json";
};

template <class ErrorT> struct TypedRouteError
{
  int status;
  ErrorT payload;
};

template <class SuccessT, class ErrorT> using TypedRouteResult = std::variant<SuccessT, TypedRouteError<ErrorT>>;

template <class Handler, class ErrorT, class SuccessT>
concept TypedNoRequestHandler = std::same_as<std::invoke_result_t<Handler, const httplib::Request &>,
                                             TypedRouteResult<typename SuccessT::Payload, ErrorT>>;

template <class Handler, class RequestT, class ErrorT, class SuccessT>
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

template <class ErrorT, class SuccessResponseT> struct TypedGetRouteDefinition
{
  TypedRouteMetadata metadata;
  std::string successDescription;
  std::vector<TypedErrorResponseSpec<ErrorT>> errorResponses;
};

template <class RequestT, class ErrorT, class SuccessResponseT> struct TypedBodyRouteDefinition
{
  TypedRouteMetadata metadata;
  bool requestBodyRequired = true;
  std::string successDescription;
  TypedErrorResponseSpec<ErrorT> parseErrorResponse;
  std::vector<TypedErrorResponseSpec<ErrorT>> errorResponses;
};

OpenApiOperation makeOpenApiOperation(const TypedRouteMetadata &metadata, std::optional<OpenApiRequestBody> requestBody,
                                      std::vector<OpenApiResponse> responses);

std::string_view toOpenApiMethod(HttpMethod method);

void writeSerializedJsonResponse(httplib::Response &response, int status, std::string body,
                                 const std::string &contentType = "application/json");

template <class ErrorT>
OpenApiResponse makeTypedErrorOpenApiResponse(const TypedErrorResponseSpec<ErrorT> &errorResponse)
{
  return OpenApiResponse::fromType<ErrorT>(std::to_string(errorResponse.status), errorResponse.description,
                                           errorResponse.contentType);
}

template <class ErrorT>
TypedRouteError<ErrorT> makeTypedRouteError(const TypedErrorResponseSpec<ErrorT> &errorResponse,
                                            const std::string &detail = "")
{
  return TypedRouteError<ErrorT>{
      .status = errorResponse.status,
      .payload = errorResponse.makePayload(detail),
  };
}

template <class ErrorT, class SuccessResponseT>
ApiRoute makeTypedGetRoute(const TypedGetRouteDefinition<ErrorT, SuccessResponseT> &definition,
                           TypedNoRequestHandler<ErrorT, SuccessResponseT> auto handler)
{
  std::vector<OpenApiResponse> responses = {
      OpenApiResponse::fromType<typename SuccessResponseT::Payload>(std::to_string(SuccessResponseT::statusCode),
                                                                    definition.successDescription),
  };

  for (const auto &errorResponse : definition.errorResponses)
  {
    responses.push_back(makeTypedErrorOpenApiResponse(errorResponse));
  }

  std::vector<OpenApiSchemaRegistrar> schemaRegistrations = {
      makeOpenApiSchemaRegistration<typename SuccessResponseT::Payload>(),
  };

  if (!definition.errorResponses.empty())
  {
    schemaRegistrations.push_back(makeOpenApiSchemaRegistration<ErrorT>());
  }

  return ApiRoute{
      .method = definition.metadata.method,
      .openApiPath = definition.metadata.openApiPath,
      .httplibPattern = definition.metadata.httplibPattern,
      .operation = makeOpenApiOperation(definition.metadata, std::nullopt, std::move(responses)),
      .schemaRegistrations = std::move(schemaRegistrations),
      .handler =
          [handler = std::move(handler)](const httplib::Request &request, httplib::Response &response)
      {
        const auto result = handler(request);
        if (std::holds_alternative<typename SuccessResponseT::Payload>(result))
        {
          writeSerializedJsonResponse(response, SuccessResponseT::statusCode,
                                      rfl::json::write(std::get<typename SuccessResponseT::Payload>(result)));
          return;
        }

        const auto &error = std::get<TypedRouteError<ErrorT>>(result);
        writeSerializedJsonResponse(response, error.status, rfl::json::write(error.payload));
      },
  };
}

template <class RequestT, class ErrorT, class SuccessResponseT>
ApiRoute makeTypedBodyRoute(const TypedBodyRouteDefinition<RequestT, ErrorT, SuccessResponseT> &definition,
                            TypedRequestHandler<RequestT, ErrorT, SuccessResponseT> auto handler)
{
  std::vector<OpenApiResponse> responses = {
      OpenApiResponse::fromType<typename SuccessResponseT::Payload>(std::to_string(SuccessResponseT::statusCode),
                                                                    definition.successDescription),
      makeTypedErrorOpenApiResponse(definition.parseErrorResponse),
  };

  for (const auto &errorResponse : definition.errorResponses)
  {
    responses.push_back(makeTypedErrorOpenApiResponse(errorResponse));
  }

  std::vector<OpenApiSchemaRegistrar> schemaRegistrations = {
      makeOpenApiSchemaRegistration<RequestT>(),
      makeOpenApiSchemaRegistration<typename SuccessResponseT::Payload>(),
      makeOpenApiSchemaRegistration<ErrorT>(),
  };

  return ApiRoute{
      .method = definition.metadata.method,
      .openApiPath = definition.metadata.openApiPath,
      .httplibPattern = definition.metadata.httplibPattern,
      .operation = makeOpenApiOperation(definition.metadata,
                                        OpenApiRequestBody::fromType<RequestT>(definition.requestBodyRequired),
                                        std::move(responses)),
      .schemaRegistrations = std::move(schemaRegistrations),
      .handler =
          [definition, handler = std::move(handler)](const httplib::Request &request, httplib::Response &response)
      {
        const auto parsed = rfl::json::read<RequestT>(request.body);
        if (!parsed)
        {
          const auto error = makeTypedRouteError(definition.parseErrorResponse, parsed.error().what());
          writeSerializedJsonResponse(response, error.status, rfl::json::write(error.payload));
          return;
        }

        const auto result = handler(request, parsed.value());
        if (std::holds_alternative<typename SuccessResponseT::Payload>(result))
        {
          writeSerializedJsonResponse(response, SuccessResponseT::statusCode,
                                      rfl::json::write(std::get<typename SuccessResponseT::Payload>(result)));
          return;
        }

        const auto &error = std::get<TypedRouteError<ErrorT>>(result);
        writeSerializedJsonResponse(response, error.status, rfl::json::write(error.payload));
      },
  };
}

} // namespace clam
