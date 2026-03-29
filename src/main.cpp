#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

#include <rfl.hpp>
#include <rfl/json.hpp>

using AgeYears = rfl::Validator<int, rfl::Minimum<0>, rfl::Maximum<30>>;
using NonEmptyString = rfl::Validator<std::string, rfl::Size<rfl::Minimum<1>>>;

enum class PetStatus
{
  available,
  adopted,
  fostered
};

struct PetSummary
{
  rfl::Description<"Unique pet identifier used in URLs.", std::string> petId;
  rfl::Description<"Display name shown in pet listings.", NonEmptyString> name;
  rfl::Description<"Current lifecycle state for the pet.", PetStatus> status;
};

struct Pet
{
  rfl::Description<"Unique pet identifier used in URLs.", std::string> petId;
  rfl::Description<"Display name presented to API clients.", NonEmptyString>
      name;
  rfl::Description<"Optional short tag used by clients for filtering.",
                   std::optional<std::string>>
      tag;
  rfl::Description<"Approximate age in whole years.", AgeYears> ageYears;
  rfl::Description<"Primary adoption contact email.", rfl::Email> contactEmail;
  rfl::Description<"Vaccinations already recorded for the pet.",
                   std::vector<NonEmptyString>>
      vaccinations;
  rfl::Description<"Current lifecycle state for the pet.", PetStatus> status;
};

struct CreatePetRequest
{
  rfl::Description<"Display name presented to API clients.", NonEmptyString>
      name;
  rfl::Description<"Optional short tag used by clients for filtering.",
                   std::optional<std::string>>
      tag;
  rfl::Description<"Approximate age in whole years.", AgeYears> ageYears;
  rfl::Description<"Primary adoption contact email.", rfl::Email> contactEmail;
  rfl::Description<"Vaccinations already recorded for the pet.",
                   std::vector<NonEmptyString>>
      vaccinations;
  rfl::Description<"Status assigned when the pet is created.", PetStatus>
      status;
};

struct PetListResponse
{
  rfl::Description<"Pets available from the collection endpoint.",
                   std::vector<PetSummary>>
      pets;
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

  Json make_object(std::initializer_list<std::pair<std::string, Json>> fields)
  {
    JsonObject object;
    for (const auto &[key, value] : fields)
    {
      object.insert(key, value);
    }
    return object;
  }

  Json make_array(std::initializer_list<Json> values)
  {
    return JsonArray(values);
  }

  using ExpectedJson = std::expected<Json, std::string>;
  using ExpectedJsonObject = std::expected<JsonObject, std::string>;
  using ExpectedString = std::expected<std::string, std::string>;
  using ExpectedVoid = std::expected<void, std::string>;
  using ExpectedBool = std::expected<bool, std::string>;

  ExpectedJson parse_json(const std::string &json)
  {
    auto result = rfl::json::read<Json>(json);
    if (!result)
    {
      return std::unexpected("Failed to parse JSON: " +
                             result.error().what());
    }
    return result.value();
  }

  ExpectedJsonObject to_object(const Json &json, std::string_view context)
  {
    auto result = json.to_object();
    if (!result)
    {
      return std::unexpected("Expected JSON object for " +
                             std::string(context) + ": " +
                             result.error().what());
    }
    return result.value();
  }

  ExpectedString to_string(const Json &json, std::string_view context)
  {
    auto result = json.to_string();
    if (!result)
    {
      return std::unexpected("Expected JSON string for " +
                             std::string(context) + ": " +
                             result.error().what());
    }
    return result.value();
  }

  ExpectedVoid rewrite_schema_refs(Json *node)
  {
    if (const auto object_result = node->to_object(); object_result)
    {
      auto object = object_result.value();

      if (object.count("$ref") != 0U)
      {
        auto ref = to_string(object.at("$ref"), "$ref");
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
        auto rewritten = rewrite_schema_refs(&value);
        if (!rewritten)
        {
          return rewritten;
        }
      }

      *node = std::move(object);
      return {};
    }

    if (const auto array_result = node->to_array(); array_result)
    {
      auto array = array_result.value();
      for (auto &value : array)
      {
        auto rewritten = rewrite_schema_refs(&value);
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
  ExpectedVoid register_schema(JsonObject *schemas)
  {
    auto schema_json = parse_json(rfl::json::to_schema<T>());
    if (!schema_json)
    {
      return std::unexpected(schema_json.error());
    }

    auto schema_document = to_object(*schema_json, "reflect-cpp JSON schema");
    if (!schema_document)
    {
      return std::unexpected(schema_document.error());
    }

    auto definitions = to_object(schema_document->at("$defs"), "$defs");
    if (!definitions)
    {
      return std::unexpected(definitions.error());
    }

    for (auto &[name, schema] : *definitions)
    {
      auto rewritten = rewrite_schema_refs(&schema);
      if (!rewritten)
      {
        return rewritten;
      }
      (*schemas)[name] = std::move(schema);
    }

    return {};
  }

  Json schema_ref(const std::string &name)
  {
    return make_object(
        {{"$ref", "#/components/schemas/" + name}});
  }

  Json json_response(const std::string &description,
                     const std::string &schema_name)
  {
    return make_object(
        {{"description", description},
         {"content",
          make_object({{"application/json",
                        make_object({{"schema", schema_ref(schema_name)}})}})}});
  }

  Json error_response(const std::string &description)
  {
    return json_response(description, "ErrorResponse");
  }

  ExpectedJson build_openapi_spec()
  {
    JsonObject schemas;
    for (const auto result : {register_schema<Pet>(&schemas),
                              register_schema<PetSummary>(&schemas),
                              register_schema<CreatePetRequest>(&schemas),
                              register_schema<PetListResponse>(&schemas),
                              register_schema<ErrorResponse>(&schemas)})
    {
      if (!result)
      {
        return std::unexpected(result.error());
      }
    }

    return make_object({
        {"openapi", "3.1.0"},
        {"info",
         make_object({
             {"title", "reflect-cpp Pets API"},
             {"version", "1.0.0"},
             {"description",
              "OpenAPI 3.1 document assembled from reflect-cpp-generated JSON "
              "Schema components."},
         })},
        {"servers",
         make_array({make_object({
             {"url", "http://localhost:8080"},
             {"description", "Local development server"},
         })})},
        {"paths",
         make_object({
             {"/pets",
              make_object({
                  {"get",
                   make_object({
                       {"summary", "List pets"},
                       {"operationId", "listPets"},
                       {"responses",
                        make_object({
                            {"200",
                             json_response("A pageless list of pets.",
                                           "PetListResponse")},
                            {"500", error_response("Unexpected server error.")},
                        })},
                   })},
                  {"post",
                   make_object({
                       {"summary", "Create a pet"},
                       {"operationId", "createPet"},
                       {"requestBody",
                        make_object({
                            {"required", true},
                            {"content",
                             make_object({{"application/json",
                                           make_object(
                                               {{"schema",
                                                 schema_ref("CreatePetRequest")}})}})},
                        })},
                       {"responses",
                        make_object({
                            {"201", json_response("The created pet.", "Pet")},
                            {"400", error_response("The request body was invalid.")},
                        })},
                   })},
              })},
             {"/pets/{petId}",
              make_object({
                  {"get",
                   make_object({
                       {"summary", "Get a pet by id"},
                       {"operationId", "getPetById"},
                       {"parameters",
                        make_array({make_object({
                            {"name", "petId"},
                            {"in", "path"},
                            {"required", true},
                            {"description",
                             "Unique identifier for a previously created pet."},
                            {"schema", make_object({{"type", "string"}})},
                        })})},
                       {"responses",
                        make_object({
                            {"200", json_response("The requested pet.", "Pet")},
                            {"404", error_response("The pet was not found.")},
                        })},
                   })},
              })},
         })},
        {"components", make_object({{"schemas", std::move(schemas)}})},
    });
  }

  ExpectedBool contains_schema_ref(const Json &node,
                                   const std::string &target_ref)
  {
    if (const auto object_result = node.to_object(); object_result)
    {
      const auto object = object_result.value();

      if (object.count("$ref") != 0U)
      {
        auto ref = to_string(object.at("$ref"), "$ref");
        if (!ref)
        {
          return std::unexpected(ref.error());
        }
        if (*ref == target_ref)
        {
          return true;
        }
      }

      for (const auto &[_, value] : object)
      {
        auto contains = contains_schema_ref(value, target_ref);
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

    if (const auto array_result = node.to_array(); array_result)
    {
      const auto array = array_result.value();
      for (const auto &value : array)
      {
        auto contains = contains_schema_ref(value, target_ref);
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

  int run_checks()
  {
    std::vector<std::string> errors;
    const auto spec = build_openapi_spec();
    if (!spec)
    {
      errors.emplace_back(spec.error());
    }
    else
    {
      const auto document = to_object(*spec, "OpenAPI document");
      if (!document)
      {
        errors.emplace_back(document.error());
      }
      else
      {
        const auto openapi = to_string(document->at("openapi"), "openapi");
        if (!openapi)
        {
          errors.emplace_back(openapi.error());
        }
        else if (*openapi != "3.1.0")
        {
          errors.emplace_back("Expected OpenAPI version 3.1.0.");
        }

        const auto components = to_object(document->at("components"), "components");
        if (!components)
        {
          errors.emplace_back(components.error());
        }
        else
        {
          const auto schemas = to_object(components->at("schemas"), "schemas");
          if (!schemas)
          {
            errors.emplace_back(schemas.error());
          }
          else
          {
            for (const auto *name :
                 {"Pet", "PetSummary", "CreatePetRequest", "PetListResponse",
                  "ErrorResponse"})
            {
              if (schemas->count(name) == 0U)
              {
                errors.emplace_back("Missing component schema: " + std::string(name));
              }
            }

            const auto petSchema = to_object(schemas->at("Pet"), "Pet schema");
            if (!petSchema)
            {
              errors.emplace_back(petSchema.error());
            }
            else
            {
              const auto petProperties =
                  to_object(petSchema->at("properties"), "Pet.properties");
              if (!petProperties)
              {
                errors.emplace_back(petProperties.error());
              }
              else
              {
                const auto contactEmail =
                    to_object(petProperties->at("contactEmail"),
                              "Pet.properties.contactEmail");
                if (!contactEmail)
                {
                  errors.emplace_back(contactEmail.error());
                }
                else
                {
                  if (contactEmail->count("description") == 0U)
                  {
                    errors.emplace_back(
                        "Expected reflect-cpp field descriptions in the Pet schema.");
                  }
                  if (contactEmail->count("pattern") == 0U)
                  {
                    errors.emplace_back(
                        "Expected reflect-cpp email validation metadata in the Pet schema.");
                  }
                }
              }
            }
          }
        }

        const auto paths = to_object(document->at("paths"), "paths");
        if (!paths)
        {
          errors.emplace_back(paths.error());
        }
        else
        {
          if (paths->count("/pets") == 0U)
          {
            errors.emplace_back("Missing /pets path.");
          }
          if (paths->count("/pets/{petId}") == 0U)
          {
            errors.emplace_back("Missing /pets/{petId} path.");
          }
        }

        for (const auto *ref :
             {"#/components/schemas/CreatePetRequest",
              "#/components/schemas/Pet",
              "#/components/schemas/ErrorResponse",
              "#/components/schemas/PetListResponse"})
        {
          auto contains = contains_schema_ref(*spec, ref);
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

struct Cat
{
  std::string breed;
  std::string name;
  double age = {};
  std::string meow = "meow";
};

const auto senorDonGato =
    Cat{.breed = "Siamese",
        .name = "Senor Don Gato",
        .age = 5.0};

int main(int argc, char **argv)
{
  std::cout << std::format("👽uoıʇɔǝlɟǝɹ🪬") << std::endl;
  std::cout << rfl::json::write(senorDonGato) << std::endl;

  if (argc > 1 && std::string_view(argv[1]) == "--check")
  {
    std::cout << "Running checks..." << std::endl;
    return run_checks();
  }

  std::cout << "Building open API spec..." << std::endl;

  const auto spec = build_openapi_spec();
  if (!spec)
  {
    std::cerr << "failed to build OpenAPI spec: " << spec.error() << '\n';
    return 1;
  }

  rfl::json::write(*spec, std::cout, rfl::json::pretty);
  std::cout << '\n';
  return 0;
}
