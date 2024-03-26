#include "string-ops.h"

#include <sstream>

namespace StringOps {
/**
 * @brief Trim the leading and trailing whitespace and tabspace off, preserving
 * any whitespace in between.
 *
 */
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) {
        /* Entire string is whitespace. */
        return "";
    }
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

/**
 * @brief Return substrings separated by any number of consecutive
 * delimiters.
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