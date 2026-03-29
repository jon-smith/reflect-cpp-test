#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
  rfl::Rename<"petId",
              rfl::Description<"Unique pet identifier used in URLs.",
                               std::string>>
      pet_id;
  rfl::Description<"Display name shown in pet listings.", NonEmptyString> name;
  rfl::Description<"Current lifecycle state for the pet.", PetStatus> status;
};

struct Pet
{
  rfl::Rename<"petId",
              rfl::Description<"Unique pet identifier used in URLs.",
                               std::string>>
      pet_id;
  rfl::Description<"Display name presented to API clients.", NonEmptyString>
      name;
  rfl::Description<"Optional short tag used by clients for filtering.",
                   std::optional<std::string>>
      tag;
  rfl::Rename<"ageYears",
              rfl::Description<"Approximate age in whole years.", AgeYears>>
      age_years;
  rfl::Rename<"contactEmail",
              rfl::Description<"Primary adoption contact email.", rfl::Email>>
      contact_email;
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
  rfl::Rename<"ageYears",
              rfl::Description<"Approximate age in whole years.", AgeYears>>
      age_years;
  rfl::Rename<"contactEmail",
              rfl::Description<"Primary adoption contact email.", rfl::Email>>
      contact_email;
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

  Json parse_json_or_throw(const std::string &json)
  {
    auto result = rfl::json::read<Json>(json);
    if (!result)
    {
      throw std::runtime_error("Failed to parse JSON: " + result.error().what());
    }
    return result.value();
  }

  JsonObject to_object_or_throw(const Json &json, std::string_view context)
  {
    auto result = json.to_object();
    if (!result)
    {
      throw std::runtime_error("Expected JSON object for " + std::string(context) +
                               ": " + result.error().what());
    }
    return result.value();
  }

  std::string to_string_or_throw(const Json &json, std::string_view context)
  {
    auto result = json.to_string();
    if (!result)
    {
      throw std::runtime_error("Expected JSON string for " + std::string(context) +
                               ": " + result.error().what());
    }
    return result.value();
  }

  void rewrite_schema_refs(Json *node)
  {
    if (const auto object_result = node->to_object(); object_result)
    {
      auto object = object_result.value();

      if (object.count("$ref") != 0U)
      {
        auto ref = to_string_or_throw(object.at("$ref"), "$ref");
        if (ref.starts_with("#/$defs/"))
        {
          object["$ref"] = "#/components/schemas/" + ref.substr(8);
        }
      }

      for (auto &[_, value] : object)
      {
        rewrite_schema_refs(&value);
      }

      *node = std::move(object);
      return;
    }

    if (const auto array_result = node->to_array(); array_result)
    {
      auto array = array_result.value();
      for (auto &value : array)
      {
        rewrite_schema_refs(&value);
      }
      *node = std::move(array);
    }
  }

  template <class T>
  void register_schema(JsonObject *schemas)
  {
    auto schema_document =
        to_object_or_throw(parse_json_or_throw(rfl::json::to_schema<T>()),
                           "reflect-cpp JSON schema");

    auto definitions = to_object_or_throw(schema_document.at("$defs"), "$defs");

    for (auto &[name, schema] : definitions)
    {
      rewrite_schema_refs(&schema);
      (*schemas)[name] = std::move(schema);
    }
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

  Json build_openapi_spec()
  {
    JsonObject schemas;
    register_schema<Pet>(&schemas);
    register_schema<PetSummary>(&schemas);
    register_schema<CreatePetRequest>(&schemas);
    register_schema<PetListResponse>(&schemas);
    register_schema<ErrorResponse>(&schemas);

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

  bool contains_schema_ref(const Json &node, const std::string &target_ref)
  {
    if (const auto object_result = node.to_object(); object_result)
    {
      const auto object = object_result.value();

      if (object.count("$ref") != 0U &&
          to_string_or_throw(object.at("$ref"), "$ref") == target_ref)
      {
        return true;
      }

      for (const auto &[_, value] : object)
      {
        if (contains_schema_ref(value, target_ref))
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
        if (contains_schema_ref(value, target_ref))
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

    try
    {
      const auto spec = build_openapi_spec();
      const auto document = to_object_or_throw(spec, "OpenAPI document");

      if (to_string_or_throw(document.at("openapi"), "openapi") != "3.1.0")
      {
        errors.emplace_back("Expected OpenAPI version 3.1.0.");
      }

      const auto components =
          to_object_or_throw(document.at("components"), "components");
      const auto schemas = to_object_or_throw(components.at("schemas"), "schemas");

      for (const auto *name :
           {"Pet", "PetSummary", "CreatePetRequest", "PetListResponse",
            "ErrorResponse"})
      {
        if (schemas.count(name) == 0U)
        {
          errors.emplace_back("Missing component schema: " + std::string(name));
        }
      }

      const auto paths = to_object_or_throw(document.at("paths"), "paths");
      if (paths.count("/pets") == 0U)
      {
        errors.emplace_back("Missing /pets path.");
      }
      if (paths.count("/pets/{petId}") == 0U)
      {
        errors.emplace_back("Missing /pets/{petId} path.");
      }

      for (const auto *ref :
           {"#/components/schemas/CreatePetRequest",
            "#/components/schemas/Pet",
            "#/components/schemas/ErrorResponse",
            "#/components/schemas/PetListResponse"})
      {
        if (!contains_schema_ref(spec, ref))
        {
          errors.emplace_back("Missing schema reference: " + std::string(ref));
        }
      }

      const auto pet_schema = to_object_or_throw(schemas.at("Pet"), "Pet schema");
      const auto pet_properties =
          to_object_or_throw(pet_schema.at("properties"), "Pet.properties");
      const auto contact_email =
          to_object_or_throw(pet_properties.at("contactEmail"),
                             "Pet.properties.contactEmail");

      if (contact_email.count("description") == 0U)
      {
        errors.emplace_back(
            "Expected reflect-cpp field descriptions in the Pet schema.");
      }
      if (contact_email.count("pattern") == 0U)
      {
        errors.emplace_back(
            "Expected reflect-cpp email validation metadata in the Pet schema.");
      }
    }
    catch (const std::exception &exception)
    {
      errors.emplace_back(exception.what());
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

  rfl::json::write(build_openapi_spec(), std::cout, rfl::json::pretty);
  std::cout << '\n';
  return 0;
}
