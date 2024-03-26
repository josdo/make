#include <map>
#include <set>
#include <string>
#include <vector>

#include "exception.h"
#include "variables.h"
/* TODO remove LOG. */
// #define LOG

/**
 * @brief Parses a makefile so target prerequisites and recipes can be queried.
 *
 */
class MakefileParser {
   public:
    MakefileParser(std::string makefilePath);

    std::tuple<std::vector<std::string>, std::vector<size_t>> getRecipes(
        std::string target);
    std::vector<std::string> getPrereqs(std::string target);
    bool outdated(std::string target);
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
    /* Path of the makefile. */
    std::string makefilePath;

    /* The prerequisites for each target in the makefile. Targets with no
     * prerequisites are also stored so the keys represent all seen targets in
     * the makefile after parsing. */
    std::map<std::string, std::vector<std::string>> makefilePrereqs;

    /* The recipes for each target in the makefile. Only targets with
     * 1 or more recipes are stored. */
    std::map<std::string, std::vector<std::string>> makefileRecipes;

    /* Line numbers, in order, for the recipes of each target in the makefile.
     * Isomorphic to `makefileRecipes`. */
    std::map<std::string, std::vector<size_t>> makefileRecipeLinenos;

    /* Targets of the first rule in the makefile. */
    std::vector<std::string> firstTargets;

    Variables vars;

    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    bool hasLoop(std::string target, std::set<std::string>& seenTargets);
};