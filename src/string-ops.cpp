#include "string-ops.h"

#include <sstream>

namespace StringOps {
/**
 * @brief Trim off all whitespaces and tabs that are leading and trailing.
 *
 */
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    bool isBlank = std::string::npos == first;
    if (isBlank) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

/**
 * @brief Splits the string wherever the delimiter appears and returns the
 * non-empty substrings.
 *
 */
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::istringstream iss(str);
    std::vector<std::string> result;

    for (std::string s; std::getline(iss, s, delimiter);) {
        if (!s.empty()) result.push_back(s);
    }
    return result;
}
}  // namespace StringOps