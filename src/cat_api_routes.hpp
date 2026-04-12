#pragma once

#include <concepts>
#include <functional>
#include <string_view>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <httplib.h>

#include "openapi_builder.hpp"
#include "cat_api_types.hpp"

using CatApiRouteHandler = std::function<void(const httplib::Request &, httplib::Response &)>;

struct CatApiRoute
{
  std::string method;
  std::string openApiPath;
  std::string httplibPattern;
  OpenApiOperation operation;
  std::vector<OpenApiSchemaRegistrar> schemaRegistrations;
  CatApiRouteHandler handler;
};

struct NoRequest
{
};

template <int StatusCode, class T> struct TypedResponse
{
  static constexpr int statusCode = StatusCode;
  using Payload = T;
};

struct TypedErrorResponseSpec
{
  int status;
  std::string description;
  std::string code;
  std::string message;
  std::string contentType = "application/json";
};

struct TypedRouteError
{
  int status;
  ErrorResponse payload;
};

template <class T> using TypedRouteResult = std::variant<T, TypedRouteError>;

template <class Handler, class SuccessT>
concept TypedNoRequestHandler =
    std::same_as<std::invoke_result_t<Handler, const httplib::Request &>, TypedRouteResult<typename SuccessT::Payload>>;

template <class Handler, class RequestT, class SuccessT>
concept TypedRequestHandler =
    std::same_as<std::invoke_result_t<Handler, const httplib::Request &, const RequestT &>,
                 TypedRouteResult<typename SuccessT::Payload>>;

struct TypedRouteMetadata
{
  std::string method;
  std::string openApiPath;
  std::string httplibPattern;
  std::string summary;
  std::string operationId;
  std::vector<OpenApiParameter> parameters;
};

template <class SuccessResponseT> struct TypedGetRouteDefinition
{
  TypedRouteMetadata metadata;
  std::string successDescription;
  std::vector<TypedErrorResponseSpec> errorResponses;
};

template <class RequestT, class SuccessResponseT> struct TypedBodyRouteDefinition
{
  TypedRouteMetadata metadata;
  bool requestBodyRequired = true;
  std::string successDescription;
  TypedErrorResponseSpec parseErrorResponse;
  std::vector<TypedErrorResponseSpec> errorResponses;
};

OpenApiResponse makeTypedErrorOpenApiResponse(const TypedErrorResponseSpec &errorResponse);

TypedRouteError makeTypedRouteError(const TypedErrorResponseSpec &errorResponse, const std::string &detail = "");

template <class SuccessResponseT>
CatApiRoute makeTypedGetRoute(const TypedGetRouteDefinition<SuccessResponseT> &definition,
                              TypedNoRequestHandler<SuccessResponseT> auto handler)
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
    schemaRegistrations.push_back(makeOpenApiSchemaRegistration<ErrorResponse>());
  }

  return CatApiRoute{
      .method = definition.metadata.method,
      .openApiPath = definition.metadata.openApiPath,
      .httplibPattern = definition.metadata.httplibPattern,
      .operation =
          {
              .summary = definition.metadata.summary,
              .operationId = definition.metadata.operationId,
              .parameters = definition.metadata.parameters,
              .responses = std::move(responses),
          },
      .schemaRegistrations = std::move(schemaRegistrations),
      .handler =
          [handler = std::move(handler)](const httplib::Request &request, httplib::Response &response)
      {
        const auto result = handler(request);
        if (std::holds_alternative<typename SuccessResponseT::Payload>(result))
        {
          response.status = SuccessResponseT::statusCode;
          response.set_content(rfl::json::write(std::get<typename SuccessResponseT::Payload>(result)),
                               "application/json");
          return;
        }

        const auto &error = std::get<TypedRouteError>(result);
        response.status = error.status;
        response.set_content(rfl::json::write(error.payload), "application/json");
      },
  };
}

template <class RequestT, class SuccessResponseT>
CatApiRoute makeTypedBodyRoute(const TypedBodyRouteDefinition<RequestT, SuccessResponseT> &definition,
                               TypedRequestHandler<RequestT, SuccessResponseT> auto handler)
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
      makeOpenApiSchemaRegistration<ErrorResponse>(),
  };

  return CatApiRoute{
      .method = definition.metadata.method,
      .openApiPath = definition.metadata.openApiPath,
      .httplibPattern = definition.metadata.httplibPattern,
      .operation =
          {
              .summary = definition.metadata.summary,
              .operationId = definition.metadata.operationId,
              .parameters = definition.metadata.parameters,
              .requestBody = OpenApiRequestBody::fromType<RequestT>(definition.requestBodyRequired),
              .responses = std::move(responses),
          },
      .schemaRegistrations = std::move(schemaRegistrations),
      .handler =
          [definition, handler = std::move(handler)](const httplib::Request &request, httplib::Response &response)
      {
        const auto parsed = rfl::json::read<RequestT>(request.body);
        if (!parsed)
        {
          const auto error = makeTypedRouteError(definition.parseErrorResponse, parsed.error().what());
          response.status = error.status;
          response.set_content(rfl::json::write(error.payload), "application/json");
          return;
        }

        const auto result = handler(request, parsed.value());
        if (std::holds_alternative<typename SuccessResponseT::Payload>(result))
        {
          response.status = SuccessResponseT::statusCode;
          response.set_content(rfl::json::write(std::get<typename SuccessResponseT::Payload>(result)),
                               "application/json");
          return;
        }

        const auto &error = std::get<TypedRouteError>(result);
        response.status = error.status;
        response.set_content(rfl::json::write(error.payload), "application/json");
      },
  };
}

std::vector<CatApiRoute> makeCatApiRoutes();
