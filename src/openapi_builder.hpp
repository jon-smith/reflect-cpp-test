#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <rfl.hpp>
#include <rfl/json.hpp>

using OpenApiJson = rfl::Generic;
using OpenApiSchemaRegistrar = std::function<std::expected<void, std::string>(OpenApiJson::Object &)>;
using OpenApiExpectedJsonObject = std::expected<OpenApiJson::Object, std::string>;
using OpenApiExpectedString = std::expected<std::string, std::string>;
using OpenApiExpectedBool = std::expected<bool, std::string>;

template <class T> struct OpenApiSchemaTraits;

template <class T> std::string openApiSchemaName()
{
  return std::string(OpenApiSchemaTraits<T>::name);
}

struct OpenApiInfo
{
  std::string title;
  std::string version;
  std::string description;
};

struct OpenApiServer
{
  std::string url;
  std::string description;
};

struct OpenApiParameter
{
  std::string name;
  std::string in;
  bool required;
  std::string description;
  OpenApiJson schema;
};

struct OpenApiRequestBody
{
  bool required;
  std::string contentType = "application/json";
  std::string schemaName;

  template <class T>
  static OpenApiRequestBody fromType(bool required = true, std::string contentType = "application/json")
  {
    return OpenApiRequestBody{
        .required = required,
        .contentType = std::move(contentType),
        .schemaName = openApiSchemaName<T>(),
    };
  }
};

struct OpenApiResponse
{
  std::string statusCode;
  std::string description;
  std::string schemaName;
  std::string contentType = "application/json";

  template <class T>
  static OpenApiResponse fromType(std::string statusCode, std::string description,
                                  std::string contentType = "application/json")
  {
    return OpenApiResponse{
        .statusCode = std::move(statusCode),
        .description = std::move(description),
        .schemaName = openApiSchemaName<T>(),
        .contentType = std::move(contentType),
    };
  }
};

struct OpenApiOperation
{
  std::string summary;
  std::string operationId;
  std::vector<OpenApiParameter> parameters;
  std::optional<OpenApiRequestBody> requestBody;
  std::vector<OpenApiResponse> responses;
};

struct OpenApiPathOperation
{
  std::string method;
  OpenApiOperation operation;
};

struct OpenApiPathItem
{
  std::string path;
  std::vector<OpenApiPathOperation> operations;
};

struct OpenApiSpecConfig
{
  OpenApiInfo info;
  std::vector<OpenApiServer> servers;
  std::vector<OpenApiPathItem> paths;
  std::vector<OpenApiSchemaRegistrar> schemaRegistrations;
};

OpenApiJson makeObject(std::initializer_list<std::pair<std::string, OpenApiJson>> fields);

OpenApiExpectedJsonObject toObject(const OpenApiJson &json, std::string_view context);

OpenApiExpectedString toString(const OpenApiJson &json, std::string_view context);

OpenApiExpectedBool containsSchemaRef(const OpenApiJson &node, const std::string &targetRef);

OpenApiExpectedBool arrayContainsString(const OpenApiJson &json, std::string_view expectedValue);

OpenApiJson stringSchema();

OpenApiResponse errorResponse(const std::string &statusCode, const std::string &description);

std::expected<void, std::string> registerGeneratedSchemaDocument(const std::string &schemaJson,
                                                                 OpenApiJson::Object &schemas);

std::expected<OpenApiJson, std::string> buildOpenApiSpec(const OpenApiSpecConfig &config);

template <class T> OpenApiSchemaRegistrar makeOpenApiSchemaRegistration()
{
  return [](OpenApiJson::Object &schemas) -> std::expected<void, std::string>
  { return registerGeneratedSchemaDocument(rfl::json::to_schema<T>(), schemas); };
}
