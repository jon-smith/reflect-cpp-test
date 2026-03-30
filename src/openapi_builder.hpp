#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <rfl.hpp>
#include <rfl/json.hpp>

using OpenApiJson = rfl::Generic;
using OpenApiSchemaRegistrar =
    std::function<std::expected<void, std::string>(OpenApiJson::Object &)>;

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
};

struct OpenApiResponse
{
  std::string statusCode;
  std::string description;
  std::string schemaName;
  std::string contentType = "application/json";
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

std::expected<void, std::string>
registerGeneratedSchemaDocument(const std::string &schemaJson,
                                OpenApiJson::Object &schemas);

std::expected<OpenApiJson, std::string>
buildOpenApiSpec(const OpenApiSpecConfig &config);

template <class T>
OpenApiSchemaRegistrar makeOpenApiSchemaRegistration()
{
  return [](OpenApiJson::Object &schemas) -> std::expected<void, std::string> {
    return registerGeneratedSchemaDocument(rfl::json::to_schema<T>(), schemas);
  };
}
