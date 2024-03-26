#include "makefile-parser.h"

#include <sys/stat.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <queue>
#include <set>
#include <string>

#include "string-ops.h"
#include "variables.h"

/**
 * @brief Finds each target's recipes and prerequisites. Throws an error for
 *      - an unopenable file
 *      - incorrect syntax
 *      - a variable or any of its dependencies is defined in terms of itself
 *      - a variable with no name
 *      - a target that depends on itself
 *      - a rule with no targets
 *
 * Allows redefinition of variables and a target's recipes.
 *
 */
MakefileParser::MakefileParser(std::string makefilePath)
    : makefilePath(makefilePath) {
    std::ifstream makefile(makefilePath);
    if (!makefile) {
        throw MakefileParserException("make: %s No such file or directory",
                                      makefilePath.c_str());
    }

    /* Hardcode a special case variable. */
    makefileVars.addVariable("$", "$", 0);

    std::string line;
    size_t lineno = 0;

    /* The targets and line number of the rule definition we are currently
     * inside. When there are no targets, we are not in a rule definition. */
    std::vector<std::string> definedTargets;
    size_t definedLineno = 0;
    while (std::getline(makefile, line)) {
        lineno++;

        /* Remove comments. */
        size_t hashPos = line.find('#');
        if (hashPos != std::string::npos) {
            line.erase(hashPos);
        }

        /* Identify the line type. Order matters. Tabs are evaluated before
         * separators. A blank line is checked before anything else. */
        bool isRecipe = line.starts_with('\t');
        size_t equalPos = line.find('=');
        size_t colonPos = line.find(':');
        bool isVariable = equalPos < colonPos;
        bool isRule = colonPos < equalPos;
        line = StringOps::trim(line);
        bool isNoOp = line.empty();

        if (isNoOp) {
            continue;
        } else if (isRecipe) {
            if (definedTargets.empty()) {
                throw MakefileParserException(
                    "%s:%d: *** recipe commences before first target.  Stop.",
                    makefilePath.c_str(), lineno);
            } else {
                /* Assign recipe to each target. */
                for (const std::string& target : definedTargets) {
                    /* Override any recipes defined in a prior rule. */
                    if (!makefileRecipeLinenos[target].empty() &&
                        makefileRecipeLinenos[target].at(0) < definedLineno) {
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

                    makefileRecipes[target].push_back(line);
                    makefileRecipeLinenos[target].push_back(lineno);
                }
            }
        } else if (isVariable) {
            definedTargets.clear();

            /* Expand variables in the variable's name. */
            size_t equalPos = line.find('=');
            assert(equalPos != std::string::npos);
            std::string varName = line.substr(0, equalPos);
            try {
                varName = makefileVars.expandVariables(varName, lineno);
            } catch (const Variables::VariablesException& e) {
                throw MakefileParserException("%s:%s", makefilePath.c_str(),
                                              e.what());
            }
            varName = StringOps::trim(varName);

            /* Error on an empty variable name. */
            if (varName.empty()) {
                throw MakefileParserException(
                    "%s:%d: *** empty variable name.  Stop.",
                    makefilePath.c_str(), lineno);
            }

            /* Assign value to the variable name. */
            std::string varValue = StringOps::trim(line.substr(equalPos + 1));
            makefileVars.addVariable(varName, varValue, lineno);
        } else if (isRule) {
            /* Expand variables in the rule. Expansion may introduce a colon, so
             * colon-separate targets from prerequisites first. */
            size_t colonPos = line.find(':');
            assert(colonPos != std::string::npos);
            std::string targetString;
            std::string prereqString;
            try {
                targetString = makefileVars.expandVariables(
                    line.substr(0, colonPos), lineno);
                prereqString = makefileVars.expandVariables(
                    line.substr(colonPos + 1), lineno);
            } catch (const Variables::VariablesException& e) {
                throw MakefileParserException("%s:%s", makefilePath.c_str(),
                                              e.what());
            }

            definedTargets = StringOps::split(targetString, ' ');
            definedLineno = lineno;

            /* Error on an empty set of targets. */
            if (definedTargets.empty()) {
                throw MakefileParserException(
                    "%s:%d: *** missing target.  Stop.", makefilePath.c_str(),
                    lineno);
            }

            /* Assign prereqs to each target. */
            std::vector<std::string> newPrereqs =
                StringOps::split(prereqString, ' ');

            for (const std::string& target : definedTargets) {
                for (const std::string& newPrereq : newPrereqs) {
                    makefilePrereqs[target].push_back(newPrereq);
                }

                /* Do not duplicate any prereqs. */
                std::sort(makefilePrereqs[target].begin(),
                          makefilePrereqs[target].end());
                makefilePrereqs[target].erase(
                    std::unique(makefilePrereqs[target].begin(),
                                makefilePrereqs[target].end()),
                    makefilePrereqs[target].end());
            }

            /* Remember the targets of the first rule defined in the file. */
            if (firstTargets.empty()) {
                firstTargets = definedTargets;
            }
        } else {
            /* Arrive here if `=` and `:` were found to have the same position,
             * which means both were std::string::npos and thus not found. */
            throw MakefileParserException(
                "%s:%d: *** missing separator.  Stop.", makefilePath.c_str(),
                lineno);
        }
    }

    makefile.close();
}

/**
 * @brief Returns a target's recipes and recipe line numbers, expanding any
 * recipe variables first, including automatic variables. If no target exists,
 * nothing is returned. Throws an error if variable expansion fails.
 *
 */
std::tuple<std::vector<std::string>, std::vector<size_t>>
MakefileParser::getRecipes(const std::string& target) {
    /* Lookup target. */
    if (!makefileRecipes.contains(target)) {
        return {};
    }
    std::vector<std::string> savedRecipes = makefileRecipes[target];
    std::vector<size_t> recipeLinenos = makefileRecipeLinenos[target];

    /* Create automatic variables. */
    Variables autovars = makefileVars;
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
    std::vector<std::string> expandedRecipes;
    for (size_t i = 0; i < savedRecipes.size(); i++) {
        std::string expandedRecipe;
        try {
            expandedRecipe = autovars.expandVariables(savedRecipes.at(i),
                                                      recipeLinenos.at(i));
        } catch (const Variables::VariablesException& e) {
            throw MakefileParserException("%s:%s", makefilePath.c_str(),
                                          e.what());
        }
        expandedRecipes.push_back(expandedRecipe);
    }

    assert(expandedRecipes.size() == recipeLinenos.size());
    return {expandedRecipes, recipeLinenos};
}

/**
 * @brief Return a target's prerequisites. Throw error if the target is not
 * defined, the target depends on itself, or a prerequisite is not defined.
 *
 */
std::vector<std::string> MakefileParser::getPrereqs(const std::string& target) {
    /* Throw error if target not defined. */
    if (!makefilePrereqs.contains(target)) {
        throw MakefileParserException(
            "make: *** No rule to make target '%s'. Stop.", target.c_str());
    }

    /* Throw error if a prereq is not defined. */
    std::vector<std::string> prereqs = makefilePrereqs[target];
    for (const std::string& prereq : prereqs) {
        if (!makefilePrereqs.contains(prereq)) {
            throw MakefileParserException(
                "make: *** No rule to make target '%s', needed by '%s'. Stop.",
                prereq.c_str(), target.c_str());
        }
    }

    /* Throw error if target depends on itself. */
    if (hasCircularDependency(target)) {
        throw MakefileParserException("Circular dependency for target %s",
                                      target.c_str());
    }

    return prereqs;
}

/**
 * @brief Returns true if the target is outdated by satisfying any of the
 * following criteria:
 * 1. No file corresponds to the target.
 * 2. No file corresponds to a prerequisite of the target.
 * 3. A file corresponding to a prerequisite has been last-modified later than
 *    the target.
 * 4. There's an error getting a file status.
 *
 */
bool MakefileParser::outdated(const std::string& target) {
    /* Lookup target file. */
    if (!std::filesystem::exists(target)) {
        return true;
    }

    /* Lookup target file's modified time */
    struct stat targetStat;
    if (stat(target.c_str(), &targetStat) != 0) {
        return true;
    }
    timespec targetModTime = targetStat.st_mtim;

    for (const std::string& prereq : makefilePrereqs[target]) {
        /* Lookup prereq file. */
        if (!std::filesystem::exists(prereq)) {
            return true;
        }

        /* Compare prereq file's last modified time to the target file's. */
        struct stat prereqStat;
        if (stat(prereq.c_str(), &prereqStat) != 0) {
            return true;
        }
        timespec prereqModTime = prereqStat.st_mtim;

        bool prereqModifiedLater =
            (prereqModTime.tv_sec > targetModTime.tv_sec) ||
            (prereqModTime.tv_sec == targetModTime.tv_sec &&
             prereqModTime.tv_nsec > targetModTime.tv_nsec);
        if (prereqModifiedLater) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Return the targets of the first rule defined in the makefile. Empty if
 * no rules are defined.
 */
std::vector<std::string> MakefileParser::getFirstTargets() {
    return firstTargets;
}

/**
 * @brief Returns true if the target or any of its dependencies depends on
 * itself.
 *
 */
bool MakefileParser::hasCircularDependency(const std::string& target) {
    std::set<std::string> visitedTargets;
    std::queue<std::string> queue;

    queue.push(target);

    while (!queue.empty()) {
        std::string currentTarget = queue.front();
        queue.pop();
        visitedTargets.insert(currentTarget);

        for (const std::string& visitor : makefilePrereqs[currentTarget]) {
            /* If this prereq was already visited, it is a circular dependency.
             */
            if (visitedTargets.contains(visitor)) {
                return true;
            }

            /* Visit this target's prereqs next. */
            queue.push(visitor);
        }
    }

    return false;
}
