#include <string>
#include <vector>

namespace MakefileBuilder {
void build(const std::string& makefilePath, std::vector<std::string> targets,
           const size_t numJobs);
}  // namespace MakefileBuilder