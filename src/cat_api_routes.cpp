#include "cat_api_routes.hpp"

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

CatApiRoute makeListCatsRoute()
{
  return makeTypedGetRoute<TypedResponse<200, CatListResponse>>(
      {
          .metadata =
              {
                  .method = "get",
                  .openApiPath = "/cats",
                  .httplibPattern = "/cats",
                  .summary = "List cats",
                  .operationId = "listCats",
              },
          .successDescription = "A list of cats.",
          .errorResponses =
              {
                  {
                      .status = 500,
                      .description = "Unexpected server error.",
                      .code = "server_error",
                      .message = "Unexpected server error.",
                  },
              },
      },
      [](const httplib::Request &) -> TypedRouteResult<CatListResponse>
      { return CatListResponse{.cats = dataStore.getAllCatSummaries()}; });
}

CatApiRoute makeCreateCatRoute()
{
  return makeTypedBodyRoute<CreateCatRequest, TypedResponse<201, Cat>>(
      {
          .metadata =
              {
                  .method = "post",
                  .openApiPath = "/cats",
                  .httplibPattern = "/cats",
                  .summary = "Create a cat",
                  .operationId = "createCat",
              },
          .successDescription = "The created cat.",
          .parseErrorResponse =
              {
                  .status = 400,
                  .description = "The request body was invalid.",
                  .code = "invalid_request",
                  .message = "The request body was invalid.",
              },
      },
      [](const httplib::Request &, const CreateCatRequest &body) -> TypedRouteResult<Cat>
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

CatApiRoute makeGetCatRoute()
{
  const TypedErrorResponseSpec catNotFound{
      .status = 404,
      .description = "The cat was not found.",
      .code = "cat_not_found",
      .message = "The cat was not found.",
  };

  return makeTypedGetRoute<TypedResponse<200, Cat>>(
      {
          .metadata =
              {
                  .method = "get",
                  .openApiPath = "/cats/{catId}",
                  .httplibPattern = R"(/cats/([^/]+))",
                  .summary = "Get a cat by id",
                  .operationId = "getCatById",
                  .parameters = {makeCatIdParameter("Unique identifier for a previously created cat.")},
              },
          .successDescription = "The requested cat.",
          .errorResponses = {catNotFound},
      },
      [catNotFound](const httplib::Request &request) -> TypedRouteResult<Cat>
      {
        const auto catId = request.matches[1].str();
        const auto cat = dataStore.tryGetCat(catId);
        if (!cat)
        {
          return makeTypedRouteError(catNotFound, catId);
        }

        return cat.value();
      });
}

CatApiRoute makeListCatLogsRoute()
{
  const TypedErrorResponseSpec catNotFound{
      .status = 404,
      .description = "The cat was not found.",
      .code = "cat_not_found",
      .message = "The cat was not found.",
  };

  return makeTypedGetRoute<TypedResponse<200, CatLogListResponse>>(
      {
          .metadata =
              {
                  .method = "get",
                  .openApiPath = "/cats/{catId}/logs",
                  .httplibPattern = R"(/cats/([^/]+)/logs)",
                  .summary = "List cat status logs",
                  .operationId = "listCatLogs",
                  .parameters = {makeCatIdParameter("Unique identifier for the cat whose logs are being requested.")},
              },
          .successDescription = "Status log entries for the cat.",
          .errorResponses = {catNotFound},
      },
      [catNotFound](const httplib::Request &request) -> TypedRouteResult<CatLogListResponse>
      {
        const auto catId = request.matches[1].str();
        const auto catLog = dataStore.tryGetCatLog(catId);
        if (!catLog)
        {
          return makeTypedRouteError(catNotFound, catId);
        }

        return CatLogListResponse{.logs = std::vector<CatLogEntry>{catLog.value()}};
      });
}

CatApiRoute makeCreateCatLogRoute()
{
  const TypedErrorResponseSpec invalidRequest{
      .status = 400,
      .description = "The request body was invalid.",
      .code = "invalid_request",
      .message = "The request body was invalid.",
  };
  const TypedErrorResponseSpec catNotFound{
      .status = 404,
      .description = "The cat was not found.",
      .code = "cat_not_found",
      .message = "The cat was not found.",
  };

  return makeTypedBodyRoute<CreateCatLogEntryRequest, TypedResponse<201, CatLogEntry>>(
      {
          .metadata =
              {
                  .method = "post",
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
                    const CreateCatLogEntryRequest &body) -> TypedRouteResult<CatLogEntry>
      {
        const auto catId = request.matches[1].str();
        if (!dataStore.tryGetCat(catId))
        {
          return makeTypedRouteError(catNotFound, catId);
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

}  // namespace

OpenApiResponse makeTypedErrorOpenApiResponse(const TypedErrorResponseSpec &errorResponse)
{
  return OpenApiResponse::fromType<ErrorResponse>(std::to_string(errorResponse.status), errorResponse.description,
                                                  errorResponse.contentType);
}

TypedRouteError makeTypedRouteError(const TypedErrorResponseSpec &errorResponse, const std::string &detail)
{
  return TypedRouteError{
      .status = errorResponse.status,
      .payload = makeErrorResponse(errorResponse.code, errorResponse.message, detail),
  };
}

std::vector<CatApiRoute> makeCatApiRoutes()
{
  return {
      makeListCatsRoute(), makeCreateCatRoute(), makeGetCatRoute(), makeListCatLogsRoute(), makeCreateCatLogRoute(),
  };
}
