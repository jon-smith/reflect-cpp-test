#include <format>
#include <iostream>

#include <rfl/json.hpp>

#include "cat_api_types.hpp"
#include "openapi_demo.hpp"

const auto senorDonGato = Cat{
    .name = "Senor Don Gato",
    .breed = "Siamese",
    .dateOfBirth = "1999-08-10",
};

int main()
{
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
