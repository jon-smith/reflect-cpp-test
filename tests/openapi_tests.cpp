#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cat_api_types.hpp"
#include "openapi_builder.hpp"
#include "openapi_demo.hpp"

namespace
{

OpenApiJson::Object requireObject(const OpenApiJson &json, const std::string &context)
{
  const auto object = toObject(json, context);
  REQUIRE(object.has_value());
  return object.value();
}

std::string requireString(const OpenApiJson &json, const std::string &context)
{
  const auto value = toString(json, context);
  REQUIRE(value.has_value());
  return value.value();
}

}

TEST_CASE("registerGeneratedSchemaDocument rewrites nested defs references", "[openapi][builder]")
{
  const std::string schemaJson = R"({
    "$defs": {
      "🐱": {
        "type": "object",
        "properties": {
          "friend": { "$ref": "#/$defs/🐟" },
          "items": {
            "type": "array",
            "items": { "$ref": "#/$defs/🌙" }
          }
        }
      },
      "🐟": {
        "type": "string"
      },
      "🌙": {
        "type": "object",
        "properties": {
          "name": { "$ref": "#/$defs/🐟" }
        }
      }
    }
  })";

  OpenApiJson::Object schemas;
  const auto result = registerGeneratedSchemaDocument(schemaJson, schemas);

  REQUIRE(result.has_value());
  REQUIRE(schemas.count("🐱") == 1U);
  REQUIRE(schemas.count("🐟") == 1U);
  REQUIRE(schemas.count("🌙") == 1U);

  const auto catContainsFish = containsSchemaRef(schemas.at("🐱"), "#/components/schemas/🐟");
  REQUIRE(catContainsFish.has_value());
  CHECK(catContainsFish.value());

  const auto catContainsMoon = containsSchemaRef(schemas.at("🐱"), "#/components/schemas/🌙");
  REQUIRE(catContainsMoon.has_value());
  CHECK(catContainsMoon.value());

  const auto moonContainsFish = containsSchemaRef(schemas.at("🌙"), "#/components/schemas/🐟");
  REQUIRE(moonContainsFish.has_value());
  CHECK(moonContainsFish.value());
}

TEST_CASE("buildOpenApiSpec assembles optional sections and component refs", "[openapi][builder]")
{
  const auto minimalSpec = buildOpenApiSpec(OpenApiSpecConfig{
      .info =
          {
              .title = "Minimal API",
              .version = "0.1.0",
              .description = "Minimal builder test.",
          },
      .paths =
          {
              OpenApiPathItem{
                  .path = "/ping",
                  .operations =
                      {
                          OpenApiPathOperation{
                              .method = "get",
                              .operation =
                                  {
                                      .summary = "Ping",
                                      .operationId = "ping",
                                      .responses =
                                          {
                                              OpenApiResponse{
                                                  .statusCode = "200",
                                                  .description = "pong",
                                                  .schemaName = "ErrorResponse",
                                              },
                                          },
                                  },
                          },
                      },
              },
          },
      .schemaRegistrations = {makeOpenApiSchemaRegistration<ErrorResponse>()},
  });

  REQUIRE(minimalSpec.has_value());

  const auto minimalDocument = requireObject(minimalSpec.value(), "minimal document");
  CHECK(minimalDocument.count("servers") == 0U);

  const auto minimalPaths = requireObject(minimalDocument.at("paths"), "minimal paths");
  const auto pingPath = requireObject(minimalPaths.at("/ping"), "/ping path");
  const auto pingOperation = requireObject(pingPath.at("get"), "/ping operation");
  CHECK(pingOperation.count("requestBody") == 0U);

  const auto minimalContainsErrorRef = containsSchemaRef(minimalSpec.value(), "#/components/schemas/ErrorResponse");
  REQUIRE(minimalContainsErrorRef.has_value());
  CHECK(minimalContainsErrorRef.value());

  const auto configuredSpec = buildOpenApiSpec(OpenApiSpecConfig{
      .info =
          {
              .title = "Configured API",
              .version = "1.0.0",
              .description = "Configured builder test.",
          },
      .servers =
          {
              OpenApiServer{
                  .url = "http://localhost:8080",
                  .description = "Local test server",
              },
          },
      .paths =
          {
              OpenApiPathItem{
                  .path = "/cats/{catId}",
                  .operations =
                      {
                          OpenApiPathOperation{
                              .method = "post",
                              .operation =
                                  {
                                      .summary = "Create cat log",
                                      .operationId = "createCatLog",
                                      .parameters =
                                          {
                                              OpenApiParameter{
                                                  .name = "catId",
                                                  .in = "path",
                                                  .required = true,
                                                  .description = "Cat identifier",
                                                  .schema = stringSchema(),
                                              },
                                          },
                                      .requestBody =
                                          OpenApiRequestBody{
                                              .required = true,
                                              .schemaName = "CreateCatLogEntryRequest",
                                          },
                                      .responses =
                                          {
                                              OpenApiResponse{
                                                  .statusCode = "201",
                                                  .description = "Created entry",
                                                  .schemaName = "CatLogEntry",
                                              },
                                              errorResponse("400", "Bad request."),
                                          },
                                  },
                          },
                      },
              },
          },
      .schemaRegistrations =
          {
              makeOpenApiSchemaRegistration<CreateCatLogEntryRequest>(),
              makeOpenApiSchemaRegistration<CatLogEntry>(),
              makeOpenApiSchemaRegistration<ErrorResponse>(),
          },
  });

  REQUIRE(configuredSpec.has_value());

  const auto configuredDocument = requireObject(configuredSpec.value(), "configured document");
  REQUIRE(configuredDocument.count("servers") == 1U);

  const auto servers = configuredDocument.at("servers").to_array();
  REQUIRE(servers.has_value());
  REQUIRE(servers.value().size() == 1U);

  const auto server = requireObject(servers.value().front(), "server");
  CHECK(requireString(server.at("url"), "server url") == "http://localhost:8080");

  const auto configuredPaths = requireObject(configuredDocument.at("paths"), "configured paths");
  const auto catLogsPath = requireObject(configuredPaths.at("/cats/{catId}"), "cat logs path");
  const auto postOperation = requireObject(catLogsPath.at("post"), "post operation");

  REQUIRE(postOperation.count("parameters") == 1U);
  REQUIRE(postOperation.count("requestBody") == 1U);

  const auto requestBody = requireObject(postOperation.at("requestBody"), "request body");
  const auto requestContent = requireObject(requestBody.at("content"), "request content");
  const auto requestJson = requireObject(requestContent.at("application/json"), "request content type");
  const auto requestSchema = requireObject(requestJson.at("schema"), "request schema");
  CHECK(requireString(requestSchema.at("$ref"), "request schema ref") ==
        "#/components/schemas/CreateCatLogEntryRequest");

  const auto responses = requireObject(postOperation.at("responses"), "responses");
  const auto created = requireObject(responses.at("201"), "created response");
  const auto createdContent = requireObject(created.at("content"), "created content");
  const auto createdJson = requireObject(createdContent.at("application/json"), "created content type");
  const auto createdSchema = requireObject(createdJson.at("schema"), "created schema");
  CHECK(requireString(createdSchema.at("$ref"), "created schema ref") == "#/components/schemas/CatLogEntry");

  const auto containsRequestRef =
      containsSchemaRef(configuredSpec.value(), "#/components/schemas/CreateCatLogEntryRequest");
  REQUIRE(containsRequestRef.has_value());
  CHECK(containsRequestRef.value());

  const auto containsResponseRef = containsSchemaRef(configuredSpec.value(), "#/components/schemas/CatLogEntry");
  REQUIRE(containsResponseRef.has_value());
  CHECK(containsResponseRef.value());
}

TEST_CASE("CatLog demo spec passes integration validation", "[openapi][demo]")
{
  const auto spec = buildOpenApiSpec();

  REQUIRE(spec.has_value());
  const auto errors = validateOpenApiDemoSpec(spec.value());
  CHECK(errors.empty());
}
