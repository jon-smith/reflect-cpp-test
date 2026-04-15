#include "demo/openapi_demo.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "demo/cat_api_routes.hpp"
#include "demo/cat_api_types.hpp"

namespace
{

std::vector<clam::OpenApiPathItem> buildOpenApiPaths(const std::vector<clam::ApiRoute> &apiRoutes)
{
  std::vector<clam::OpenApiPathItem> pathItems;
  std::unordered_map<std::string, std::size_t> pathIndexes;

  for (const auto &route : apiRoutes)
  {
    const auto it = pathIndexes.find(route.openApiPath);
    if (it == pathIndexes.end())
    {
      pathIndexes.emplace(route.openApiPath, pathItems.size());
      pathItems.push_back(clam::OpenApiPathItem{
          .path = route.openApiPath,
          .operations = {clam::OpenApiPathOperation{
              .method = std::string(clam::toOpenApiMethod(route.method)),
              .operation = route.operation,
          }},
      });
      continue;
    }

    pathItems[it->second].operations.push_back(clam::OpenApiPathOperation{
        .method = std::string(clam::toOpenApiMethod(route.method)),
        .operation = route.operation,
    });
  }

  return pathItems;
}

std::vector<clam::OpenApiSchemaRegistrar> buildOpenApiSchemaRegistrations(const std::vector<clam::ApiRoute> &apiRoutes)
{
  std::vector<clam::OpenApiSchemaRegistrar> schemaRegistrations;

  for (const auto &route : apiRoutes)
  {
    schemaRegistrations.insert(schemaRegistrations.end(), route.schemaRegistrations.begin(),
                               route.schemaRegistrations.end());
  }

  return schemaRegistrations;
}

clam::OpenApiSpecConfig makeCatLogOpenApiConfig()
{
  const auto apiRoutes = makeCatApiRoutes();

  return clam::OpenApiSpecConfig{
      .info =
          {
              .title = "reflect-cpp CatLog API",
              .version = "1.0.0",
              .description = "OpenAPI 3.1 document assembled from reflect-cpp-generated "
                             "JSON Schema components for a CatLog API.",
          },
      .servers = {clam::OpenApiServer{
          .url = "http://localhost:8080",
          .description = "Local development server",
      }},
      .paths = buildOpenApiPaths(apiRoutes),
      .schemaRegistrations = buildOpenApiSchemaRegistrations(apiRoutes),
  };
}
}

std::expected<rfl::Generic, std::string> buildCatLogOpenApiSpec()
{
  return clam::buildOpenApiSpec(makeCatLogOpenApiConfig());
}

std::vector<std::string> validateOpenApiDemoSpec(const rfl::Generic &spec)
{
  std::vector<std::string> errors;
  const auto document = clam::toObject(spec, "OpenAPI document");
  if (!document)
  {
    errors.emplace_back(document.error());
  }
  else
  {
    const auto openapi = clam::toString(document.value().at("openapi"), "openapi");
    if (!openapi)
    {
      errors.emplace_back(openapi.error());
    }
    else if (openapi.value() != "3.1.0")
    {
      errors.emplace_back("Expected OpenAPI version 3.1.0.");
    }

    const auto components = clam::toObject(document.value().at("components"), "components");
    if (!components)
    {
      errors.emplace_back(components.error());
    }
    else
    {
      const auto schemas = clam::toObject(components.value().at("schemas"), "schemas");
      if (!schemas)
      {
        errors.emplace_back(schemas.error());
      }
      else
      {
        for (const std::string_view name :
             std::array{"Cat", "CatSummary", "CreateCatRequest", "CatLogEntry", "CreateCatLogEntryRequest",
                        "CatListResponse", "CatLogListResponse", "ErrorResponse"})
        {
          if (schemas.value().count(std::string(name)) == 0U)
          {
            errors.emplace_back("Missing component schema: " + std::string(name));
          }
        }

        if (schemas.value().count("Cat") != 0U)
        {
          const auto catSchema = clam::toObject(schemas.value().at("Cat"), "Cat schema");
          if (!catSchema)
          {
            errors.emplace_back(catSchema.error());
          }
          else
          {
            const auto catProperties = clam::toObject(catSchema.value().at("properties"), "Cat.properties");
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
                errors.emplace_back("Did not expect Cat.ownerEmail in the schema.");
              }

              if (catProperties.value().count("name") != 0U)
              {
                const auto nameSchema = clam::toObject(catProperties.value().at("name"), "Cat.properties.name");
                if (!nameSchema)
                {
                  errors.emplace_back(nameSchema.error());
                }
                else
                {
                  if (nameSchema.value().count("description") == 0U)
                  {
                    errors.emplace_back("Expected reflect-cpp field descriptions in the Cat schema.");
                  }
                  if (nameSchema.value().count("minLength") == 0U)
                  {
                    errors.emplace_back("Expected reflect-cpp validation metadata in the Cat schema.");
                  }
                }
              }

              if (catProperties.value().count("dateOfBirth") != 0U)
              {
                const auto dateOfBirthSchema =
                    clam::toObject(catProperties.value().at("dateOfBirth"), "Cat.properties.dateOfBirth");
                if (!dateOfBirthSchema)
                {
                  errors.emplace_back(dateOfBirthSchema.error());
                }
                else if (dateOfBirthSchema.value().count("pattern") == 0U)
                {
                  errors.emplace_back("Expected dateOfBirth to include string/date validation metadata.");
                }
              }
            }
          }
        }

        if (schemas.value().count("CatStatus") != 0U)
        {
          const auto statusSchema = clam::toObject(schemas.value().at("CatStatus"), "CatStatus schema");
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
            for (const std::string_view status : std::array{"sassy", "sleepy", "zoomy", "cute"})
            {
              const auto contains = clam::arrayContainsString(statusSchema.value().at("enum"), status);
              if (!contains)
              {
                errors.emplace_back(contains.error());
              }
              else if (!contains.value())
              {
                errors.emplace_back("Missing CatStatus enum value: " + std::string(status));
              }
            }
          }
        }
      }
    }

    const auto paths = clam::toObject(document.value().at("paths"), "paths");
    if (!paths)
    {
      errors.emplace_back(paths.error());
    }
    else
    {
      for (const std::string_view path : std::array{"/cats", "/cats/{catId}", "/cats/{catId}/logs"})
      {
        if (paths.value().count(std::string(path)) == 0U)
        {
          errors.emplace_back("Missing path: " + std::string(path));
        }
      }
    }

    for (const std::string_view ref :
         std::array{"#/components/schemas/CreateCatRequest", "#/components/schemas/Cat",
                    "#/components/schemas/CreateCatLogEntryRequest", "#/components/schemas/CatLogEntry",
                    "#/components/schemas/CatLogListResponse"})
    {
      const auto contains = clam::containsSchemaRef(spec, std::string(ref));
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

  return errors;
}
