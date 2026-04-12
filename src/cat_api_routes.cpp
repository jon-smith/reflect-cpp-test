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

constexpr auto kKnownCatId = "senor-don-gato";

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

CatSummary makeKnownCatSummary()
{
  return CatSummary{
      .catId = std::string(kKnownCatId),
      .name = "Senor Don Gato",
      .latestStatus = CatStatus::cute,
  };
}

Cat makeKnownCat()
{
  return Cat{
      .catId = std::string(kKnownCatId),
      .name = "Senor Don Gato",
      .breed = "Siamese",
      .dateOfBirth = "1999-08-10",
      .notes = "Resident demo cat.",
  };
}

CatLogEntry makeKnownLog()
{
  return CatLogEntry{
      .logId = "log-1",
      .catId = std::string(kKnownCatId),
      .status = CatStatus::cute,
      .loggedAt = "2024-05-01T09:30:00Z",
      .note = "Holding the API together with pure charisma.",
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

bool isKnownCatId(const std::string &catId)
{
  return catId == kKnownCatId;
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

void writeErrorResponse(httplib::Response &response,
                        int status,
                        const std::string &code,
                        const std::string &message,
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
        writeJsonResponse(response, 200, CatListResponse{.cats = std::vector<CatSummary>{makeKnownCatSummary()}});
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
        writeJsonResponse(response,
                          201,
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
              .parameters = {catIdParameter("Unique identifier for a previously created cat.")},
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
        if (!isKnownCatId(catId))
        {
          writeErrorResponse(response, 404, "cat_not_found", "The cat was not found.", catId);
          return;
        }

        writeJsonResponse(response, 200, makeKnownCat());
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
              .parameters = {catIdParameter("Unique identifier for the cat whose logs are being requested.")},
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
        if (!isKnownCatId(catId))
        {
          writeErrorResponse(response, 404, "cat_not_found", "The cat was not found.", catId);
          return;
        }

        writeJsonResponse(response, 200, CatLogListResponse{.logs = std::vector<CatLogEntry>{makeKnownLog()}});
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
              .parameters = {catIdParameter("Unique identifier for the cat receiving the new log entry.")},
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
        if (!isKnownCatId(catId))
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
        writeJsonResponse(response,
                          201,
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

}  // namespace

std::vector<CatApiRoute> makeCatApiRoutes()
{
  return {
      makeListCatsRoute(),
      makeCreateCatRoute(),
      makeGetCatRoute(),
      makeListCatLogsRoute(),
      makeCreateCatLogRoute(),
  };
}
