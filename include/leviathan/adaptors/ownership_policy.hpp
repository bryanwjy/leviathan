// Copyright 2025, Bryan Wong

#include "leviathan/pointer.hpp"

#include <array>
#include <compare>
#include <iterator>

namespace lev {
namespace py {
enum class ownership_policy {
    none = 0,
    strong = 1
};
} // namespace py
} // namespace lev
