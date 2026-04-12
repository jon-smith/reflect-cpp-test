#include <chrono>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include "core/api_server_support.hpp"
#include "core/openapi_builder.hpp"
#include "core/typed_api_routes.hpp"

namespace
{

clam::OpenApiJson::Object requireObject(const clam::OpenApiJson &json, const std::string &context)
{
  const auto object = clam::toObject(json, context);
  REQUIRE(object.has_value());
  return object.value();
}

std::string requireString(const clam::OpenApiJson &json, const std::string &context)
{
  const auto value = clam::toString(json, context);
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

template <class SetupFn> TestServerHandle startTestServer(SetupFn &&setup)
{
  TestServerHandle handle;
  setup(*handle.server);
  handle.port = handle.server->bind_to_any_port("127.0.0.1");
  REQUIRE(handle.port > 0);

  handle.thread = std::thread([server = handle.server.get()] { server->listen_after_bind(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  return handle;
}

}  // namespace

struct SyntheticResponse
{
  std::string ok;
};

struct SyntheticError
{
  std::string reason;
};

namespace clam
{
template <> struct OpenApiSchemaTraits<SyntheticResponse>
{
  static constexpr std::string_view name = "SyntheticResponse";
};

template <> struct OpenApiSchemaTraits<SyntheticError>
{
  static constexpr std::string_view name = "SyntheticError";
};
}  // namespace clam

static_assert(
    clam::TypedNoRequestHandler<decltype([](const httplib::Request &) -> clam::TypedRouteResult<SyntheticResponse, SyntheticError>
                                         { return SyntheticResponse{.ok = "yes"}; }),
                                SyntheticError, clam::TypedResponse<200, SyntheticResponse>>);

TEST_CASE("registerGeneratedSchemaDocument rewrites nested defs references", "[openapi][builder][core]")
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

  clam::OpenApiJson::Object schemas;
  const auto result = clam::registerGeneratedSchemaDocument(schemaJson, schemas);

  REQUIRE(result.has_value());
  REQUIRE(schemas.count("🐱") == 1U);
  REQUIRE(schemas.count("🐟") == 1U);
  REQUIRE(schemas.count("🌙") == 1U);

  const auto catContainsFish = clam::containsSchemaRef(schemas.at("🐱"), "#/components/schemas/🐟");
  REQUIRE(catContainsFish.has_value());
  CHECK(catContainsFish.value());

  const auto catContainsMoon = clam::containsSchemaRef(schemas.at("🐱"), "#/components/schemas/🌙");
  REQUIRE(catContainsMoon.has_value());
  CHECK(catContainsMoon.value());
}

TEST_CASE("buildOpenApiSpec assembles optional sections and component refs", "[openapi][builder][core]")
{
  const auto minimalSpec = clam::buildOpenApiSpec(clam::OpenApiSpecConfig{
      .info = {.title = "Minimal API", .version = "0.1.0", .description = "Minimal builder test."},
      .paths = {clam::OpenApiPathItem{
          .path = "/ping",
          .operations = {
              clam::OpenApiPathOperation{
                  .method = "get",
                  .operation =
                      {
                          .summary = "Ping",
                          .operationId = "ping",
                          .responses =
                              {
                                  clam::OpenApiResponse{
                                      .statusCode = "200",
                                      .description = "pong",
                                      .schemaName = "SyntheticError",
                                  },
                              },
                      },
              },
          },
      }},
      .schemaRegistrations = {clam::makeOpenApiSchemaRegistration<SyntheticError>()},
  });

  REQUIRE(minimalSpec.has_value());
  const auto minimalDocument = requireObject(minimalSpec.value(), "minimal document");
  CHECK(minimalDocument.count("servers") == 0U);
  const auto pingPath = requireObject(requireObject(minimalDocument.at("paths"), "paths").at("/ping"), "/ping");
  CHECK(requireObject(pingPath.at("get"), "/ping get").count("requestBody") == 0U);
}

TEST_CASE("shared route registration mounts GET and POST ApiRoute handlers", "[server][shared][core]")
{
  const auto serverHandle = startTestServer([](httplib::Server &server)
                                            {
                                              clam::registerRoutes(server, std::vector<clam::ApiRoute>{
                                                                              clam::ApiRoute{
                                                                                  .method = clam::HttpMethod::get,
                                                                                  .openApiPath = "/synthetic-get",
                                                                                  .httplibPattern = "/synthetic-get",
                                                                                  .handler =
                                                                                      [](const httplib::Request &,
                                                                                         httplib::Response &response)
                                                                                  {
                                                                                    response.status = 200;
                                                                                    response.set_content(R"({"ok":"get"})",
                                                                                                         "application/json");
                                                                                  },
                                                                              },
                                                                              clam::ApiRoute{
                                                                                  .method = clam::HttpMethod::post,
                                                                                  .openApiPath = "/synthetic-post",
                                                                                  .httplibPattern = "/synthetic-post",
                                                                                  .handler =
                                                                                      [](const httplib::Request &request,
                                                                                         httplib::Response &response)
                                                                                  {
                                                                                    response.status = 201;
                                                                                    response.set_content(request.body,
                                                                                                         "application/json");
                                                                                  },
                                                                              },
                                                                          }); });

  httplib::Client client("127.0.0.1", serverHandle.port);
  const auto getResponse = client.Get("/synthetic-get");
  REQUIRE(getResponse);
  CHECK(getResponse->status == 200);

  const auto postResponse = client.Post("/synthetic-post", R"({"ok":"post"})", "application/json");
  REQUIRE(postResponse);
  CHECK(postResponse->status == 201);
}

TEST_CASE("shared OpenAPI endpoint registration serves success and generic failure JSON", "[server][shared][core]")
{
  const auto successServer = startTestServer([](httplib::Server &server)
                                             {
                                               clam::registerOpenApiJsonEndpoint(server,
                                                                                []
                                                                                {
                                                                                  return std::expected<clam::OpenApiJson, std::string>(
                                                                                      clam::makeObject({
                                                                                          {"openapi", "3.1.0"},
                                                                                          {"info", clam::makeObject({{"title", "Synthetic API"},
                                                                                                                     {"version", "0.0.1"}})},
                                                                                          {"paths", clam::makeObject({})},
                                                                                      }));
                                                                                });
                                             });

  httplib::Client successClient("127.0.0.1", successServer.port);
  const auto successResponse = successClient.Get("/openapi.json");
  REQUIRE(successResponse);
  CHECK(successResponse->status == 200);

  const auto failureServer = startTestServer([](httplib::Server &server)
                                             {
                                               clam::registerOpenApiJsonEndpoint(server,
                                                                                []
                                                                                { return std::unexpected(std::string("synthetic failure")); },
                                                                                "/synthetic-openapi.json");
                                             });

  httplib::Client failureClient("127.0.0.1", failureServer.port);
  const auto failureResponse = failureClient.Get("/synthetic-openapi.json");
  REQUIRE(failureResponse);
  CHECK(failureResponse->status == 500);
  const auto failureBody = requireObject(requireJsonBody<clam::OpenApiJson>(failureResponse->body), "failure body");
  CHECK(requireString(failureBody.at("error"), "error") == "openapi_generation_failed");
  CHECK(requireString(failureBody.at("detail"), "detail") == "synthetic failure");
}
