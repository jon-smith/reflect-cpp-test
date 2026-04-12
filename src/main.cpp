#include <format>
#include <iostream>
#include <string_view>

#include <rfl/json.hpp>

#include "cat_api_types.hpp"
#include "cat_api_server.hpp"
#include "openapi_demo.hpp"

const auto senorDonGato = Cat{
    .name = "Senor Don Gato",
    .breed = "Siamese",
    .dateOfBirth = "1999-08-10",
};

int main(int argc, char **argv)
{
  if (argc > 1 && std::string_view(argv[1]) == "--serve")
  {
    std::cout << "Starting CatLog demo server on http://localhost:8080" << std::endl;
    const auto result = serveCatApi();
    if (!result)
    {
      std::cerr << result.error() << '\n';
      return 1;
    }
    return 0;
  }

  std::cout << std::format("👽uoıʇɔǝlɟǝɹ🪬") << std::endl;
  std::cout << rfl::json::write(senorDonGato) << std::endl;

  std::cout << "Building open API spec..." << std::endl;

  const auto spec = buildOpenApiSpec();
  if (!spec)
  {
    std::cerr << "failed to build OpenAPI spec: " << spec.error() << '\n';
    return 1;
  }

  rfl::json::write(spec.value(), std::cout, rfl::json::pretty);
  std::cout << '\n';
  return 0;
}
