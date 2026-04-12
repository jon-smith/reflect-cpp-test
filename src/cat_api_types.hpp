#pragma once

#include <optional>
#include <string>
#include <vector>

#include <rfl.hpp>

#include "openapi_builder.hpp"

using DateString = rfl::Pattern<R"(^\d{4}-\d{2}-\d{2}$)", "Date">;
using DateTimeString = rfl::Pattern<R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)", "DateTime">;
using NonEmptyString = rfl::Validator<std::string, rfl::Size<rfl::Minimum<1>>>;

enum class CatStatus
{
  sassy,
  sleepy,
  zoomy,
  cute
};

struct CatSummary
{
  rfl::Description<"Unique cat identifier used in URLs.", std::string> catId;
  rfl::Description<"Display name shown in cat listings.", NonEmptyString> name;
  rfl::Description<"Most recently logged status for this cat.", CatStatus> latestStatus;
};

struct Cat
{
  rfl::Description<"Unique cat identifier used in URLs.", std::string> catId;
  rfl::Description<"Display name presented to API clients.", NonEmptyString> name;
  rfl::Description<"Optional breed information for the cat.", std::optional<std::string>> breed;
  rfl::Description<"Date of birth in YYYY-MM-DD format.", DateString> dateOfBirth;
  rfl::Description<"Optional notes about the cat.", std::optional<NonEmptyString>> notes;
};

struct CreateCatRequest
{
  rfl::Description<"Display name presented to API clients.", NonEmptyString> name;
  rfl::Description<"Optional breed information for the cat.", std::optional<std::string>> breed;
  rfl::Description<"Date of birth in YYYY-MM-DD format.", DateString> dateOfBirth;
  rfl::Description<"Optional notes about the cat.", std::optional<NonEmptyString>> notes;
};

struct CatLogEntry
{
  rfl::Description<"Unique log identifier for this status entry.", std::string> logId;
  rfl::Description<"Identifier of the cat this status belongs to.", std::string> catId;
  rfl::Description<"Cat mood/status captured by the entry.", CatStatus> status;
  rfl::Description<"Timestamp recorded in UTC as YYYY-MM-DDTHH:MM:SSZ.", DateTimeString> loggedAt;
  rfl::Description<"Optional note recorded alongside the status.", std::optional<NonEmptyString>> note;
};

struct CreateCatLogEntryRequest
{
  rfl::Description<"Cat mood/status captured by the entry.", CatStatus> status;
  rfl::Description<"Timestamp recorded in UTC as YYYY-MM-DDTHH:MM:SSZ.", DateTimeString> loggedAt;
  rfl::Description<"Optional note recorded alongside the status.", std::optional<NonEmptyString>> note;
};

struct CatListResponse
{
  rfl::Description<"Cats available from the collection endpoint.", std::vector<CatSummary>> cats;
};

struct CatLogListResponse
{
  rfl::Description<"Status log entries recorded for a cat.", std::vector<CatLogEntry>> logs;
};

struct ErrorResponse
{
  rfl::Description<"Stable machine-readable error code.", std::string> code;
  rfl::Description<"Human-readable summary of the failure.", std::string> message;
  rfl::Description<"Additional debugging context when available.", std::optional<std::string>> detail;
};

#define OPENAPI_SCHEMA_TRAITS(TYPE)      \
  template <> struct OpenApiSchemaTraits<TYPE> \
  {                                      \
    static constexpr std::string_view name = #TYPE; \
  }

OPENAPI_SCHEMA_TRAITS(Cat);
OPENAPI_SCHEMA_TRAITS(CatSummary);
OPENAPI_SCHEMA_TRAITS(CreateCatRequest);
OPENAPI_SCHEMA_TRAITS(CatLogEntry);
OPENAPI_SCHEMA_TRAITS(CreateCatLogEntryRequest);
OPENAPI_SCHEMA_TRAITS(CatListResponse);
OPENAPI_SCHEMA_TRAITS(CatLogListResponse);
OPENAPI_SCHEMA_TRAITS(ErrorResponse);

#undef OPENAPI_SCHEMA_TRAITS
