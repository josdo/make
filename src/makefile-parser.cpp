#include "makefile-parser.h"

#include <sys/stat.h>

#include <algorithm>
#include <cassert>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>

/**
 * @brief Parses the makefile to find the recipes and prerequisites for every
 * target. Throws an error for
 *      - unopenable file
 *      - incorrect syntax
 *      - variable "loop" (variable defined by its own name)
 *      - rule "loop" (target is a prereq for one of its prereqs)
 *      - defining two different sets of recipes for the same target
 *
 */
MakefileParser::MakefileParser(std::string makefilePath)
    : makefilePath(makefilePath) {
    std::ifstream makefile(makefilePath);
    if (!makefile) {
        throw MakefileParserException(
            {"make: " + makefilePath + " No such file or directory"});
    }

    /* This is our one default variable mapping. */
    makefileVariableValues = {{"$", "$"}};

    std::string line;
    LineType lineType = NOOP;
    size_t lineno = 0;
    std::vector<std::string> ruleTargets;
    bool isFirstRecipe =
        false;  // True if this is the first recipe for a given ruleTarget
    while (std::getline(makefile, line)) {
        lineno++;
        std::tie(lineType, line) = identifyLine(line, !ruleTargets.empty());

        if (lineType == VARIABLE) {
            /* Not in a rule any longer. */
            ruleTargets.clear();
            isFirstRecipe = false;

            /* Resolve variable name. Strip off whitespace for consistency. */
            size_t equalPos = line.find('=');
            assert(equalPos != std::string::npos);
            std::string varName = line.substr(0, equalPos);

            bool success;
            std::set<std::string> seenVariables = {};
            std::tie(varName, success) =
                substituteVariables(varName, lineno, makefileVariableValues,
                                    makefileVariableLinenos, seenVariables);
            if (!success) {
                throw MakefileParserException({varName});
            }
            varName = trim(varName);

            /* Disallow empty variable name. */
            if (varName.empty()) {
                throw MakefileParserException(
                    {makefilePath + ":" + std::to_string(lineno) + ": *** " +
                     "empty variable name" + ".  Stop."});
            }

            /* Map variable value to the resolved name. */
            std::string varValue = trim(line.substr(equalPos + 1));
            makefileVariableValues[varName] = varValue;
            makefileVariableLinenos[varName] = lineno;

        } else if (lineType == RULE) {
            /* Clear old rule. Indicate next recipe will be the first of this
             * rule. */
            ruleTargets.clear();
            isFirstRecipe = true;

            /* Resolve rule definition. Resolve targets and prereqs separately
             * since cannot guarantee where the colon will end up after
             * resolution or whether a target name will contain a colon. */
            size_t colonPos = line.find(':');
            assert(colonPos != std::string::npos);
            std::string targetString;
            std::string prereqString;
            bool targetSuccess = false;
            bool prereqSuccess = false;
            std::set<std::string> seenVariables = {};
            std::tie(targetString, targetSuccess) = substituteVariables(
                line.substr(0, colonPos), lineno, makefileVariableValues,
                makefileVariableLinenos, seenVariables);
            if (!targetSuccess) {
                throw MakefileParserException({targetString});
            }

            seenVariables.clear();
            std::tie(prereqString, prereqSuccess) = substituteVariables(
                line.substr(colonPos + 1), lineno, makefileVariableValues,
                makefileVariableLinenos, seenVariables);
            if (!prereqSuccess) {
                throw MakefileParserException({prereqString});
            }

            /* Disallow no targets. */
            ruleTargets = split(targetString, ' ');
            if (ruleTargets.empty()) {
                throw MakefileParserException(
                    {makefilePath + ":" + std::to_string(lineno) + ": *** " +
                     "missing target" + ".  Stop."});
            }

            /* Map target to all its prereqs (even empty prereqs) and store as
             * the new rule context. */
            std::vector<std::string> newPrereqs = split(prereqString, ' ');

            for (const std::string& target : ruleTargets) {
                auto keyValue = makefilePrereqs.find(target);
                if (keyValue == makefilePrereqs.end()) {
                    /* Target doesn't have prereqs yet. Create a new
                     * mapping. */
                    makefilePrereqs[target] = newPrereqs;
                } else {
                    /* Target has prereqs, so append to it those not yet
                     * included. TODO: write unit test for duplicates. */
                    std::vector<std::string> existingPrereqs = keyValue->second;
                    for (const std::string& newPrereq : newPrereqs) {
                        if (std::find(existingPrereqs.begin(),
                                      existingPrereqs.end(),
                                      newPrereq) == existingPrereqs.end()) {
                            existingPrereqs.push_back(newPrereq);
                        }
                    }
                }
            }

            /* If this was the first rule seen, remember its targets. */
            if (firstTargets.empty()) {
                firstTargets = ruleTargets;
            }

        } else if (lineType == RECIPE) {
            /* Assign each target this recipe. */
            std::string recipe = trim(line);
            assert(!recipe.empty());
            for (const std::string& target : ruleTargets) {
                auto keyValue = makefileRecipes.find(target);

                if (keyValue != makefileRecipes.end()) {
                    /* There already exists at least 1 recipe for this target.
                     */
                    if (isFirstRecipe) {
                        /* Before this rule defined its first recipe for this
                         * target, another rule already assigned recipes to this
                         * target. Clear existing recipes. */
                        std::cerr << makefilePath << ":"
                                  << std::to_string(lineno) << ":"
                                  << " warning: overriding recipe for target '"
                                  << target << "'" << '\n';
                        size_t oldFirstLineno =
                            makefileRecipeLinenos[target].at(0);
                        std::cerr
                            << makefilePath << ":"
                            << std::to_string(oldFirstLineno) << ":"
                            << " warning: ignoring old recipe for target '"
                            << target << "'" << '\n';
                        makefileRecipes.clear();
                        makefileRecipeLinenos.clear();
                    }
                    /* A new recipe for this target is ready to be added. */
                    makefileRecipes[target].push_back(recipe);
                    makefileRecipeLinenos[target].push_back(lineno);
                } else {
                    /* Target has zero recipes right now. */
                    makefileRecipes[target] = {recipe};
                    makefileRecipeLinenos[target].push_back(lineno);
                }
            }

            /* If this was the first recipe of this rule, indicate that recipes
             * seen next are not the first one. */
            if (isFirstRecipe) {
                isFirstRecipe = false;
            }
        } else if (lineType == INVALID) {
            throw MakefileParserException({makefilePath + ":" +
                                           std::to_string(lineno) + ": *** " +
                                           line + ".  Stop."});
        }

#ifdef LOG
        /* Print identified line. */
        std::string lineTypeName = LineTypeNames[lineType];
        int width = 10;
        if (lineTypeName != "NOOP") {
            lineTypeName = "\033[1m" + lineTypeName + "\033[0m";
            // 4 characters for "\033[1m" and 4 characters for "\033[0m"
            width += 8;
        }
        std::cout << std::setw(3) << std::right << lineno << "|"
                  << std::setw(width) << std::right << lineTypeName << "|"
                  << line << '\n';
#endif
    }

#ifdef LOG
    /* Print variable mapping. */
    // std::cout << "VARIABLES" << '\n';
    for (const auto& [name, value] : variableValues) {
        std::cout << std::setw(20) << std::left << "{" + name + "}"
                  << " -> "
                  << "{" << value << "}" << '\n';
    }

    /* Print prereq mapping. */
    // std::cout << "PREREQS" << '\n';
    for (const auto& [target, prereqs] : targetPrereqs) {
        std::cout << std::setw(20) << std::left << "{" + target + "}"
                  << " -> "
                  << "{";
        for (const auto& prereq : prereqs) {
            std::cout << "\n    "
                      << "{" + prereq + "}";
        }
        std::cout << "\n}\n";
    }

    /* Print recipe mapping. */
    // std::cout << "RECIPES" << '\n';
    for (const auto& [target, recipes] : targetRecipes) {
        std::cout << std::setw(20) << std::left << "{" + target + "}"
                  << " -> "
                  << "{";
        for (const auto& recipe : recipes) {
            std::cout << "\n    "
                      << "%" + recipe + "%";
        }
        std::cout << "\n}\n";
    }
#endif

    makefile.close();
}

// Return the recipes for a target and the line numbers they were defined at.
// Return nothing if target does not exist or has no recipes. Throws error if
// recipe invalid.
std::tuple<std::vector<std::string>, std::vector<size_t>>
MakefileParser::getRecipes(std::string target) {
    /* Lookup recipes. */
    std::vector<std::string> subbedRecipes;
    auto keyValue = makefileRecipes.find(target);
    if (keyValue == makefileRecipes.end()) {
        return {};
    }
    std::vector<std::string> originalRecipes = keyValue->second;
    std::vector<size_t> linenos = makefileRecipeLinenos[target];

    /* Introduce automatic variables to substitution map. */
    std::map<std::string, std::string> autovariableValues =
        makefileVariableValues;
    autovariableValues["@"] = target;
    std::vector<std::string> prereqs = makefilePrereqs[target];
    if (!prereqs.empty()) {
        autovariableValues["<"] = prereqs.front();
        for (const std::string& prereq : prereqs) {
            if (!autovariableValues["^"].empty()) {
                autovariableValues["^"] += " ";
            }
            autovariableValues["^"] += prereq;
        }
    }

    /* Substitute recipes with variables. */
    for (size_t i = 0; i < originalRecipes.size(); i++) {
        std::set<std::string> seen;
        std::string subbedRecipe;
        bool success = false;
        /* TODO: Information leakage. There are no line numbers for autovariable
         * linenos, but there's also no variables nested in their variable
         * value, so their linenos will not be used and this is safe. */
        std::tie(subbedRecipe, success) = substituteVariables(
            originalRecipes.at(i), linenos.at(i), autovariableValues,
            makefileVariableLinenos, seen);
        if (!success) {
            throw MakefileParserException({subbedRecipe});
        }
        subbedRecipes.push_back(subbedRecipe);
    }

    assert(subbedRecipes.size() == linenos.size());
    return {subbedRecipes, linenos};
}

// Returns the prerequisites for a target. Throws error if target not found,
// a circular dependency exists, or a rule not found for a prerequisite.
std::vector<std::string> MakefileParser::getPrereqs(std::string target) {
    /* Lookup prereqs. */
    auto it = makefilePrereqs.find(target);
    if (it == makefilePrereqs.end()) {
        throw MakefileParserException(
            {"make: *** No rule to make target '" + target + "'. Stop."});
    }
    std::vector<std::string> prereqs = makefilePrereqs[target];

    /* Throw error if prereq not defined by a rule. */
    for (const std::string& prereq : prereqs) {
        if (makefilePrereqs.find(prereq) == makefilePrereqs.end()) {
            throw MakefileParserException(
                {"make: *** No rule to make target '" + prereq +
                 "', needed by '" + target + "'. Stop."});
        }
    }

    /* Throw error if any circular dependency. */
    std::set<std::string> visitedTargets;
    if (hasLoop(target, visitedTargets)) {
        throw MakefileParserException(
            {"Circular dependency for target " + target});
    }

    return prereqs;
}

/**
 * @brief Returns true if the target satisfies any of the following
 * conditions:
 * 1. No file exists named target.
 * 2. No files exists named a prerequisite of the target.
 * 3. A prerequisite file has a last-modified time later than that of the
 *    target.
 * 4. Error getting the file status.
 *
 */
bool MakefileParser::outdated(std::string target) {
    if (!std::filesystem::exists(target)) {
        /* Found a file named target. */
        return true;
    }

    struct stat targetStat;
    if (stat(target.c_str(), &targetStat) != 0) {
        /* Could not get the target file's mod time. */
        return true;
    }
    timespec targetModTime = targetStat.st_mtim;

    for (const std::string& prereq : makefilePrereqs[target]) {
        if (!std::filesystem::exists(prereq)) {
            /* Found a file named prereq. */
            return true;
        }

        struct stat prereqStat;
        if (stat(prereq.c_str(), &prereqStat) != 0) {
            /* Could not get the prereq file's mod time. */
            return true;
        }
        timespec prereqModTime = prereqStat.st_mtim;

        bool prereqNewer = (prereqModTime.tv_sec > targetModTime.tv_sec) ||
                           (prereqModTime.tv_sec == targetModTime.tv_sec &&
                            prereqModTime.tv_nsec > targetModTime.tv_nsec);
        if (prereqNewer) {
            /* Prereq file modified more recently than target's file. */
            return true;
        }
    }

    return false;
}

/**
 * @brief Return the targets of the first rule in the makefile. Return an empty
 * vector if the makefile has no rules.
 *
 */
std::vector<std::string> MakefileParser::getFirstTargets() {
    return firstTargets;
}

/**
 * @brief Splits the string by $() or $. Returns each substring and if it
 * was a variable. Does not handle nesting, e.g. $($()). Preserves
 * whitespace.
 * TODO: rename method, it does formatting too.
 *
 * $<space> or $<EOL> will return as a variable with an empty substring.
 *
 * @return std::vector<std::tuple<std::string, bool>> Each element is a
 * substring and 0 if not a variable, 1 if a variable, or -1 if an
 * unterminated variable reference. If -1, the substring is the error
 * message.
 */
std::vector<std::tuple<std::string, int>> MakefileParser::getVariables(
    std::string input) {
    std::vector<std::tuple<std::string, int>> substrings;
    std::string remainder = input;
    while (!remainder.empty()) {
        /* Find start of next variable. The substring before it is not a
         * variable. */
        size_t dollarPos = remainder.find('$');
        if (dollarPos == std::string::npos) {
            substrings.push_back({remainder, 0});
            remainder = "";
            continue;
        }
        if (dollarPos > 0) {
            substrings.push_back({remainder.substr(0, dollarPos), 0});
        }
        remainder = remainder.substr(dollarPos + 1);

        /* Find the end of the variable. */
        if (remainder.starts_with('(')) {
            /* Parentheses-enclosed variable. */
            size_t endParen = remainder.find(')');
            if (endParen == std::string::npos) {
                /* No closing parenthesis means this is an invalid variable
                 * reference. */
                substrings.push_back({"unterminated variable reference", -1});
                remainder = "";
                continue;
            } else {
                /* Do not include the parentheses in the returned
                 * substring. */
                substrings.push_back({remainder.substr(1, endParen - 1), 1});
                remainder = remainder.substr(endParen + 1);
            }
        } else {
            /* Single character variable. */
            bool isSpace =
                remainder.starts_with(' ') || remainder.starts_with('\t');
            bool isEOL = remainder.empty();
            if (isSpace) {
                substrings.push_back({"", 1});
            } else if (isEOL) {
                /* TODO: is this a variable or not? */
                substrings.push_back({"$", 0});
            } else {
                substrings.push_back({remainder.substr(0, 1), 1});
                remainder = remainder.substr(1);
            }
        }
    }
    return substrings;
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
std::tuple<std::string, bool> MakefileParser::substituteVariables(
    std::string input, size_t inputLineno,
    std::map<std::string, std::string>& subValues,
    std::map<std::string, size_t>& variableLinenos,
    std::set<std::string>& seenVariables) {
    std::string output = "";

    for (auto [substring, status] : getVariables(input)) {
        if (status == -1) {
            /* Failure in getVariables. There is no recursion in getVariables,
             * so our current input is the cause of the problem. Thus report the
             * lineno of this input. */
            std::string errorMessage = makefilePath + ":" +
                                       std::to_string(inputLineno) + ": *** " +
                                       substring + ".  Stop.";
            return {errorMessage, false};
        } else if (status == 1) {
            /* If this substring isn't a variable, it evaluates to an empty
             * string, so it can be skipped without modifying the output. */
            if (subValues.find(substring) == subValues.end()) {
                continue;
            }

            /* This substring is a variable, so it will require recursive
             * substitution. If an error happens, it is because the line where
             * the variable definition caused it, so our new input line is where
             * the variable is defined. */
            size_t newInputLineno = variableLinenos[substring];

            /* If this variable has been seen already, this is a loop.
             * Otherwise, add self to seen chain. */
            if (seenVariables.find(substring) != seenVariables.end()) {
                std::string errorMessage =
                    makefilePath + ":" + std::to_string(newInputLineno) +
                    ": *** Recursive variable '" + substring +
                    "' references itself (eventually).  Stop.";
                return {errorMessage, false};
            }
            seenVariables.insert(substring);

            /* Variable that requires more substitution. If variable not in
             * map, treat the variable as an empty string in the output. */
            auto it = subValues.find(substring);
            if (it != subValues.end()) {
                auto [subbedSubstring, subSuccess] = substituteVariables(
                    subValues[substring], newInputLineno, subValues,
                    variableLinenos, seenVariables);

                if (!subSuccess) {
                    /* Error during substitution. Preserve the error message
                     * since it already contains lineno info closest to the
                     * issue. */
                    return {subbedSubstring, false};
                } else {
                    /* Successful substitution. */
                    output += subbedSubstring;
                }
            }

            /* Once substitution for this variable finishes, it goes out of
             * scope and is not on the seen chain. */
            seenVariables.erase(substring);
        } else {
            /* Non-variable, or another status. */
            output += substring;
        }
    }
    return {output, true};
}

/**
 * @brief Returns the semantic type of a line and a "clean" version of it.
 *
 * @return MakefileParser::LineType
 * @return std::string If valid, this is the same line but without comments
 * or leading whitespace or tabspace. If invalid, this is the error message.
 */
std::tuple<MakefileParser::LineType, std::string> MakefileParser::identifyLine(
    std::string line, bool inRule) {
    bool startsWithTab = line.starts_with('\t');

    /* Remove leading whitespace or tabs. TODO: use trim()? */
    size_t firstNonWhitespacePos = line.find_first_not_of(" \t");
    if (firstNonWhitespacePos != std::string::npos) {
        line.erase(0, firstNonWhitespacePos);
    } else {
        line.erase();
    }

    /* Look for recipe if tabbed. Ignore tabbed blankspace. */
    if (line.empty()) return {NOOP, line};
    if (startsWithTab) {
        if (inRule) {
            return {RECIPE, line};
        } else {
            return {INVALID, "recipe commences before first target"};
        }
    }

    /* Remove anything from # onwards first. Look for a no-op. */
    size_t hashPos = line.find('#');
    if (hashPos != std::string::npos) {
        line.erase(hashPos);
    }
    if (line.empty()) return {NOOP, line};

    /* Look for a variable or rule.*/
    size_t equalPos = line.find('=');
    size_t colonPos = line.find(':');
    if ((equalPos != std::string::npos) && (colonPos != std::string::npos)) {
        assert(equalPos != colonPos);
        if (equalPos < colonPos) {
            return {VARIABLE, line};
        }
        return {RULE, line};
    } else if (equalPos != std::string::npos) {
        return {VARIABLE, line};
    } else if (colonPos != std::string::npos) {
        return {RULE, line};
    }

    /* Found unknown. */
    return {INVALID, "missing separator"};
}

/**
 * @brief Trim the leading and trailing whitespace off, preserving any
 * whitespace in between.
 *
 */
std::string MakefileParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) {
        /* Entire string is whitespace. */
        return "";
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

/**
 * @brief Return substrings separated by any number of consecutive
 * delimiters.
 *
 */
std::vector<std::string> MakefileParser::split(const std::string& str,
                                               char delimiter) {
    std::istringstream iss(str);
    std::vector<std::string> result;

    for (std::string s; std::getline(iss, s, delimiter);) {
        if (!s.empty()) result.push_back(s);
    }
    return result;
}

bool MakefileParser::hasLoop(std::string target,
                             std::set<std::string>& visitedTargets) {
    /* Indicate this target is visited. */
    visitedTargets.insert(target);

    /* Lookup its prereqs. If a target does not exist, it is treated like a
     * target with no prereqs. */
    std::vector<std::string> prereqs = {};
    if (makefilePrereqs.find(target) != makefilePrereqs.end()) {
        prereqs = makefilePrereqs[target];
    }

    for (const std::string& visitor : prereqs) {
        /* See if we visited this prereq as a target already. If so, this is
         * a loop. */
        if (visitedTargets.find(visitor) != visitedTargets.end()) {
            return true;
        }

        /* Visit prereqs of this prereq. */
        if (hasLoop(visitor, visitedTargets)) {
            return true;
        }
    }

    /* Forget this target was visited. */
    visitedTargets.erase(target);

    return false;
}
