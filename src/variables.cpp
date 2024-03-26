#include "variables.h"

/* Permits empty string for name or value. */
void Variables::addVariable(const std::string& name, const std::string& value,
                            size_t lineno) {
    variables[name] = value;
    variableLinenos[name] = lineno;
}

/* substituteVariables + getVariables together. */
std::string Variables::expandVariables(const std::string& input,
                                       size_t lineno) {
    // TODO
    return {};
}