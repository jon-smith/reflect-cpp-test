#include <chrono>
#include <concepts>
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

clam::OpenApiJson::Object RequireObject(const clam::OpenApiJson &json, const std::string &context)
{
  const auto object = clam::ToObject(json, context);
  REQUIRE(object.has_value());
  return object.value();
}

std::string RequireString(const clam::OpenApiJson &json, const std::string &context)
{
  const auto value = clam::ToString(json, context);
  REQUIRE(value.has_value());
  return value.value();
}

template <typename T> T RequireJsonBody(const std::string &body)
{
  const auto parsed = rfl::json::read<T>(body);
  REQUIRE(parsed.has_value());
  return parsed.value();
}

struct TestServerHandle
{
private:
  httplib::Server server;
  std::thread thread;
  int port;

public:
  TestServerHandle(const TestServerHandle &) = delete;
  TestServerHandle &operator=(const TestServerHandle &) = delete;
  TestServerHandle(TestServerHandle &&) noexcept = delete;
  TestServerHandle &operator=(TestServerHandle &&) noexcept = delete;

  template <typename SetupFn> explicit TestServerHandle(SetupFn &&setup)
  {
    setup(server);
    port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    thread = std::thread([this] { server.listen_after_bind(); });
    server.wait_until_ready();
    REQUIRE(server.is_running());
  }

  ~TestServerHandle()
  {
    server.stop();
    if (thread.joinable())
    {
      thread.join();
    }
  }

  int getPort() const
  {
    return port;
  }
};

template <typename SetupFn> TestServerHandle StartTestServer(SetupFn &&setup)
{
  return TestServerHandle(std::forward<SetupFn>(setup));
}

}

struct SyntheticResponse
{
  std::string ok;
};

struct SyntheticError
{
  std::string reason;
};

struct OptionalSyntheticRequest
{
  std::string value;
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

template <> struct OpenApiSchemaTraits<OptionalSyntheticRequest>
{
  static constexpr std::string_view name = "OptionalSyntheticRequest";
};
}

static_assert(clam::TypedNoRequestHandler<
              decltype([](const httplib::Request &) -> clam::TypedRouteResult<SyntheticResponse, SyntheticError>
                       { return SyntheticResponse{.ok = "yes"}; }),
              SyntheticError, clam::TypedResponse<200, SyntheticResponse>>);

TEST_CASE("RegisterGeneratedSchemaDocument rewrites nested defs references", "[openapi][builder][core]")
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
  const auto result = clam::RegisterGeneratedSchemaDocument(schemaJson, schemas);

  REQUIRE(result.has_value());
  REQUIRE(schemas.count("🐱") == 1U);
  REQUIRE(schemas.count("🐟") == 1U);
  REQUIRE(schemas.count("🌙") == 1U);

  const auto catContainsFish = clam::ContainsSchemaRef(schemas.at("🐱"), "#/components/schemas/🐟");
  REQUIRE(catContainsFish.has_value());
  CHECK(catContainsFish.value());

  const auto catContainsMoon = clam::ContainsSchemaRef(schemas.at("🐱"), "#/components/schemas/🌙");
  REQUIRE(catContainsMoon.has_value());
  CHECK(catContainsMoon.value());
}

TEST_CASE("BuildOpenApiSpec assembles optional sections and component refs", "[openapi][builder][core]")
{
  const auto minimalSpec = clam::BuildOpenApiSpec(clam::OpenApiSpecConfig{
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
      .schemaRegistrations = {clam::MakeOpenApiSchemaRegistration<SyntheticError>()},
  });

  REQUIRE(minimalSpec.has_value());
  const auto minimalDocument = RequireObject(minimalSpec.value(), "minimal document");
  CHECK(minimalDocument.count("servers") == 0U);
  const auto pingPath = RequireObject(RequireObject(minimalDocument.at("paths"), "paths").at("/ping"), "/ping");
  CHECK(RequireObject(pingPath.at("get"), "/ping get").count("requestBody") == 0U);
}

TEST_CASE("shared route registration mounts GET and POST ApiRoute handlers", "[server][shared][core]")
{
  const auto serverHandle = StartTestServer(
      [](httplib::Server &server)
      {
        clam::RegisterRoutes(server,
                             std::vector<clam::ApiRoute>{
                                 clam::ApiRoute{
                                     .method = clam::HttpMethod::get,
                                     .openApiPath = "/synthetic-get",
                                     .httplibPattern = "/synthetic-get",
                                     .handler =
                                         [](const httplib::Request &, httplib::Response &response)
                                     {
                                       response.status = 200;
                                       response.set_content(R"({"ok":"get"})", "application/json");
                                     },
                                 },
                                 clam::ApiRoute{
                                     .method = clam::HttpMethod::post,
                                     .openApiPath = "/synthetic-post",
                                     .httplibPattern = "/synthetic-post",
                                     .handler =
                                         [](const httplib::Request &request, httplib::Response &response)
                                     {
                                       response.status = 201;
                                       response.set_content(request.body, "application/json");
                                     },
                                 },
                             });
      });

  httplib::Client client("127.0.0.1", serverHandle.getPort());
  const auto getResponse = client.Get("/synthetic-get");
  REQUIRE(getResponse);
  CHECK(getResponse->status == 200);

  const auto postResponse = client.Post("/synthetic-post", R"({"ok":"post"})", "application/json");
  REQUIRE(postResponse);
  CHECK(postResponse->status == 201);
}

TEST_CASE("typed body routes honour optional bodies and parse error content types", "[server][routes][core]")
{
  auto parseError = clam::TypedErrorResponseSpec<SyntheticError>{
      .status = 422,
      .description = "Synthetic parse failure.",
      .makePayload = [](const std::string &detail) { return SyntheticError{.reason = detail}; },
      .contentType = "application/problem+json",
  };

  const auto serverHandle = StartTestServer(
      [parseError](httplib::Server &server)
      {
        clam::RegisterRoute(server,
                            clam::MakeTypedBodyRoute<OptionalSyntheticRequest, SyntheticError,
                                                     clam::TypedResponse<200, SyntheticResponse>>(
                                {
                                    .metadata =
                                        {
                                            .method = clam::HttpMethod::post,
                                            .openApiPath = "/synthetic-optional",
                                            .httplibPattern = "/synthetic-optional",
                                            .summary = "Synthetic optional body",
                                            .operationId = "syntheticOptionalBody",
                                        },
                                    .requestBodyRequired = false,
                                    .successDescription = "Synthetic response.",
                                    .parseErrorResponse = parseError,
                                },
                                [](const httplib::Request &, const OptionalSyntheticRequest &body)
                                    -> clam::TypedRouteResult<SyntheticResponse, SyntheticError>
                                { return SyntheticResponse{.ok = body.value.empty() ? "empty" : body.value}; }));
      });

  httplib::Client client("127.0.0.1", serverHandle.getPort());
  const auto omittedBodyResponse = client.Post("/synthetic-optional");
  REQUIRE(omittedBodyResponse);
  CHECK(omittedBodyResponse->status == 200);
  CHECK(RequireJsonBody<SyntheticResponse>(omittedBodyResponse->body).ok == "empty");

  const auto invalidBodyResponse = client.Post("/synthetic-optional", "{", "application/json");
  REQUIRE(invalidBodyResponse);
  CHECK(invalidBodyResponse->status == 422);
  CHECK(invalidBodyResponse->get_header_value("Content-Type") == "application/problem+json");
}

TEST_CASE("shared OpenAPI endpoint registration serves success and generic failure JSON", "[server][shared][core]")
{
  const auto successServer = StartTestServer(
      [](httplib::Server &server)
      {
        clam::RegisterOpenApiJsonEndpoint(
            server,
            []
            {
              return std::expected<clam::OpenApiJson, std::string>(clam::MakeObject({
                  {"openapi", "3.1.0"},
                  {"info", clam::MakeObject({{"title", "Synthetic API"}, {"version", "0.0.1"}})},
                  {"paths", clam::MakeObject({})},
              }));
            });
      });

  httplib::Client successClient("127.0.0.1", successServer.getPort());
  const auto successResponse = successClient.Get("/openapi.json");
  REQUIRE(successResponse);
  CHECK(successResponse->status == 200);

  const auto failureServer = StartTestServer(
      [](httplib::Server &server)
      {
        clam::RegisterOpenApiJsonEndpoint(
            server, [] { return std::unexpected(std::string("synthetic failure")); }, "/synthetic-openapi.json");
      });

  httplib::Client failureClient("127.0.0.1", failureServer.getPort());
  const auto failureResponse = failureClient.Get("/synthetic-openapi.json");
  REQUIRE(failureResponse);
  CHECK(failureResponse->status == 500);
  const auto failureBody = RequireObject(RequireJsonBody<clam::OpenApiJson>(failureResponse->body), "failure body");
  CHECK(RequireString(failureBody.at("error"), "error") == "openapi_generation_failed");
  CHECK(RequireString(failureBody.at("detail"), "detail") == "synthetic failure");
}
