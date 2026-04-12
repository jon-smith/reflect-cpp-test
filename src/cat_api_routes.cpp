#include "cat_api_routes.hpp"

#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <rfl/json.hpp>

#include "cat_api_types.hpp"

namespace
{

class MockDataStore
{
  static constexpr std::string MOCK_DB_CAT_ID = "senor-don-gato";

  const CatSummary MockCatSummary = {
      .catId = std::string(MOCK_DB_CAT_ID),
      .name = "Senor Don Gato",
      .latestStatus = CatStatus::cute,
  };

public:
  std::vector<CatSummary> getAllCatSummaries() const
  {
    return std::vector<CatSummary>{MockCatSummary};
  }

  std::optional<CatSummary> tryGetCatSummary(const std::string &catId) const
  {
    if (catId != MOCK_DB_CAT_ID)
    {
      return std::nullopt;
    }

    return MockCatSummary;
  }

  std::optional<Cat> tryGetCat(const std::string &catId) const
  {
    if (catId != MOCK_DB_CAT_ID)
    {
      return std::nullopt;
    }

    return Cat{
        .catId = std::string(MOCK_DB_CAT_ID),
        .name = "Senor Don Gato",
        .breed = "Siamese",
        .dateOfBirth = "1999-08-10",
        .notes = "Resident demo cat.",
    };
  }

  std::optional<CatLogEntry> tryGetCatLog(const std::string &catId) const
  {
    if (catId != MOCK_DB_CAT_ID)
    {
      return std::nullopt;
    }

    return CatLogEntry{
        .logId = "log-1",
        .catId = std::string(MOCK_DB_CAT_ID),
        .status = CatStatus::cute,
        .loggedAt = "2024-05-01T09:30:00Z",
        .note = "Holding the API together with pure charisma.",
    };
  }
};

const auto dataStore = MockDataStore{};

OpenApiParameter makeCatIdParameter(const std::string &description)
{
  return OpenApiParameter{
      .name = "catId",
      .in = "path",
      .required = true,
      .description = description,
      .schema = stringSchema(),
  };
}

ErrorResponse makeErrorResponse(const std::string &code, const std::string &message, const std::string &detail = "")
{
  return ErrorResponse{
      .code = code,
      .message = message,
      .detail = detail.empty() ? std::nullopt : std::optional<std::string>(detail),
  };
}

template <class T> std::expected<T, std::string> parseJsonBody(const httplib::Request &request)
{
  const auto result = rfl::json::read<T>(request.body);
  if (!result)
  {
    return std::unexpected(result.error().what());
  }
  return result.value();
}

template <class T> void writeJsonResponse(httplib::Response &response, int status, const T &payload)
{
  response.status = status;
  response.set_content(rfl::json::write(payload), "application/json");
}

void writeErrorResponse(httplib::Response &response, int status, const std::string &code, const std::string &message,
                        const std::string &detail = "")
{
  writeJsonResponse(response, status, makeErrorResponse(code, message, detail));
}

CatApiRoute makeListCatsRoute()
{
  return CatApiRoute{
      .method = "get",
      .openApiPath = "/cats",
      .httplibPattern = "/cats",
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
      .handler =
          [](const httplib::Request &, httplib::Response &response)
      {
        const auto allCats = dataStore.getAllCatSummaries();
        writeJsonResponse(response, 200, CatListResponse{.cats = allCats});
      },
  };
}

CatApiRoute makeCreateCatRoute()
{
  return CatApiRoute{
      .method = "post",
      .openApiPath = "/cats",
      .httplibPattern = "/cats",
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
      .handler =
          [](const httplib::Request &request, httplib::Response &response)
      {
        const auto parsed = parseJsonBody<CreateCatRequest>(request);
        if (!parsed)
        {
          writeErrorResponse(response, 400, "invalid_request", "The request body was invalid.", parsed.error());
          return;
        }

        const auto &body = parsed.value();
        writeJsonResponse(response, 201,
                          Cat{
                              .catId = "created-cat",
                              .name = body.name,
                              .breed = body.breed,
                              .dateOfBirth = body.dateOfBirth,
                              .notes = body.notes,
                          });
      },
  };
}

CatApiRoute makeGetCatRoute()
{
  return CatApiRoute{
      .method = "get",
      .openApiPath = "/cats/{catId}",
      .httplibPattern = R"(/cats/([^/]+))",
      .operation =
          {
              .summary = "Get a cat by id",
              .operationId = "getCatById",
              .parameters = {makeCatIdParameter("Unique identifier for a previously created cat.")},
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
      .handler =
          [](const httplib::Request &request, httplib::Response &response)
      {
        const auto catId = request.matches[1].str();
        const auto cat = dataStore.tryGetCat(catId);
        if (!cat)
        {
          writeErrorResponse(response, 404, "cat_not_found", "The cat was not found.", catId);
          return;
        }

        writeJsonResponse(response, 200, cat.value());
      },
  };
}

CatApiRoute makeListCatLogsRoute()
{
  return CatApiRoute{
      .method = "get",
      .openApiPath = "/cats/{catId}/logs",
      .httplibPattern = R"(/cats/([^/]+)/logs)",
      .operation =
          {
              .summary = "List cat status logs",
              .operationId = "listCatLogs",
              .parameters = {makeCatIdParameter("Unique identifier for the cat whose logs are being requested.")},
              .responses =
                  {
                      OpenApiResponse{
                          .statusCode = "200",
                          .description = "Status log entries for the cat.",
                          .schemaName = "CatLogListResponse",
                      },
                      errorResponse("404", "The cat was not found."),
                  },
          },
      .handler =
          [](const httplib::Request &request, httplib::Response &response)
      {
        const auto catId = request.matches[1].str();
        const auto catLog = dataStore.tryGetCatLog(catId);
        if (!catLog)
        {
          writeErrorResponse(response, 404, "cat_not_found", "The cat was not found.", catId);
          return;
        }

        writeJsonResponse(response, 200, CatLogListResponse{.logs = std::vector<CatLogEntry>{catLog.value()}});
      },
  };
}

CatApiRoute makeCreateCatLogRoute()
{
  return CatApiRoute{
      .method = "post",
      .openApiPath = "/cats/{catId}/logs",
      .httplibPattern = R"(/cats/([^/]+)/logs)",
      .operation =
          {
              .summary = "Create a cat status log entry",
              .operationId = "createCatLogEntry",
              .parameters = {makeCatIdParameter("Unique identifier for the cat receiving the new log entry.")},
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
      .handler =
          [](const httplib::Request &request, httplib::Response &response)
      {
        const auto catId = request.matches[1].str();
        if (!dataStore.tryGetCat(catId))
        {
          writeErrorResponse(response, 404, "cat_not_found", "The cat was not found.", catId);
          return;
        }

        const auto parsed = parseJsonBody<CreateCatLogEntryRequest>(request);
        if (!parsed)
        {
          writeErrorResponse(response, 400, "invalid_request", "The request body was invalid.", parsed.error());
          return;
        }

        const auto &body = parsed.value();
        writeJsonResponse(response, 201,
                          CatLogEntry{
                              .logId = "created-log",
                              .catId = catId,
                              .status = body.status,
                              .loggedAt = body.loggedAt,
                              .note = body.note,
                          });
      },
  };
}

} // namespace

std::vector<CatApiRoute> makeCatApiRoutes()
{
  return {
      makeListCatsRoute(), makeCreateCatRoute(), makeGetCatRoute(), makeListCatLogsRoute(), makeCreateCatLogRoute(),
  };
}
