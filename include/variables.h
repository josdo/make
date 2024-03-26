#ifndef VARIABLES_H
#define VARIABLES_H

#include <map>
#include <set>
#include <string>

#include "exception.h"

class Variables {
   public:
    Variables(){};
    /* Lineno 0 is a special value since they start from 1. */
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
    std::map<std::string, std::string> variables;
    std::map<std::string, size_t> variableLinenos;

    /* Variables currently in process of being expanded in an `expandVariables`
     * call.*/
    std::set<std::string> expandingVariables;
};

#endif  // VARIABLES_H