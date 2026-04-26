#include <chrono>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include "core/typed_api_routes.hpp"
#include "demo/cat_api_routes.hpp"
#include "demo/cat_api_server.hpp"
#include "demo/cat_api_types.hpp"
#include "demo/openapi_demo.hpp"

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
private:
  httplib::Server server;
  std::thread thread;
  int port;

public:
  TestServerHandle(const TestServerHandle &) = delete;
  TestServerHandle &operator=(const TestServerHandle &) = delete;
  TestServerHandle(TestServerHandle &&) noexcept = delete;
  TestServerHandle &operator=(TestServerHandle &&) noexcept = delete;

  TestServerHandle()
  {
    registerCatApiRoutes(server);
    port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    thread = std::thread([this] { server.listen_after_bind(); });

    // todo, replace with cv
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

}

static_assert(clam::TypedNoRequestHandler<
              decltype([](const httplib::Request &) -> clam::TypedRouteResult<Cat, ErrorResponse>
                       { return Cat{.catId = "cat-1", .name = "Mochi", .dateOfBirth = "2020-01-01"}; }),
              ErrorResponse, clam::TypedResponse<200, Cat>>);

static_assert(
    clam::TypedRequestHandler<decltype([](const httplib::Request &,
                                          const CreateCatRequest &) -> clam::TypedRouteResult<Cat, ErrorResponse>
                                       { return Cat{.catId = "cat-1", .name = "Mochi", .dateOfBirth = "2020-01-01"}; }),
                              CreateCatRequest, ErrorResponse, clam::TypedResponse<201, Cat>>);

TEST_CASE("CatLog demo spec passes integration validation", "[openapi][demo]")
{
  const auto spec = buildCatLogOpenApiSpec();
  REQUIRE(spec.has_value());
  CHECK(validateOpenApiDemoSpec(spec.value()).empty());
}

TEST_CASE("Cat API route registry is the shared source of truth", "[openapi][routes][demo]")
{
  const auto routes = makeCatApiRoutes();

  REQUIRE(routes.size() == 5U);
  CHECK(routes[0].method == clam::HttpMethod::get);
  CHECK(routes[1].method == clam::HttpMethod::post);
  CHECK(routes[2].openApiPath == "/cats/{catId}");
  CHECK(routes[4].operation.operationId == "createCatLogEntry");
  REQUIRE(routes[1].operation.requestBody.has_value());
  CHECK(routes[1].operation.requestBody->schemaName == "CreateCatRequest");
}

TEST_CASE("Cat API server exposes OpenAPI and stub routes", "[server][demo]")
{
  TestServerHandle handle;
  httplib::Client client("127.0.0.1", handle.getPort());

  const auto openapiResponse = client.Get("/openapi.json");
  REQUIRE(openapiResponse);
  CHECK(openapiResponse->status == 200);
  const auto spec = requireJsonBody<clam::OpenApiJson>(openapiResponse->body);
  CHECK(requireString(requireObject(spec, "served spec").at("openapi"), "version") == "3.1.0");

  const auto catsResponse = client.Get("/cats");
  REQUIRE(catsResponse);
  CHECK(catsResponse->status == 200);
  const auto cats = requireJsonBody<CatListResponse>(catsResponse->body);
  REQUIRE(cats.cats.get().size() == 1U);

  const auto missingCatResponse = client.Get("/cats/not-found");
  REQUIRE(missingCatResponse);
  CHECK(missingCatResponse->status == 404);

  const auto createCatResponse =
      client.Post("/cats", R"({"name":"Mittens","dateOfBirth":"2020-02-02"})", "application/json");
  REQUIRE(createCatResponse);
  CHECK(createCatResponse->status == 201);

  const auto invalidCreateCatResponse = client.Post("/cats", R"({"name":""})", "application/json");
  REQUIRE(invalidCreateCatResponse);
  CHECK(invalidCreateCatResponse->status == 400);
}
