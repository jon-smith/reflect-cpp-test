#include "openapi_demo.hpp"

#include <array>
#include <expected>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <rfl/json.hpp>

#include "cat_api_types.hpp"

namespace
{

using Json = rfl::Generic;
using JsonArray = Json::Array;
using JsonObject = Json::Object;
using ExpectedJson = std::expected<Json, std::string>;
using ExpectedJsonObject = std::expected<JsonObject, std::string>;
using ExpectedString = std::expected<std::string, std::string>;
using ExpectedVoid = std::expected<void, std::string>;
using ExpectedBool = std::expected<bool, std::string>;

Json makeObject(
    const std::initializer_list<std::pair<std::string, Json>> fields)
{
  JsonObject object;
  for (const auto &[key, value] : fields)
  {
    object.insert(key, value);
  }
  return object;
}

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

ExpectedJsonObject toObject(const Json &json, const std::string_view context)
{
  const auto result = json.to_object();
  if (!result)
  {
    return std::unexpected("Expected JSON object for " + std::string(context) +
                           ": " + result.error().what());
  }
  return result.value();
}

ExpectedString toString(const Json &json, const std::string_view context)
{
  const auto result = json.to_string();
  if (!result)
  {
    return std::unexpected("Expected JSON string for " + std::string(context) +
                           ": " + result.error().what());
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

template <class T>
ExpectedVoid registerSchema(JsonObject &schemas)
{
  const auto schemaJson = parseJson(rfl::json::to_schema<T>());
  if (!schemaJson)
  {
    return std::unexpected(schemaJson.error());
  }

  const auto schemaDocument =
      toObject(schemaJson.value(), "reflect-cpp JSON schema");
  if (!schemaDocument)
  {
    return std::unexpected(schemaDocument.error());
  }

  auto definitions = toObject(schemaDocument.value().at("$defs"), "$defs");
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

Json schemaRef(const std::string &name)
{
  return makeObject({{"$ref", "#/components/schemas/" + name}});
}

Json jsonResponse(const std::string &description, const std::string &schemaName)
{
  return makeObject(
      {{"description", description},
       {"content",
        makeObject({{"application/json",
                     makeObject({{"schema", schemaRef(schemaName)}})}})}});
}

Json errorResponse(const std::string &description)
{
  return jsonResponse(description, "ErrorResponse");
}

ExpectedBool containsSchemaRef(const Json &node, const std::string &targetRef)
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

ExpectedBool arrayContainsString(const Json &json,
                                 const std::string_view expectedValue)
{
  const auto array = json.to_array();
  if (!array)
  {
    return std::unexpected("Expected JSON array while checking enum values: " +
                           array.error().what());
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

} // namespace

std::expected<rfl::Generic, std::string> buildOpenApiSpec()
{
  JsonObject schemas;
  for (const auto result : {registerSchema<Cat>(schemas),
                            registerSchema<CatSummary>(schemas),
                            registerSchema<CreateCatRequest>(schemas),
                            registerSchema<CatLogEntry>(schemas),
                            registerSchema<CreateCatLogEntryRequest>(schemas),
                            registerSchema<CatListResponse>(schemas),
                            registerSchema<CatLogListResponse>(schemas),
                            registerSchema<ErrorResponse>(schemas)})
  {
    if (!result)
    {
      return std::unexpected(result.error());
    }
  }

  return makeObject({
      {"openapi", "3.1.0"},
      {"info",
       makeObject({
           {"title", "reflect-cpp CatLog API"},
           {"version", "1.0.0"},
           {"description",
            "OpenAPI 3.1 document assembled from reflect-cpp-generated JSON "
            "Schema components for a CatLog API."},
       })},
      {"servers",
       makeArray({makeObject({
           {"url", "http://localhost:8080"},
           {"description", "Local development server"},
       })})},
      {"paths",
       makeObject({
           {"/cats",
            makeObject({
                {"get",
                 makeObject({
                     {"summary", "List cats"},
                     {"operationId", "listCats"},
                     {"responses",
                      makeObject({
                          {"200", jsonResponse("A list of cats.", "CatListResponse")},
                          {"500", errorResponse("Unexpected server error.")},
                      })},
                 })},
                {"post",
                 makeObject({
                     {"summary", "Create a cat"},
                     {"operationId", "createCat"},
                     {"requestBody",
                      makeObject({
                          {"required", true},
                          {"content",
                           makeObject({{"application/json",
                                        makeObject({{"schema",
                                                     schemaRef("CreateCatRequest")}})}})},
                      })},
                     {"responses",
                      makeObject({
                          {"201", jsonResponse("The created cat.", "Cat")},
                          {"400", errorResponse("The request body was invalid.")},
                      })},
                 })},
            })},
           {"/cats/{catId}",
            makeObject({
                {"get",
                 makeObject({
                     {"summary", "Get a cat by id"},
                     {"operationId", "getCatById"},
                     {"parameters",
                      makeArray({makeObject({
                          {"name", "catId"},
                          {"in", "path"},
                          {"required", true},
                          {"description",
                           "Unique identifier for a previously created cat."},
                          {"schema", makeObject({{"type", "string"}})},
                      })})},
                     {"responses",
                      makeObject({
                          {"200", jsonResponse("The requested cat.", "Cat")},
                          {"404", errorResponse("The cat was not found.")},
                      })},
                 })},
            })},
           {"/cats/{catId}/logs",
            makeObject({
                {"get",
                 makeObject({
                     {"summary", "List cat status logs"},
                     {"operationId", "listCatLogs"},
                     {"parameters",
                      makeArray({makeObject({
                          {"name", "catId"},
                          {"in", "path"},
                          {"required", true},
                          {"description",
                           "Unique identifier for the cat whose logs are being requested."},
                          {"schema", makeObject({{"type", "string"}})},
                      })})},
                     {"responses",
                      makeObject({
                          {"200",
                           jsonResponse("Status log entries for the cat.",
                                        "CatLogListResponse")},
                          {"404", errorResponse("The cat was not found.")},
                      })},
                 })},
                {"post",
                 makeObject({
                     {"summary", "Create a cat status log entry"},
                     {"operationId", "createCatLogEntry"},
                     {"parameters",
                      makeArray({makeObject({
                          {"name", "catId"},
                          {"in", "path"},
                          {"required", true},
                          {"description",
                           "Unique identifier for the cat receiving the new log entry."},
                          {"schema", makeObject({{"type", "string"}})},
                      })})},
                     {"requestBody",
                      makeObject({
                          {"required", true},
                          {"content",
                           makeObject({{"application/json",
                                        makeObject(
                                            {{"schema",
                                              schemaRef("CreateCatLogEntryRequest")}})}})},
                      })},
                     {"responses",
                      makeObject({
                          {"201",
                           jsonResponse("The created log entry.", "CatLogEntry")},
                          {"400", errorResponse("The request body was invalid.")},
                          {"404", errorResponse("The cat was not found.")},
                      })},
                 })},
            })},
       })},
      {"components", makeObject({{"schemas", std::move(schemas)}})},
  });
}

int runChecks()
{
  std::vector<std::string> errors;
  const auto spec = buildOpenApiSpec();
  if (!spec)
  {
    errors.emplace_back(spec.error());
  }
  else
  {
    const auto document = toObject(spec.value(), "OpenAPI document");
    if (!document)
    {
      errors.emplace_back(document.error());
    }
    else
    {
      const auto openapi = toString(document.value().at("openapi"), "openapi");
      if (!openapi)
      {
        errors.emplace_back(openapi.error());
      }
      else if (openapi.value() != "3.1.0")
      {
        errors.emplace_back("Expected OpenAPI version 3.1.0.");
      }

      const auto components =
          toObject(document.value().at("components"), "components");
      if (!components)
      {
        errors.emplace_back(components.error());
      }
      else
      {
        const auto schemas =
            toObject(components.value().at("schemas"), "schemas");
        if (!schemas)
        {
          errors.emplace_back(schemas.error());
        }
        else
        {
          for (const std::string_view name :
               std::array{"Cat",
                          "CatSummary",
                          "CreateCatRequest",
                          "CatLogEntry",
                          "CreateCatLogEntryRequest",
                          "CatListResponse",
                          "CatLogListResponse",
                          "ErrorResponse"})
          {
            if (schemas.value().count(std::string(name)) == 0U)
            {
              errors.emplace_back("Missing component schema: " +
                                  std::string(name));
            }
          }

          if (schemas.value().count("Cat") != 0U)
          {
            const auto catSchema =
                toObject(schemas.value().at("Cat"), "Cat schema");
            if (!catSchema)
            {
              errors.emplace_back(catSchema.error());
            }
            else
            {
              const auto catProperties =
                  toObject(catSchema.value().at("properties"), "Cat.properties");
              if (!catProperties)
              {
                errors.emplace_back(catProperties.error());
              }
              else
              {
                if (catProperties.value().count("dateOfBirth") == 0U)
                {
                  errors.emplace_back("Expected Cat.dateOfBirth in the schema.");
                }
                if (catProperties.value().count("ageYears") != 0U)
                {
                  errors.emplace_back("Did not expect Cat.ageYears in the schema.");
                }
                if (catProperties.value().count("ownerEmail") != 0U)
                {
                  errors.emplace_back(
                      "Did not expect Cat.ownerEmail in the schema.");
                }

                if (catProperties.value().count("name") != 0U)
                {
                  const auto nameSchema =
                      toObject(catProperties.value().at("name"),
                               "Cat.properties.name");
                  if (!nameSchema)
                  {
                    errors.emplace_back(nameSchema.error());
                  }
                  else
                  {
                    if (nameSchema.value().count("description") == 0U)
                    {
                      errors.emplace_back(
                          "Expected reflect-cpp field descriptions in the Cat schema.");
                    }
                    if (nameSchema.value().count("minLength") == 0U)
                    {
                      errors.emplace_back(
                          "Expected reflect-cpp validation metadata in the Cat schema.");
                    }
                  }
                }

                if (catProperties.value().count("dateOfBirth") != 0U)
                {
                  const auto dateOfBirthSchema =
                      toObject(catProperties.value().at("dateOfBirth"),
                               "Cat.properties.dateOfBirth");
                  if (!dateOfBirthSchema)
                  {
                    errors.emplace_back(dateOfBirthSchema.error());
                  }
                  else if (dateOfBirthSchema.value().count("pattern") == 0U)
                  {
                    errors.emplace_back(
                        "Expected dateOfBirth to include string/date validation metadata.");
                  }
                }
              }
            }
          }

          if (schemas.value().count("CatStatus") != 0U)
          {
            const auto statusSchema =
                toObject(schemas.value().at("CatStatus"), "CatStatus schema");
            if (!statusSchema)
            {
              errors.emplace_back(statusSchema.error());
            }
            else if (statusSchema.value().count("enum") == 0U)
            {
              errors.emplace_back("Expected CatStatus enum values in the schema.");
            }
            else
            {
              for (const std::string_view status :
                   std::array{"sassy", "sleepy", "zoomy", "cute"})
              {
                const auto contains =
                    arrayContainsString(statusSchema.value().at("enum"), status);
                if (!contains)
                {
                  errors.emplace_back(contains.error());
                }
                else if (!contains.value())
                {
                  errors.emplace_back("Missing CatStatus enum value: " +
                                      std::string(status));
                }
              }
            }
          }
        }
      }

      const auto paths = toObject(document.value().at("paths"), "paths");
      if (!paths)
      {
        errors.emplace_back(paths.error());
      }
      else
      {
        for (const std::string_view path :
             std::array{"/cats", "/cats/{catId}", "/cats/{catId}/logs"})
        {
          if (paths.value().count(std::string(path)) == 0U)
          {
            errors.emplace_back("Missing path: " + std::string(path));
          }
        }
      }

      for (const std::string_view ref :
           std::array{"#/components/schemas/CreateCatRequest",
                      "#/components/schemas/Cat",
                      "#/components/schemas/CreateCatLogEntryRequest",
                      "#/components/schemas/CatLogEntry",
                      "#/components/schemas/CatLogListResponse"})
      {
        const auto contains = containsSchemaRef(spec.value(), std::string(ref));
        if (!contains)
        {
          errors.emplace_back(contains.error());
        }
        else if (!contains.value())
        {
          errors.emplace_back("Missing schema reference: " + std::string(ref));
        }
      }
    }
  }

  if (!errors.empty())
  {
    for (const auto &error : errors)
    {
      std::cerr << "check failed: " << error << '\n';
    }
    return 1;
  }

  std::cout << "OpenAPI demo validation passed.\n";
  return 0;
}
