#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include "cat_api_routes.hpp"
#include "cat_api_server.hpp"
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

template <class T> T requireJsonBody(const std::string &body)
{
  const auto parsed = rfl::json::read<T>(body);
  REQUIRE(parsed.has_value());
  return parsed.value();
}

struct TestServerHandle
{
  std::unique_ptr<httplib::Server> server = std::make_unique<httplib::Server>();
  std::thread thread;
  int port = -1;

  TestServerHandle() = default;
  TestServerHandle(const TestServerHandle &) = delete;
  TestServerHandle &operator=(const TestServerHandle &) = delete;
  TestServerHandle(TestServerHandle &&) noexcept = default;
  TestServerHandle &operator=(TestServerHandle &&) noexcept = default;

  ~TestServerHandle()
  {
    server->stop();
    if (thread.joinable())
    {
      thread.join();
    }
  }
};

TestServerHandle startTestServer()
{
  TestServerHandle handle;
  registerCatApiRoutes(*handle.server);
  handle.port = handle.server->bind_to_any_port("127.0.0.1");
  REQUIRE(handle.port > 0);

  handle.thread = std::thread([server = handle.server.get()]
                              { server->listen_after_bind(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  return handle;
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

TEST_CASE("Cat API route registry is the shared source of truth", "[openapi][routes]")
{
  const auto routes = makeCatApiRoutes();

  REQUIRE(routes.size() == 5U);
  CHECK(routes[0].method == "get");
  CHECK(routes[0].openApiPath == "/cats");
  CHECK(routes[0].operation.operationId == "listCats");

  CHECK(routes[1].method == "post");
  CHECK(routes[1].openApiPath == "/cats");
  CHECK(routes[1].operation.operationId == "createCat");

  CHECK(routes[2].openApiPath == "/cats/{catId}");
  CHECK(routes[2].operation.operationId == "getCatById");

  CHECK(routes[3].openApiPath == "/cats/{catId}/logs");
  CHECK(routes[3].operation.operationId == "listCatLogs");

  CHECK(routes[4].openApiPath == "/cats/{catId}/logs");
  CHECK(routes[4].operation.operationId == "createCatLogEntry");
}

TEST_CASE("Cat API server exposes OpenAPI and stub routes", "[server]")
{
  auto handle = startTestServer();
  httplib::Client client("127.0.0.1", handle.port);

  const auto openapiResponse = client.Get("/openapi.json");
  REQUIRE(openapiResponse);
  CHECK(openapiResponse->status == 200);
  const auto spec = requireJsonBody<OpenApiJson>(openapiResponse->body);
  const auto specDocument = requireObject(spec, "served spec");
  CHECK(requireString(specDocument.at("openapi"), "served openapi version") == "3.1.0");

  const auto catsResponse = client.Get("/cats");
  REQUIRE(catsResponse);
  CHECK(catsResponse->status == 200);
  const auto cats = requireJsonBody<CatListResponse>(catsResponse->body);
  REQUIRE(cats.cats.get().size() == 1U);
  CHECK(cats.cats.get().front().catId.get() == "senor-don-gato");

  const auto missingCatResponse = client.Get("/cats/not-found");
  REQUIRE(missingCatResponse);
  CHECK(missingCatResponse->status == 404);
  const auto missingCat = requireJsonBody<ErrorResponse>(missingCatResponse->body);
  CHECK(missingCat.code.get() == "cat_not_found");

  const auto createCatResponse =
      client.Post("/cats", R"({"name":"Mittens","dateOfBirth":"2020-02-02"})", "application/json");
  REQUIRE(createCatResponse);
  CHECK(createCatResponse->status == 201);
  const auto createdCat = requireJsonBody<Cat>(createCatResponse->body);
  CHECK(createdCat.catId.get() == "created-cat");

  const auto invalidCreateCatResponse = client.Post("/cats", R"({"name":""})", "application/json");
  REQUIRE(invalidCreateCatResponse);
  CHECK(invalidCreateCatResponse->status == 400);
  const auto invalidCreateCat = requireJsonBody<ErrorResponse>(invalidCreateCatResponse->body);
  CHECK(invalidCreateCat.code.get() == "invalid_request");

  const auto createLogResponse = client.Post("/cats/senor-don-gato/logs",
                                             R"({"status":"zoomy","loggedAt":"2024-05-01T10:00:00Z"})",
                                             "application/json");
  REQUIRE(createLogResponse);
  CHECK(createLogResponse->status == 201);
  const auto createdLog = requireJsonBody<CatLogEntry>(createLogResponse->body);
  CHECK(createdLog.catId.get() == "senor-don-gato");

  const auto missingLogResponse = client.Get("/cats/not-found/logs");
  REQUIRE(missingLogResponse);
  CHECK(missingLogResponse->status == 404);
}
