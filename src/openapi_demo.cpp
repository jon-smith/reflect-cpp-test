#include "openapi_demo.hpp"

#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "cat_api_types.hpp"

namespace
{

OpenApiParameter catIdParameter(const std::string &description)
{
  return OpenApiParameter{
      .name = "catId",
      .in = "path",
      .required = true,
      .description = description,
      .schema = stringSchema(),
  };
}

OpenApiSpecConfig makeCatLogOpenApiConfig()
{
  return OpenApiSpecConfig{
      .info =
          {
              .title = "reflect-cpp CatLog API",
              .version = "1.0.0",
              .description = "OpenAPI 3.1 document assembled from reflect-cpp-generated "
                             "JSON Schema components for a CatLog API.",
          },
      .servers = {{
          .url = "http://localhost:8080",
          .description = "Local development server",
      }},
      .paths =
          {
              OpenApiPathItem{
                  .path = "/cats",
                  .operations =
                      {
                          OpenApiPathOperation{
                              .method = "get",
                              .operation =
                                  {
                                      .summary = "List cats",
                                      .operationId = "listCats",
                                      .responses =
                                          {
                                              OpenApiResponse{
                                                  .statusCode = "200",
                                                  .description = "A list of cats.",
                                                  .schemaName = "CatListResponse",
                                              },
                                              errorResponse("500", "Unexpected server error."),
                                          },
                                  },
                          },
                          OpenApiPathOperation{
                              .method = "post",
                              .operation =
                                  {
                                      .summary = "Create a cat",
                                      .operationId = "createCat",
                                      .requestBody =
                                          OpenApiRequestBody{
                                              .required = true,
                                              .schemaName = "CreateCatRequest",
                                          },
                                      .responses =
                                          {
                                              OpenApiResponse{
                                                  .statusCode = "201",
                                                  .description = "The created cat.",
                                                  .schemaName = "Cat",
                                              },
                                              errorResponse("400", "The request body was invalid."),
                                          },
                                  },
                          },
                      },
              },
              OpenApiPathItem{
                  .path = "/cats/{catId}",
                  .operations =
                      {
                          OpenApiPathOperation{
                              .method = "get",
                              .operation =
                                  {
                                      .summary = "Get a cat by id",
                                      .operationId = "getCatById",
                                      .parameters = {catIdParameter("Unique identifier for a "
                                                                    "previously created cat.")},
                                      .responses =
                                          {
                                              OpenApiResponse{
                                                  .statusCode = "200",
                                                  .description = "The requested cat.",
                                                  .schemaName = "Cat",
                                              },
                                              errorResponse("404", "The cat was not found."),
                                          },
                                  },
                          },
                      },
              },
              OpenApiPathItem{
                  .path = "/cats/{catId}/logs",
                  .operations =
                      {
                          OpenApiPathOperation{
                              .method = "get",
                              .operation =
                                  {
                                      .summary = "List cat status logs",
                                      .operationId = "listCatLogs",
                                      .parameters = {catIdParameter("Unique identifier for the cat "
                                                                    "whose logs are being requested.")},
                                      .responses =
                                          {
                                              OpenApiResponse{
                                                  .statusCode = "200",
                                                  .description = "Status log entries for "
                                                                 "the cat.",
                                                  .schemaName = "CatLogListResponse",
                                              },
                                              errorResponse("404", "The cat was not found."),
                                          },
                                  },
                          },
                          OpenApiPathOperation{
                              .method = "post",
                              .operation =
                                  {
                                      .summary = "Create a cat status log entry",
                                      .operationId = "createCatLogEntry",
                                      .parameters = {catIdParameter("Unique identifier for the cat "
                                                                    "receiving the new log entry.")},
                                      .requestBody =
                                          OpenApiRequestBody{
                                              .required = true,
                                              .schemaName = "CreateCatLogEntryRequest",
                                          },
                                      .responses =
                                          {
                                              OpenApiResponse{
                                                  .statusCode = "201",
                                                  .description = "The created log entry.",
                                                  .schemaName = "CatLogEntry",
                                              },
                                              errorResponse("400", "The request body was invalid."),
                                              errorResponse("404", "The cat was not found."),
                                          },
                                  },
                          },
                      },
              },
          },
      .schemaRegistrations =
          {
              makeOpenApiSchemaRegistration<Cat>(),
              makeOpenApiSchemaRegistration<CatSummary>(),
              makeOpenApiSchemaRegistration<CreateCatRequest>(),
              makeOpenApiSchemaRegistration<CatLogEntry>(),
              makeOpenApiSchemaRegistration<CreateCatLogEntryRequest>(),
              makeOpenApiSchemaRegistration<CatListResponse>(),
              makeOpenApiSchemaRegistration<CatLogListResponse>(),
              makeOpenApiSchemaRegistration<ErrorResponse>(),
          },
  };
}

}

std::expected<rfl::Generic, std::string> buildOpenApiSpec()
{
  return buildOpenApiSpec(makeCatLogOpenApiConfig());
}

std::vector<std::string> validateOpenApiDemoSpec(const rfl::Generic &spec)
{
  std::vector<std::string> errors;
  const auto document = toObject(spec, "OpenAPI document");
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

    const auto components = toObject(document.value().at("components"), "components");
    if (!components)
    {
      errors.emplace_back(components.error());
    }
    else
    {
      const auto schemas = toObject(components.value().at("schemas"), "schemas");
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
          const auto catSchema = toObject(schemas.value().at("Cat"), "Cat schema");
          if (!catSchema)
          {
            errors.emplace_back(catSchema.error());
          }
          else
          {
            const auto catProperties = toObject(catSchema.value().at("properties"), "Cat.properties");
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
                const auto nameSchema = toObject(catProperties.value().at("name"), "Cat.properties.name");
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
                    toObject(catProperties.value().at("dateOfBirth"), "Cat.properties.dateOfBirth");
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
          const auto statusSchema = toObject(schemas.value().at("CatStatus"), "CatStatus schema");
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
              const auto contains = arrayContainsString(statusSchema.value().at("enum"), status);
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

    const auto paths = toObject(document.value().at("paths"), "paths");
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
      const auto contains = containsSchemaRef(spec, std::string(ref));
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

std::vector<std::string> validateOpenApiDemoSpec()
{
  const auto spec = buildOpenApiSpec();
  if (!spec)
  {
    return {spec.error()};
  }

  return validateOpenApiDemoSpec(spec.value());
}
