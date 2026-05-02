#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <rfl.hpp>
#include <rfl/json.hpp>

namespace clam
{

using OpenApiJson = rfl::Generic;
using OpenApiSchemaRegistrar = std::function<std::expected<void, std::string>(OpenApiJson::Object &)>;
using OpenApiExpectedJsonObject = std::expected<OpenApiJson::Object, std::string>;
using OpenApiExpectedString = std::expected<std::string, std::string>;
using OpenApiExpectedBool = std::expected<bool, std::string>;

template <typename T> struct OpenApiSchemaTraits;

template <typename T> std::string OpenApiSchemaName()
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

  template <typename T>
  static OpenApiRequestBody fromType(bool required = true, std::string contentType = "application/json")
  {
    return OpenApiRequestBody{
        .required = required,
        .contentType = std::move(contentType),
        .schemaName = OpenApiSchemaName<T>(),
    };
  }
};

struct OpenApiResponse
{
  std::string statusCode;
  std::string description;
  std::string schemaName;
  std::string contentType = "application/json";

  template <typename T>
  static OpenApiResponse fromType(std::string statusCode, std::string description,
                                  std::string contentType = "application/json")
  {
    return OpenApiResponse{
        .statusCode = std::move(statusCode),
        .description = std::move(description),
        .schemaName = OpenApiSchemaName<T>(),
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

OpenApiJson MakeObject(std::initializer_list<std::pair<std::string, OpenApiJson>> fields);

OpenApiExpectedJsonObject ToObject(const OpenApiJson &json, std::string_view context);

OpenApiExpectedString ToString(const OpenApiJson &json, std::string_view context);

OpenApiExpectedBool ContainsSchemaRef(const OpenApiJson &node, const std::string &targetRef);

OpenApiExpectedBool ArrayContainsString(const OpenApiJson &json, std::string_view expectedValue);

OpenApiJson StringSchema();

OpenApiResponse ErrorResponse(const std::string &statusCode, const std::string &description);

std::expected<void, std::string> RegisterGeneratedSchemaDocument(const std::string &schemaJson,
                                                                 OpenApiJson::Object &schemas);

std::expected<OpenApiJson, std::string> BuildOpenApiSpec(const OpenApiSpecConfig &config);

template <typename T> OpenApiSchemaRegistrar MakeOpenApiSchemaRegistration()
{
  return [](OpenApiJson::Object &schemas) -> std::expected<void, std::string>
  { return RegisterGeneratedSchemaDocument(rfl::json::to_schema<T>(), schemas); };
}

}
