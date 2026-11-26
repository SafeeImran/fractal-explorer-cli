#include "input.hpp"

#include <cerrno>
#include <cstdlib>
#include <sstream>

namespace input {

std::optional<long> parseInt(const std::string& token, long lo, long hi) {
    if (token.empty()) return std::nullopt;
    char* end = nullptr;
    errno = 0;
    long value = std::strtol(token.c_str(), &end, 10);
    // *end must be the terminating '\0', otherwise there was trailing junk like "12abc".
    if (errno != 0 || *end != '\0' || value < lo || value > hi) return std::nullopt;
    return value;
}

std::optional<double> parseDouble(const std::string& token, double lo, double hi) {
    if (token.empty()) return std::nullopt;
    char* end = nullptr;
    errno = 0;
    double value = std::strtod(token.c_str(), &end);
    // Written as !(v >= lo && v <= hi) so that NaN is rejected as well.
    if (errno != 0 || *end != '\0' || !(value >= lo && value <= hi)) return std::nullopt;
    return value;
}

std::vector<std::string> split(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    for (std::string t; in >> t;) out.push_back(t);
    return out;
}

} // namespace input
