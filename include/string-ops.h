#include <string>
#include <vector>

/**
 * @brief Operations that extract semantically meaningful
 * information from a string.
 *
 */
namespace StringOps {
std::string trim(const std::string& str);
std::vector<std::string> split(const std::string& str, char delimiter);
}  // namespace StringOps