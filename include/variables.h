#include <map>
#include <string>

class Variables {
   public:
    Variables();
    void addVariable(const std::string& name, const std::string& value,
                     size_t lineno);

    std::string expandVariables(const std::string& input, size_t lineno);

   private:
    std::map<std::string, std::string> variables;
    std::map<std::string, size_t> variableLinenos;
};