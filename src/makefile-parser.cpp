#include "makefile-parser.h"

#include <sys/stat.h>

#include <algorithm>
#include <cassert>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <set>
#include <string>

#include "variables.h"

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
        throw MakefileParserException("make: %s No such file or directory",
                                      makefilePath.c_str());
    }

    /* This is our one default variable mapping. */
    vars.addVariable("$", "$", 0);

    std::string line;
    size_t lineno = 0;
    /* The targets of the rule that is currently in scope, and if that's
     * nonempty, the line number the rule was defined at. */
    /* TODO: group all target info together. */
    std::vector<std::string> activeTargets;
    size_t activeLineno = 0;
    while (std::getline(makefile, line)) {
        lineno++;

        /* Remove comments. */
        size_t hashPos = line.find('#');
        if (hashPos != std::string::npos) {
            line.erase(hashPos);
        }

        /* Identify the line type. Order matters. Tabbed whitespace should be
         * ignored. Tabbed nonwhitespace may contain a variable or rule
         * separator, but will be treated as a recipe. Thus, evaluation is first
         * no-op, then recipe, then variable or rule. */
        bool isRecipe = line.starts_with('\t');
        size_t equalPos = line.find('=');
        size_t colonPos = line.find(':');
        bool isVariable = equalPos < colonPos;
        bool isRule = colonPos < equalPos;
        line = trim(line);
        bool isNoOp = line.empty();

        if (isNoOp) {
            continue;
        } else if (isRecipe) {
            if (activeTargets.empty()) {
                throw MakefileParserException(
                    "%s:%d: *** recipe commences before first target.  Stop.",
                    makefilePath.c_str(), lineno);
            } else {
                /* Assign each target this recipe. */
                std::string recipe = line;
                for (const std::string& target : activeTargets) {
                    /* Override any recipes defined in a prior rule. */
                    if (!makefileRecipeLinenos[target].empty() &&
                        makefileRecipeLinenos[target].at(0) < activeLineno) {
                        std::cerr << makefilePath << ":"
                                  << std::to_string(lineno) << ":"
                                  << " warning: overriding recipe for target '"
                                  << target << "'" << '\n';
                        std::cerr
                            << makefilePath << ":"
                            << std::to_string(
                                   makefileRecipeLinenos[target].at(0))
                            << ":"
                            << " warning: ignoring old recipe for target '"
                            << target << "'" << '\n';
                        makefileRecipes.clear();
                        makefileRecipeLinenos.clear();
                    }

                    makefileRecipes[target].push_back(recipe);
                    makefileRecipeLinenos[target].push_back(lineno);
                }
            }
        } else if (isVariable) {
            /* Indicate no longer in a rule. */
            activeTargets.clear();

            /* Resolve variable name. Strip off whitespace for consistency. */
            size_t equalPos = line.find('=');
            assert(equalPos != std::string::npos);
            std::string varName = line.substr(0, equalPos);
            try {
                varName = vars.expandVariables(varName, lineno);
            } catch (const Variables::VariablesException& e) {
                throw MakefileParserException("%s:%s", makefilePath.c_str(),
                                              e.what());
            }
            varName = trim(varName);

            /* Disallow an empty variable name. */
            if (varName.empty()) {
                throw MakefileParserException(
                    "%s:%d: *** empty variable name.  Stop.",
                    makefilePath.c_str(), lineno);
            }

            /* Map variable value to the resolved name. */
            std::string varValue = trim(line.substr(equalPos + 1));
            vars.addVariable(varName, varValue, lineno);
        } else if (isRule) {
            /* Resolve rule definition. Resolve targets and prereqs separately
             * since cannot guarantee where the colon will end up after
             * resolution or whether a target name will contain a colon. */
            size_t colonPos = line.find(':');
            assert(colonPos != std::string::npos);
            std::string targetString;
            std::string prereqString;
            try {
                targetString =
                    vars.expandVariables(line.substr(0, colonPos), lineno);
                prereqString =
                    vars.expandVariables(line.substr(colonPos + 1), lineno);
            } catch (const Variables::VariablesException& e) {
                throw MakefileParserException("%s:%s", makefilePath.c_str(),
                                              e.what());
            }

            activeTargets = split(targetString, ' ');
            activeLineno = lineno;

            /* Disallow an empty set of targets. */
            if (activeTargets.empty()) {
                throw MakefileParserException(
                    "%s:%d: *** missing target.  Stop.", makefilePath.c_str(),
                    lineno);
            }

            /* Map target to all its prereqs (even empty prereqs) and store as
             * the new rule context. */
            std::vector<std::string> newPrereqs = split(prereqString, ' ');

            for (const std::string& target : activeTargets) {
                for (const std::string& newPrereq : newPrereqs) {
                    makefilePrereqs[target].push_back(newPrereq);
                }

                /* Remove any duplicate prereqs. */
                std::sort(makefilePrereqs[target].begin(),
                          makefilePrereqs[target].end());
                makefilePrereqs[target].erase(
                    std::unique(makefilePrereqs[target].begin(),
                                makefilePrereqs[target].end()),
                    makefilePrereqs[target].end());
            }

            /* If this was the first rule seen, remember its targets. */
            if (firstTargets.empty()) {
                firstTargets = activeTargets;
            }
        } else {
            /* If equal and colon in the same position, they must both be npos
             * and thus not present. */
            throw MakefileParserException(
                "%s:%d: *** missing separator.  Stop.", makefilePath.c_str(),
                lineno);
        }
    }

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
    Variables autovars = vars;
    autovars.addVariable("@", target, 0);
    std::vector<std::string> prereqs = makefilePrereqs[target];
    if (!prereqs.empty()) {
        autovars.addVariable("<", prereqs.front(), 0);
        autovars.addVariable(
            "^",
            std::accumulate(std::next(prereqs.begin()), prereqs.end(),
                            prereqs[0],
                            [](const std::string& a, const std::string& b) {
                                return a + " " + b;
                            }),
            0);
    }

    /* Expand variables in each recipe. */
    for (size_t i = 0; i < originalRecipes.size(); i++) {
        std::string subbedRecipe;
        try {
            subbedRecipe =
                autovars.expandVariables(originalRecipes.at(i), linenos.at(i));
        } catch (const Variables::VariablesException& e) {
            throw MakefileParserException("%s:%s", makefilePath.c_str(),
                                          e.what());
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
            "make: *** No rule to make target '%s'. Stop.", target.c_str());
    }
    std::vector<std::string> prereqs = makefilePrereqs[target];

    /* Throw error if prereq not defined by a rule. */
    for (const std::string& prereq : prereqs) {
        if (makefilePrereqs.find(prereq) == makefilePrereqs.end()) {
            throw MakefileParserException(
                "make: *** No rule to make target '%s', needed by '%s'. Stop.",
                prereq.c_str(), target.c_str());
        }
    }

    /* Throw error if any circular dependency. */
    if (hasCircularDependency(target)) {
        throw MakefileParserException("Circular dependency for target %s",
                                      target.c_str());
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
 * $<space> will return as "" and $<EOL> will return as "$".
 *
 * @return std::vector<std::tuple<std::string, bool>> Each element is a
 * substring and 0 if not a variable, 1 if a variable, or -1 if an
 * unterminated variable reference. If -1, the substring is the error
 * message.
 */

/**
 * @brief Trim the leading and trailing whitespace and tabspace off, preserving
 * any whitespace in between.
 *
 */
std::string MakefileParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) {
        /* Entire string is whitespace. */
        return "";
    }
    size_t last = str.find_last_not_of(" \t");
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

bool MakefileParser::hasCircularDependency(std::string target) {
    std::set<std::string> visitedTargets;
    std::queue<std::string> queue;

    queue.push(target);

    while (!queue.empty()) {
        std::string currentTarget = queue.front();
        queue.pop();

        /* Indicate this target is visited. */
        visitedTargets.insert(currentTarget);

        /* Lookup its prereqs. If a target does not exist, it is treated like a
         * target with no prereqs. */
        std::vector<std::string> prereqs = {};
        if (makefilePrereqs.find(currentTarget) != makefilePrereqs.end()) {
            prereqs = makefilePrereqs[currentTarget];
        }

        for (const std::string& visitor : prereqs) {
            /* See if we visited this prereq as a target already. If so, this is
             * a loop. */
            if (visitedTargets.find(visitor) != visitedTargets.end()) {
                return true;
            }

            /* Add prereqs of this prereq to the queue. */
            queue.push(visitor);
        }
    }

    return false;
}
