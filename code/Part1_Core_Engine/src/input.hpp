// Strict text -> number parsing and tokenising for the CLI (no crashes on bad input).
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace input {

// The WHOLE token must be a valid number inside [lo, hi]; otherwise returns nullopt.
// (So "12abc", "", "1e999" and out-of-range values are all rejected.)
std::optional<long> parseInt(const std::string& token, long lo, long hi);
std::optional<double> parseDouble(const std::string& token, double lo, double hi);

// Splits a line on whitespace: "zoom  3 " -> {"zoom", "3"}.
std::vector<std::string> split(const std::string& line);

} // namespace input
