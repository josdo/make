#include <string>

class Variables {
   public:
    Variables();
    void addVariable(const std::string& name, const std::string& value,
                     size_t lineno);

    /* substituteVariables + getVariables together. */
    std::string expandVariables(const std::string& input, size_t lineno);

   private:
};