#include <string>
#include <vector>

class MakefileBuilder {
   public:
    MakefileBuilder(){};

    void build(const std::string& makefilePath,
               std::vector<std::string> targets, const size_t numJobs);
};