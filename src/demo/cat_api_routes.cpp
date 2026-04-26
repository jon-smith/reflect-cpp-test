#include "demo/cat_api_routes.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

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

template <typename T> using CatRouteResult = clam::TypedRouteResult<T, ErrorResponse>;

clam::OpenApiParameter makeCatIdParameter(const std::string &description)
{
  return clam::OpenApiParameter{
      .name = "catId",
      .in = "path",
      .required = true,
      .description = description,
      .schema = clam::stringSchema(),
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

clam::TypedErrorResponseSpec<ErrorResponse> makeErrorSpec(const int status, const std::string &description,
                                                          const std::string &code, const std::string &message)
{
  return clam::TypedErrorResponseSpec<ErrorResponse>{
      .status = status,
      .description = description,
      .makePayload = [code, message](const std::string &detail) { return makeErrorResponse(code, message, detail); },
  };
}

clam::ApiRoute makeListCatsRoute()
{
  return clam::makeTypedGetRoute<ErrorResponse, clam::TypedResponse<200, CatListResponse>>(
      {
          .metadata =
              {
                  .method = clam::HttpMethod::get,
                  .openApiPath = "/cats",
                  .httplibPattern = "/cats",
                  .summary = "List cats",
                  .operationId = "listCats",
              },
          .successDescription = "A list of cats.",
          .errorResponses =
              {
                  makeErrorSpec(500, "Unexpected server error.", "server_error", "Unexpected server error."),
              },
      },
      [](const httplib::Request &) -> CatRouteResult<CatListResponse>
      { return CatListResponse{.cats = dataStore.getAllCatSummaries()}; });
}

clam::ApiRoute makeCreateCatRoute()
{
  return clam::makeTypedBodyRoute<CreateCatRequest, ErrorResponse, clam::TypedResponse<201, Cat>>(
      {
          .metadata =
              {
                  .method = clam::HttpMethod::post,
                  .openApiPath = "/cats",
                  .httplibPattern = "/cats",
                  .summary = "Create a cat",
                  .operationId = "createCat",
              },
          .successDescription = "The created cat.",
          .parseErrorResponse =
              makeErrorSpec(400, "The request body was invalid.", "invalid_request", "The request body was invalid."),
      },
      [](const httplib::Request &, const CreateCatRequest &body) -> CatRouteResult<Cat>
      {
        return Cat{
            .catId = "created-cat",
            .name = body.name,
            .breed = body.breed,
            .dateOfBirth = body.dateOfBirth,
            .notes = body.notes,
        };
      });
}

clam::ApiRoute makeGetCatRoute()
{
  const auto catNotFound = makeErrorSpec(404, "The cat was not found.", "cat_not_found", "The cat was not found.");

  return clam::makeTypedGetRoute<ErrorResponse, clam::TypedResponse<200, Cat>>(
      {
          .metadata =
              {
                  .method = clam::HttpMethod::get,
                  .openApiPath = "/cats/{catId}",
                  .httplibPattern = R"(/cats/([^/]+))",
                  .summary = "Get a cat by id",
                  .operationId = "getCatById",
                  .parameters = {makeCatIdParameter("Unique identifier for a previously created cat.")},
              },
          .successDescription = "The requested cat.",
          .errorResponses = {catNotFound},
      },
      [catNotFound](const httplib::Request &request) -> CatRouteResult<Cat>
      {
        const auto catId = request.matches[1].str();
        const auto cat = dataStore.tryGetCat(catId);
        if (!cat)
        {
          return clam::makeTypedRouteError(catNotFound, catId);
        }

        return cat.value();
      });
}

clam::ApiRoute makeListCatLogsRoute()
{
  const auto catNotFound = makeErrorSpec(404, "The cat was not found.", "cat_not_found", "The cat was not found.");

  return clam::makeTypedGetRoute<ErrorResponse, clam::TypedResponse<200, CatLogListResponse>>(
      {
          .metadata =
              {
                  .method = clam::HttpMethod::get,
                  .openApiPath = "/cats/{catId}/logs",
                  .httplibPattern = R"(/cats/([^/]+)/logs)",
                  .summary = "List cat status logs",
                  .operationId = "listCatLogs",
                  .parameters = {makeCatIdParameter("Unique identifier for the cat whose logs are being requested.")},
              },
          .successDescription = "Status log entries for the cat.",
          .errorResponses = {catNotFound},
      },
      [catNotFound](const httplib::Request &request) -> CatRouteResult<CatLogListResponse>
      {
        const auto catId = request.matches[1].str();
        const auto catLog = dataStore.tryGetCatLog(catId);
        if (!catLog)
        {
          return clam::makeTypedRouteError(catNotFound, catId);
        }

        return CatLogListResponse{.logs = std::vector<CatLogEntry>{catLog.value()}};
      });
}

clam::ApiRoute makeCreateCatLogRoute()
{
  const auto invalidRequest =
      makeErrorSpec(400, "The request body was invalid.", "invalid_request", "The request body was invalid.");
  const auto catNotFound = makeErrorSpec(404, "The cat was not found.", "cat_not_found", "The cat was not found.");

  return clam::makeTypedBodyRoute<CreateCatLogEntryRequest, ErrorResponse, clam::TypedResponse<201, CatLogEntry>>(
      {
          .metadata =
              {
                  .method = clam::HttpMethod::post,
                  .openApiPath = "/cats/{catId}/logs",
                  .httplibPattern = R"(/cats/([^/]+)/logs)",
                  .summary = "Create a cat status log entry",
                  .operationId = "createCatLogEntry",
                  .parameters = {makeCatIdParameter("Unique identifier for the cat receiving the new log entry.")},
              },
          .successDescription = "The created log entry.",
          .parseErrorResponse = invalidRequest,
          .errorResponses = {catNotFound},
      },
      [catNotFound](const httplib::Request &request,
                    const CreateCatLogEntryRequest &body) -> CatRouteResult<CatLogEntry>
      {
        const auto catId = request.matches[1].str();
        if (!dataStore.tryGetCat(catId))
        {
          return clam::makeTypedRouteError(catNotFound, catId);
        }

        return CatLogEntry{
            .logId = "created-log",
            .catId = catId,
            .status = body.status,
            .loggedAt = body.loggedAt,
            .note = body.note,
        };
      });
}

}

std::vector<clam::ApiRoute> makeCatApiRoutes()
{
  return {
      makeListCatsRoute(), makeCreateCatRoute(), makeGetCatRoute(), makeListCatLogsRoute(), makeCreateCatLogRoute(),
  };
}
