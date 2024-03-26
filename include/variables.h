#ifndef VARIABLES_H
#define VARIABLES_H

#include <map>
#include <set>
#include <string>

#include "exception.h"

class Variables {
   public:
    Variables(){};
    void addVariable(const std::string& name, const std::string& value,
                     size_t lineno);
    std::string expandVariables(const std::string& input, size_t lineno);

    class VariablesException : public PrintfException {
       public:
        VariablesException(const char* format, ...) : PrintfException() {
            va_list ap;
            va_start(ap, format);
            set_msg(format, ap);
        }
    };

    PRIVATE
    /* The value for each variable name added. */
    std::map<std::string, std::string> variables;

    /* The lineno where each added variable name was defined. */
    std::map<std::string, size_t> variableLinenos;

    /* Variables currently in process of being expanded in an `expandVariables`
     * call.*/
    std::set<std::string> expandingVariables;
};

#endif  // VARIABLES_H