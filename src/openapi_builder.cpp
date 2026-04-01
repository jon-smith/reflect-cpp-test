#include "openapi_builder.hpp"

#include <expected>
#include <initializer_list>
#include <string>
#include <string_view>

namespace
{

using Json = OpenApiJson;
using JsonArray = Json::Array;
using JsonObject = Json::Object;
using ExpectedJson = std::expected<Json, std::string>;
using ExpectedVoid = std::expected<void, std::string>;

Json makeArray(const std::initializer_list<Json> values)
{
  return JsonArray(values);
}

ExpectedJson parseJson(const std::string &json)
{
  const auto result = rfl::json::read<Json>(json);
  if (!result)
  {
    return std::unexpected("Failed to parse JSON: " + result.error().what());
  }
  return result.value();
}

ExpectedVoid rewriteSchemaRefs(Json &node)
{
  if (const auto objectResult = node.to_object(); objectResult)
  {
    auto object = objectResult.value();

    if (object.count("$ref") != 0U)
    {
      const auto ref = toString(object.at("$ref"), "$ref");
      if (!ref)
      {
        return std::unexpected(ref.error());
      }
      if (ref.value().starts_with("#/$defs/"))
      {
        object["$ref"] = "#/components/schemas/" + ref.value().substr(8);
      }
    }

    for (auto &[_, value] : object)
    {
      const auto rewritten = rewriteSchemaRefs(value);
      if (!rewritten)
      {
        return rewritten;
      }
    }

    node = std::move(object);
    return {};
  }

  if (const auto arrayResult = node.to_array(); arrayResult)
  {
    auto array = arrayResult.value();
    for (auto &value : array)
    {
      const auto rewritten = rewriteSchemaRefs(value);
      if (!rewritten)
      {
        return rewritten;
      }
    }
    node = std::move(array);
  }

  return {};
}

Json schemaRef(const std::string &name)
{
  return makeObject({{"$ref", "#/components/schemas/" + name}});
}

Json makeContentObject(const std::string &contentType, const Json &schema)
{
  return makeObject({{contentType, makeObject({{"schema", schema}})}});
}

Json makeResponseObject(const OpenApiResponse &response)
{
  return makeObject({{"description", response.description},
                     {"content", makeContentObject(response.contentType, schemaRef(response.schemaName))}});
}

Json makeParameterObject(const OpenApiParameter &parameter)
{
  return makeObject({
      {"name", parameter.name},
      {"in", parameter.in},
      {"required", parameter.required},
      {"description", parameter.description},
      {"schema", parameter.schema},
  });
}

Json makeOperationObject(const OpenApiOperation &operation)
{
  JsonObject object;
  object["summary"] = operation.summary;
  object["operationId"] = operation.operationId;

  if (!operation.parameters.empty())
  {
    JsonArray parameters;
    for (const auto &parameter : operation.parameters)
    {
      parameters.emplace_back(makeParameterObject(parameter));
    }
    object["parameters"] = std::move(parameters);
  }

  if (operation.requestBody)
  {
    object["requestBody"] = makeObject({{"required", operation.requestBody->required},
                                        {"content", makeContentObject(operation.requestBody->contentType,
                                                                      schemaRef(operation.requestBody->schemaName))}});
  }

  JsonObject responses;
  for (const auto &response : operation.responses)
  {
    responses[response.statusCode] = makeResponseObject(response);
  }
  object["responses"] = std::move(responses);

  return object;
}

Json makePathsObject(const std::vector<OpenApiPathItem> &paths)
{
  JsonObject pathObjects;
  for (const auto &pathItem : paths)
  {
    JsonObject operations;
    for (const auto &binding : pathItem.operations)
    {
      operations[binding.method] = makeOperationObject(binding.operation);
    }
    pathObjects[pathItem.path] = std::move(operations);
  }

  return pathObjects;
}

}

OpenApiJson makeObject(const std::initializer_list<std::pair<std::string, OpenApiJson>> fields)
{
  JsonObject object;
  for (const auto &[key, value] : fields)
  {
    object.insert(key, value);
  }
  return object;
}

OpenApiExpectedJsonObject toObject(const OpenApiJson &json, const std::string_view context)
{
  const auto result = json.to_object();
  if (!result)
  {
    return std::unexpected("Expected JSON object for " + std::string(context) + ": " + result.error().what());
  }
  return result.value();
}

OpenApiExpectedString toString(const OpenApiJson &json, const std::string_view context)
{
  const auto result = json.to_string();
  if (!result)
  {
    return std::unexpected("Expected JSON string for " + std::string(context) + ": " + result.error().what());
  }
  return result.value();
}

OpenApiExpectedBool containsSchemaRef(const OpenApiJson &node, const std::string &targetRef)
{
  if (const auto objectResult = node.to_object(); objectResult)
  {
    const auto object = objectResult.value();

    if (object.count("$ref") != 0U)
    {
      const auto ref = toString(object.at("$ref"), "$ref");
      if (!ref)
      {
        return std::unexpected(ref.error());
      }
      if (ref.value() == targetRef)
      {
        return true;
      }
    }

    for (const auto &[_, value] : object)
    {
      const auto contains = containsSchemaRef(value, targetRef);
      if (!contains)
      {
        return std::unexpected(contains.error());
      }
      if (contains.value())
      {
        return true;
      }
    }
  }

  if (const auto arrayResult = node.to_array(); arrayResult)
  {
    const auto array = arrayResult.value();
    for (const auto &value : array)
    {
      const auto contains = containsSchemaRef(value, targetRef);
      if (!contains)
      {
        return std::unexpected(contains.error());
      }
      if (contains.value())
      {
        return true;
      }
    }
  }

  return false;
}

OpenApiExpectedBool arrayContainsString(const OpenApiJson &json, const std::string_view expectedValue)
{
  const auto array = json.to_array();
  if (!array)
  {
    return std::unexpected("Expected JSON array while checking enum values: " + array.error().what());
  }

  for (const auto &value : array.value())
  {
    const auto stringValue = toString(value, "array value");
    if (!stringValue)
    {
      return std::unexpected(stringValue.error());
    }
    if (stringValue.value() == expectedValue)
    {
      return true;
    }
  }

  return false;
}

OpenApiJson stringSchema()
{
  return makeObject({{"type", "string"}});
}

OpenApiResponse errorResponse(const std::string &statusCode, const std::string &description)
{
  return OpenApiResponse{
      .statusCode = statusCode,
      .description = description,
      .schemaName = "ErrorResponse",
  };
}

ExpectedVoid registerGeneratedSchemaDocument(const std::string &schemaJson, JsonObject &schemas)
{
  const auto schemaDocument = parseJson(schemaJson);
  if (!schemaDocument)
  {
    return std::unexpected(schemaDocument.error());
  }

  const auto schemaRoot = toObject(schemaDocument.value(), "reflect-cpp JSON schema");
  if (!schemaRoot)
  {
    return std::unexpected(schemaRoot.error());
  }

  auto definitions = toObject(schemaRoot.value().at("$defs"), "$defs");
  if (!definitions)
  {
    return std::unexpected(definitions.error());
  }

  for (auto &[name, schema] : definitions.value())
  {
    const auto rewritten = rewriteSchemaRefs(schema);
    if (!rewritten)
    {
      return rewritten;
    }
    schemas[name] = std::move(schema);
  }

  return {};
}

std::expected<OpenApiJson, std::string> buildOpenApiSpec(const OpenApiSpecConfig &config)
{
  JsonObject schemas;
  for (const auto &registerSchema : config.schemaRegistrations)
  {
    const auto result = registerSchema(schemas);
    if (!result)
    {
      return std::unexpected(result.error());
    }
  }

  JsonObject document;
  document["openapi"] = "3.1.0";
  document["info"] = makeObject({
      {"title", config.info.title},
      {"version", config.info.version},
      {"description", config.info.description},
  });

  if (!config.servers.empty())
  {
    JsonArray servers;
    for (const auto &server : config.servers)
    {
      servers.emplace_back(makeObject({
          {"url", server.url},
          {"description", server.description},
      }));
    }
    document["servers"] = std::move(servers);
  }

  document["paths"] = makePathsObject(config.paths);
  document["components"] = makeObject({{"schemas", std::move(schemas)}});
  return document;
}
