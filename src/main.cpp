#include <iostream>
#include <format>
#include <numbers>

#include <rfl/json.hpp>
#include <rfl.hpp>

struct Cat
{
  std::string breed;
  std::string name;
  double age = {};
  std::string meow = "meow";
};

const auto senorDonGato =
    Cat{.breed = "Siamese",
        .name = "Senor Don Gato",
        .age = 5.0};

int main()
{
  std::cout << std::format("👽uoıʇɔǝlɟǝɹ🪬") << std::endl;
  std::cout << rfl::json::write(senorDonGato) << std::endl;
  return 0;
}
