#pragma once

#include <expected>
#include <string>

#include <httplib.h>

struct CatApiServerOptions
{
  std::string host = "localhost";
  int port = 8080;
};

void RegisterCatApiRoutes(httplib::Server &server);

std::expected<void, std::string> ServeCatApi(const CatApiServerOptions &options = {});
