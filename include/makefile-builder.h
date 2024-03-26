#include <string>
#include <vector>

/**
 * @brief Performs the make operation end to end, from parsing to rule
 * execution.
 *
 */
namespace MakefileBuilder {
void build(const std::string& makefilePath, std::vector<std::string> targets,
           const size_t numJobs);
}  // namespace MakefileBuilder