#include <functional>
#include <string>
#include <vector>

/**
 * @brief An algorithm that concurrently executes tasks with a directed
 * acyclic dependency graph.
 *
 */
namespace TaskGraph {
struct Task {
    std::string task{};                     /* Name of this task. */
    std::vector<std::string> parentTasks{}; /* Tasks that this depends on. */
    std::function<bool(std::string)> runTask{}; /* False if failed. */
};

bool run(const std::vector<Task>& tasks, int maxThreads);
}  // namespace TaskGraph