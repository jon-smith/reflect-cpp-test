#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <rfl.hpp>
#include <rfl/json.hpp>

using DateString = rfl::Pattern<R"(^\d{4}-\d{2}-\d{2}$)", "Date">;
using DateTimeString =
    rfl::Pattern<R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)", "DateTime">;
using NonEmptyString = rfl::Validator<std::string, rfl::Size<rfl::Minimum<1>>>;

enum class CatStatus
{
  sassy,
  sleepy,
  zoomy,
  cute
};

struct CatSummary
{
  rfl::Description<"Unique cat identifier used in URLs.", std::string> catId;
  rfl::Description<"Display name shown in cat listings.", NonEmptyString> name;
  rfl::Description<"Most recently logged status for this cat.", CatStatus>
      latestStatus;
};

struct Cat
{
  rfl::Description<"Unique cat identifier used in URLs.", std::string> catId;
  rfl::Description<"Display name presented to API clients.", NonEmptyString>
      name;
  rfl::Description<"Optional breed information for the cat.",
                   std::optional<std::string>>
      breed;
  rfl::Description<"Date of birth in YYYY-MM-DD format.", DateString>
      dateOfBirth;
  rfl::Description<"Optional notes about the cat.",
                   std::optional<NonEmptyString>>
      notes;
};

struct CreateCatRequest
{
  rfl::Description<"Display name presented to API clients.", NonEmptyString>
      name;
  rfl::Description<"Optional breed information for the cat.",
                   std::optional<std::string>>
      breed;
  rfl::Description<"Date of birth in YYYY-MM-DD format.", DateString>
      dateOfBirth;
  rfl::Description<"Optional notes about the cat.",
                   std::optional<NonEmptyString>>
      notes;
};

struct CatLogEntry
{
  rfl::Description<"Unique log identifier for this status entry.", std::string>
      logId;
  rfl::Description<"Identifier of the cat this status belongs to.", std::string>
      catId;
  rfl::Description<"Cat mood/status captured by the entry.", CatStatus> status;
  rfl::Description<"Timestamp recorded in UTC as YYYY-MM-DDTHH:MM:SSZ.",
                   DateTimeString>
      loggedAt;
  rfl::Description<"Optional note recorded alongside the status.",
                   std::optional<NonEmptyString>>
      note;
};

struct CreateCatLogEntryRequest
{
  rfl::Description<"Cat mood/status captured by the entry.", CatStatus> status;
  rfl::Description<"Timestamp recorded in UTC as YYYY-MM-DDTHH:MM:SSZ.",
                   DateTimeString>
      loggedAt;
  rfl::Description<"Optional note recorded alongside the status.",
                   std::optional<NonEmptyString>>
      note;
};

struct CatListResponse
{
  rfl::Description<"Cats available from the collection endpoint.",
                   std::vector<CatSummary>>
      cats;
};

struct CatLogListResponse
{
  rfl::Description<"Status log entries recorded for a cat.",
                   std::vector<CatLogEntry>>
      logs;
};

struct ErrorResponse
{
  rfl::Description<"Stable machine-readable error code.", std::string> code;
  rfl::Description<"Human-readable summary of the failure.", std::string>
      message;
  rfl::Description<"Additional debugging context when available.",
                   std::optional<std::string>>
      detail;
};

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

  Json makeObject(std::initializer_list<std::pair<std::string, Json>> fields)
  {
    JsonObject object;
    for (const auto &[key, value] : fields)
    {
      object.insert(key, value);
    }
    return object;
  }

  Json makeArray(std::initializer_list<Json> values)
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

  ExpectedJsonObject toObject(const Json &json, std::string_view context)
  {
    const auto result = json.to_object();
    if (!result)
    {
      return std::unexpected("Expected JSON object for " + std::string(context) +
                             ": " + result.error().what());
    }
    return result.value();
  }

  ExpectedString toString(const Json &json, std::string_view context)
  {
    const auto result = json.to_string();
    if (!result)
    {
      return std::unexpected("Expected JSON string for " + std::string(context) +
                             ": " + result.error().what());
    }
    return result.value();
  }

  ExpectedVoid rewriteSchemaRefs(Json *node)
  {
    if (const auto objectResult = node->to_object(); objectResult)
    {
      auto object = objectResult.value();

      if (object.count("$ref") != 0U)
      {
        const auto ref = toString(object.at("$ref"), "$ref");
        if (!ref)
        {
          return std::unexpected(ref.error());
        }
        if (ref->starts_with("#/$defs/"))
        {
          object["$ref"] = "#/components/schemas/" + ref->substr(8);
        }
      }

      for (auto &[_, value] : object)
      {
        const auto rewritten = rewriteSchemaRefs(&value);
        if (!rewritten)
        {
          return rewritten;
        }
      }

      *node = std::move(object);
      return {};
    }

    if (const auto arrayResult = node->to_array(); arrayResult)
    {
      auto array = arrayResult.value();
      for (auto &value : array)
      {
        const auto rewritten = rewriteSchemaRefs(&value);
        if (!rewritten)
        {
          return rewritten;
        }
      }
      *node = std::move(array);
    }

    return {};
  }

  template <class T>
  ExpectedVoid registerSchema(JsonObject *schemas)
  {
    const auto schemaJson = parseJson(rfl::json::to_schema<T>());
    if (!schemaJson)
    {
      return std::unexpected(schemaJson.error());
    }

    const auto schemaDocument = toObject(*schemaJson, "reflect-cpp JSON schema");
    if (!schemaDocument)
    {
      return std::unexpected(schemaDocument.error());
    }

    auto definitions = toObject(schemaDocument->at("$defs"), "$defs");
    if (!definitions)
    {
      return std::unexpected(definitions.error());
    }

    for (auto &[name, schema] : *definitions)
    {
      const auto rewritten = rewriteSchemaRefs(&schema);
      if (!rewritten)
      {
        return rewritten;
      }
      (*schemas)[name] = std::move(schema);
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

  ExpectedJson buildOpenApiSpec()
  {
    JsonObject schemas;
    for (const auto result : {registerSchema<Cat>(&schemas),
                              registerSchema<CatSummary>(&schemas),
                              registerSchema<CreateCatRequest>(&schemas),
                              registerSchema<CatLogEntry>(&schemas),
                              registerSchema<CreateCatLogEntryRequest>(&schemas),
                              registerSchema<CatListResponse>(&schemas),
                              registerSchema<CatLogListResponse>(&schemas),
                              registerSchema<ErrorResponse>(&schemas)})
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
        if (*ref == targetRef)
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
        if (*contains)
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
        if (*contains)
        {
          return true;
        }
      }
    }

    return false;
  }

  ExpectedBool arrayContainsString(const Json &json, std::string_view expectedValue)
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
      if (*stringValue == expectedValue)
      {
        return true;
      }
    }

    return false;
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
      const auto document = toObject(*spec, "OpenAPI document");
      if (!document)
      {
        errors.emplace_back(document.error());
      }
      else
      {
        const auto openapi = toString(document->at("openapi"), "openapi");
        if (!openapi)
        {
          errors.emplace_back(openapi.error());
        }
        else if (*openapi != "3.1.0")
        {
          errors.emplace_back("Expected OpenAPI version 3.1.0.");
        }

        const auto components = toObject(document->at("components"), "components");
        if (!components)
        {
          errors.emplace_back(components.error());
        }
        else
        {
          const auto schemas = toObject(components->at("schemas"), "schemas");
          if (!schemas)
          {
            errors.emplace_back(schemas.error());
          }
          else
          {
            for (const auto *name : {"Cat",
                                     "CatSummary",
                                     "CreateCatRequest",
                                     "CatLogEntry",
                                     "CreateCatLogEntryRequest",
                                     "CatListResponse",
                                     "CatLogListResponse",
                                     "ErrorResponse"})
            {
              if (schemas->count(name) == 0U)
              {
                errors.emplace_back("Missing component schema: " + std::string(name));
              }
            }

            if (schemas->count("Cat") != 0U)
            {
              const auto catSchema = toObject(schemas->at("Cat"), "Cat schema");
              if (!catSchema)
              {
                errors.emplace_back(catSchema.error());
              }
              else
              {
                const auto catProperties =
                    toObject(catSchema->at("properties"), "Cat.properties");
                if (!catProperties)
                {
                  errors.emplace_back(catProperties.error());
                }
                else
                {
                  if (catProperties->count("dateOfBirth") == 0U)
                  {
                    errors.emplace_back("Expected Cat.dateOfBirth in the schema.");
                  }
                  if (catProperties->count("ageYears") != 0U)
                  {
                    errors.emplace_back("Did not expect Cat.ageYears in the schema.");
                  }
                  if (catProperties->count("ownerEmail") != 0U)
                  {
                    errors.emplace_back("Did not expect Cat.ownerEmail in the schema.");
                  }

                  if (catProperties->count("name") != 0U)
                  {
                    const auto nameSchema =
                        toObject(catProperties->at("name"), "Cat.properties.name");
                    if (!nameSchema)
                    {
                      errors.emplace_back(nameSchema.error());
                    }
                    else
                    {
                      if (nameSchema->count("description") == 0U)
                      {
                        errors.emplace_back(
                            "Expected reflect-cpp field descriptions in the Cat schema.");
                      }
                      if (nameSchema->count("minLength") == 0U)
                      {
                        errors.emplace_back(
                            "Expected reflect-cpp validation metadata in the Cat schema.");
                      }
                    }
                  }

                  if (catProperties->count("dateOfBirth") != 0U)
                  {
                    const auto dateOfBirthSchema = toObject(
                        catProperties->at("dateOfBirth"),
                        "Cat.properties.dateOfBirth");
                    if (!dateOfBirthSchema)
                    {
                      errors.emplace_back(dateOfBirthSchema.error());
                    }
                    else if (dateOfBirthSchema->count("pattern") == 0U)
                    {
                      errors.emplace_back(
                          "Expected dateOfBirth to include string/date validation metadata.");
                    }
                  }
                }
              }
            }

            if (schemas->count("CatStatus") != 0U)
            {
              const auto statusSchema =
                  toObject(schemas->at("CatStatus"), "CatStatus schema");
              if (!statusSchema)
              {
                errors.emplace_back(statusSchema.error());
              }
              else if (statusSchema->count("enum") == 0U)
              {
                errors.emplace_back("Expected CatStatus enum values in the schema.");
              }
              else
              {
                for (const auto *status : {"sassy", "sleepy", "zoomy", "cute"})
                {
                  const auto contains =
                      arrayContainsString(statusSchema->at("enum"), status);
                  if (!contains)
                  {
                    errors.emplace_back(contains.error());
                  }
                  else if (!*contains)
                  {
                    errors.emplace_back("Missing CatStatus enum value: " +
                                        std::string(status));
                  }
                }
              }
            }
          }
        }

        const auto paths = toObject(document->at("paths"), "paths");
        if (!paths)
        {
          errors.emplace_back(paths.error());
        }
        else
        {
          for (const auto *path :
               {"/cats", "/cats/{catId}", "/cats/{catId}/logs"})
          {
            if (paths->count(path) == 0U)
            {
              errors.emplace_back("Missing path: " + std::string(path));
            }
          }
        }

        for (const auto *ref : {"#/components/schemas/CreateCatRequest",
                                "#/components/schemas/Cat",
                                "#/components/schemas/CreateCatLogEntryRequest",
                                "#/components/schemas/CatLogEntry",
                                "#/components/schemas/CatLogListResponse"})
        {
          const auto contains = containsSchemaRef(*spec, ref);
          if (!contains)
          {
            errors.emplace_back(contains.error());
          }
          else if (!*contains)
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

} // namespace

const auto senorDonGato =
    Cat{
        .name = "Senor Don Gato",
        .breed = "Siamese",
        .dateOfBirth = "1999-08-10",
    };

int main(int argc, char **argv)
{
  std::cout << std::format("👽uoıʇɔǝlɟǝɹ🪬") << std::endl;
  std::cout << rfl::json::write(senorDonGato) << std::endl;
  if (argc > 1 && std::string_view(argv[1]) == "--check")
  {
    std::cout << "Running checks..." << std::endl;
    return runChecks();
  }

  std::cout << "Building open API spec..." << std::endl;

  const auto spec = buildOpenApiSpec();
  if (!spec)
  {
    std::cerr << "failed to build OpenAPI spec: " << spec.error() << '\n';
    return 1;
  }

  rfl::json::write(*spec, std::cout, rfl::json::pretty);
  std::cout << '\n';
  return 0;
}
