#include "variables.h"

/* Permits empty string for name or value. */
void Variables::addVariable(const std::string& name, const std::string& value,
                            size_t lineno) {
    variables[name] = value;
    variableLinenos[name] = lineno;
}

/**
 * @brief Substitute each variable in the input string with its value if
 * defined in the map, or otherwise with an empty string. Any variable found
 * inside another variable's value is substituted recursively. Preserves
 * whitespace.
 *
 * @param subValues Variable names mapped to values that can be use in
 * substitution.
 * @return bool True if success. False if failure.
 * @return std::string The substituted input if success. The error message
 * if failure.
 */
std::string Variables::expandVariables(const std::string& input,
                                       size_t lineno) {
    std::string output = "";
    std::string remainingInput = input;
    while (!remainingInput.empty()) {
        /* Go to the next variable. */
        size_t dollarPos = remainingInput.find('$');
        output += remainingInput.substr(0, dollarPos);
        if (dollarPos == std::string::npos) {
            /* The entire input has been went through. */
            break;
        }

        /* Handle $ at the end of a line. */
        remainingInput = remainingInput.substr(dollarPos + 1);
        if (remainingInput.empty()) {
            output += "$";
            break;
        }

        /* Capture variable name. */
        std::string currentName;
        if (remainingInput.starts_with('(')) {
            /* Parentheses-enclosed variable. */
            size_t endParen = remainingInput.find(')');
            if (endParen == std::string::npos) {
                /* No closing parenthesis means this is an invalid variable
                 * reference. */
                throw VariablesException(
                    "%u: *** unterminated variable reference.  Stop.", lineno);
            }
            currentName = remainingInput.substr(1, endParen - 1);
            remainingInput = remainingInput.substr(endParen + 1);
        } else {
            /* Single character variable. */
            currentName = remainingInput.substr(0, 1);
            remainingInput = remainingInput.substr(1);
        }

        /* Capture the line where this variable is defined. This is the new
         * lineno to blame for any error. If this variable has not been defined,
         * its lineno will correctly be 0. */
        size_t currentLineno = variableLinenos[currentName];

        /* Discover if this variable name has been seen before. */
        if (expandingVariables.contains(currentName)) {
            throw VariablesException(
                "%u: *** Recursive variable '%s' references itself "
                "(eventually).  Stop.",
                currentLineno, currentName.c_str());
        }

        /* Expand any variables inside this name. If the name doesn't have a
         * value, it will be an empty string and thus correctly not be expanded.
         */
        std::string currentValue = variables[currentName];

        expandingVariables.insert(currentName);
        output += expandVariables(currentValue, currentLineno);
        expandingVariables.erase(currentName);
    }

    return output;
}