#include <map>
#include <string>
#include <vector>

#include "exception.h"
#include "variables.h"

/**
 * @brief Parses a makefile and provides information about any target
 * that is needed to build it.
 *
 */
class MakefileParser {
   public:
    MakefileParser(std::string makefilePath);

    std::tuple<std::vector<std::string>, std::vector<size_t>> getRecipes(
        const std::string& target);
    std::vector<std::string> getPrereqs(const std::string& target);
    bool outdated(const std::string& target);
    std::vector<std::string> getFirstTargets();

    class MakefileParserException : public PrintfException {
       public:
        MakefileParserException(const char* format, ...) : PrintfException() {
            va_list ap;
            va_start(ap, format);
            set_msg(format, ap);
        }
    };

    PRIVATE
    /* Path of the parsed makefile. */
    std::string makefilePath;

    /* The prerequisites for each target in the makefile. Targets with no
     * prerequisites are also stored, so the keys represent all parsed
     * makefile targets. */
    std::map<std::string, std::vector<std::string>> makefilePrereqs;

    /* The recipes for each target in the makefile. Only targets with
     * 1 or more recipes are stored. */
    std::map<std::string, std::vector<std::string>> makefileRecipes;

    /* Line numbers for each target's recipes. Isomorphic to `makefileRecipes`.
     */
    std::map<std::string, std::vector<size_t>> makefileRecipeLinenos;

    /* Targets of the first rule defined in the makefile. */
    std::vector<std::string> firstTargets;

    /* Storage for all variable definitions. */
    Variables makefileVars;

    bool hasCircularDependency(const std::string& target);
};