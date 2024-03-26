#include "makefile-builder.h"

#include <sys/wait.h>
#include <unistd.h>

#include <deque>
#include <iostream>
#include <memory>
#include <string>

#include "makefile-parser.h"
#include "task-graph.h"

namespace MakefileBuilder {

/**
 * @brief Builds the given targets using the rules defined in the makefile.
 * Returns early and outputs an error message to std::cerr if there is incorrect
 * make syntax or bash exits with an error during a build. For efficiency,
 * builds targets concurrently wherever possible up to the number of jobs
 * allowed.
 *
 */
void build(const std::string& makefilePath, std::vector<std::string> targets,
           const size_t numJobs) {
    /* Parse. */
    std::shared_ptr<MakefileParser> parser;
    try {
        parser = std::make_shared<MakefileParser>(makefilePath);
    } catch (const MakefileParser::MakefileParserException& e) {
        std::cerr << e.what() << '\n';
        return;
    }

    /* Build the first-defined rule if no targets given. */
    if (targets.empty()) {
        targets = parser->getFirstTargets();
    }

    for (const std::string& currTarget : targets) {
        /* Turn this target into a DAG of tasks. */
        std::deque<std::string> taskify = {currTarget};
        std::vector<TaskGraph::Task> tasks;
        while (!taskify.empty()) {
            TaskGraph::Task task{.task = taskify.front()};
            taskify.pop_front();

            /* Lookup the recipes. */
            std::vector<std::string> recipes;
            std::vector<size_t> recipeLinenos;
            try {
                task.parentTasks = parser->getPrereqs(task.task);
                std::tie(recipes, recipeLinenos) =
                    parser->getRecipes(task.task);
            } catch (const MakefileParser::MakefileParserException& e) {
                std::cerr << e.what() << '\n';
                return;
            }

            /* Create the recipe-running function. */
            task.runTask = [parser, currTarget, recipes, recipeLinenos,
                            makefilePath](const std::string& target) {
                /* Don't run if the target is up to date. */
                if (!parser->outdated(target)) {
                    if (currTarget == target) {
                        std::cout << "make: '" + target + "' is up to date.\n";
                    }
                    return true;
                }
                /* Run each recipe of the target. */
                for (size_t i = 0; i < recipes.size(); i++) {
                    std::string recipe = recipes.at(i);
                    size_t lineno = recipeLinenos.at(i);
                    pid_t pid = fork();
                    if (pid == -1) {
                        perror("fork failed");
                        return false;
                    } else if (pid == 0) {
                        /* Print out recipe unless preceded by `@`. */
                        if (recipe.starts_with('@')) {
                            /* Remove `@` from shell command. */
                            recipe = recipe.substr(1);
                        } else {
                            std::cout << recipe << '\n';
                        }

                        /* Run recipe in child process. */
                        const char* argv[] = {"bash", "-c", recipe.c_str(),
                                              NULL};
                        execvp(argv[0], const_cast<char* const*>(argv));

                        /* Execvp only returns if an error occurred. */
                        perror("execvp failed");
                        return false;
                    } else {
                        /* Wait before going to next recipe in the parent
                         * process. */
                        int status;
                        if (waitpid(pid, &status, 0) == -1) {
                            perror("waitpid failed");
                            return false;
                        }
                        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                            /* The child process terminated with an error. */
                            std::cerr << "make: *** [" + makefilePath + ":" +
                                             std::to_string(lineno) + ": " +
                                             target + "] Error "
                                      << WEXITSTATUS(status) << '\n';
                            return false;
                        }
                    }
                }
                return true;
            };

            tasks.push_back(task);

            /* Turn the prerequisites into tasks next. */
            taskify.insert(taskify.end(), task.parentTasks.begin(),
                           task.parentTasks.end());
        }

        /* Run the tasks to build this target. If one target fails, do not build
         * any remaining targets. */
        bool success = TaskGraph::run(tasks, numJobs);
        if (!success) {
            return;
        }
    }
}

}  // namespace MakefileBuilder