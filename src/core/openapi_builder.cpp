#include "core/openapi_builder.hpp"

#include <expected>
#include <initializer_list>
#include <string>
#include <string_view>

namespace clam
{

namespace
{

using Json = OpenApiJson;
using JsonArray = Json::Array;
using JsonObject = Json::Object;
using ExpectedJson = std::expected<Json, std::string>;
using ExpectedVoid = std::expected<void, std::string>;

Json MakeArray(const std::initializer_list<Json> values)
{
  return JsonArray(values);
}

ExpectedJson ParseJson(const std::string &json)
{
  const auto result = rfl::json::read<Json>(json);
  if (!result)
  {
    return std::unexpected("Failed to parse JSON: " + result.error().what());
  }
  return result.value();
}

ExpectedVoid RewriteSchemaRefs(Json &node)
{
  if (const auto objectResult = node.to_object(); objectResult)
  {
    auto object = objectResult.value();

    if (object.count("$ref") != 0U)
    {
      const auto ref = ToString(object.at("$ref"), "$ref");
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
      const auto rewritten = RewriteSchemaRefs(value);
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
      const auto rewritten = RewriteSchemaRefs(value);
      if (!rewritten)
      {
        return rewritten;
      }
    }
    node = std::move(array);
  }

  return {};
}

Json SchemaRef(const std::string &name)
{
  return MakeObject({{"$ref", "#/components/schemas/" + name}});
}

Json MakeContentObject(const std::string &contentType, const Json &schema)
{
  return MakeObject({{contentType, MakeObject({{"schema", schema}})}});
}

Json MakeResponseObject(const OpenApiResponse &response)
{
  return MakeObject({{"description", response.description},
                     {"content", MakeContentObject(response.contentType, SchemaRef(response.schemaName))}});
}

Json MakeParameterObject(const OpenApiParameter &parameter)
{
  return MakeObject({
      {"name", parameter.name},
      {"in", parameter.in},
      {"required", parameter.required},
      {"description", parameter.description},
      {"schema", parameter.schema},
  });
}

Json MakeOperationObject(const OpenApiOperation &operation)
{
  JsonObject object;
  object["summary"] = operation.summary;
  object["operationId"] = operation.operationId;

  if (!operation.parameters.empty())
  {
    JsonArray parameters;
    for (const auto &parameter : operation.parameters)
    {
      parameters.emplace_back(MakeParameterObject(parameter));
    }
    object["parameters"] = std::move(parameters);
  }

  if (operation.requestBody)
  {
    object["requestBody"] = MakeObject({{"required", operation.requestBody->required},
                                        {"content", MakeContentObject(operation.requestBody->contentType,
                                                                      SchemaRef(operation.requestBody->schemaName))}});
  }

  JsonObject responses;
  for (const auto &response : operation.responses)
  {
    responses[response.statusCode] = MakeResponseObject(response);
  }
  object["responses"] = std::move(responses);

  return object;
}

Json MakePathsObject(const std::vector<OpenApiPathItem> &paths)
{
  JsonObject pathObjects;
  for (const auto &pathItem : paths)
  {
    JsonObject operations;
    for (const auto &binding : pathItem.operations)
    {
      operations[binding.method] = MakeOperationObject(binding.operation);
    }
    pathObjects[pathItem.path] = std::move(operations);
  }

  return pathObjects;
}

}

OpenApiJson MakeObject(const std::initializer_list<std::pair<std::string, OpenApiJson>> fields)
{
  JsonObject object;
  for (const auto &[key, value] : fields)
  {
    object.insert(key, value);
  }
  return object;
}

OpenApiExpectedJsonObject ToObject(const OpenApiJson &json, const std::string_view context)
{
  const auto result = json.to_object();
  if (!result)
  {
    return std::unexpected("Expected JSON object for " + std::string(context) + ": " + result.error().what());
  }
  return result.value();
}

OpenApiExpectedString ToString(const OpenApiJson &json, const std::string_view context)
{
  const auto result = json.to_string();
  if (!result)
  {
    return std::unexpected("Expected JSON string for " + std::string(context) + ": " + result.error().what());
  }
  return result.value();
}

OpenApiExpectedBool ContainsSchemaRef(const OpenApiJson &node, const std::string &targetRef)
{
  if (const auto objectResult = node.to_object(); objectResult)
  {
    const auto object = objectResult.value();

    if (object.count("$ref") != 0U)
    {
      const auto ref = ToString(object.at("$ref"), "$ref");
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
      const auto contains = ContainsSchemaRef(value, targetRef);
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
      const auto contains = ContainsSchemaRef(value, targetRef);
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

OpenApiExpectedBool ArrayContainsString(const OpenApiJson &json, const std::string_view expectedValue)
{
  const auto array = json.to_array();
  if (!array)
  {
    return std::unexpected("Expected JSON array while checking enum values: " + array.error().what());
  }

  for (const auto &value : array.value())
  {
    const auto stringValue = ToString(value, "array value");
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

OpenApiJson StringSchema()
{
  return MakeObject({{"type", "string"}});
}

OpenApiResponse ErrorResponse(const std::string &statusCode, const std::string &description)
{
  return OpenApiResponse{
      .statusCode = statusCode,
      .description = description,
      .schemaName = "ErrorResponse",
  };
}

ExpectedVoid RegisterGeneratedSchemaDocument(const std::string &schemaJson, JsonObject &schemas)
{
  const auto schemaDocument = ParseJson(schemaJson);
  if (!schemaDocument)
  {
    return std::unexpected(schemaDocument.error());
  }

  const auto schemaRoot = ToObject(schemaDocument.value(), "reflect-cpp JSON schema");
  if (!schemaRoot)
  {
    return std::unexpected(schemaRoot.error());
  }

  if (schemaRoot.value().count("$defs") == 0U)
  {
    return std::unexpected("reflect-cpp JSON schema did not contain $defs.");
  }

  auto definitions = ToObject(schemaRoot.value().at("$defs"), "$defs");
  if (!definitions)
  {
    return std::unexpected(definitions.error());
  }

  for (auto &[name, schema] : definitions.value())
  {
    const auto rewritten = RewriteSchemaRefs(schema);
    if (!rewritten)
    {
      return rewritten;
    }
    schemas[name] = std::move(schema);
  }

  return {};
}

std::expected<OpenApiJson, std::string> BuildOpenApiSpec(const OpenApiSpecConfig &config)
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
  document["info"] = MakeObject({
      {"title", config.info.title},
      {"version", config.info.version},
      {"description", config.info.description},
  });

  if (!config.servers.empty())
  {
    JsonArray servers;
    for (const auto &server : config.servers)
    {
      servers.emplace_back(MakeObject({
          {"url", server.url},
          {"description", server.description},
      }));
    }
    document["servers"] = std::move(servers);
  }

  document["paths"] = MakePathsObject(config.paths);
  document["components"] = MakeObject({{"schemas", std::move(schemas)}});
  return document;
}

}
