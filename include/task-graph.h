#include <functional>
#include <string>
#include <vector>

class TaskGraph {
   public:
    TaskGraph() {}

    struct Task {
        /* If a duplicate task given, the latter task will be ignored. */
        std::string task{};
        /* If a parent task does not exist, it is skipped. */
        /* If a circular dependency exists, it will be detected while running as
         * a stall. */
        std::vector<std::string> parentTasks{};
        /* True means success. False means failure. Must be thread safe. */
        std::function<bool(std::string)> runTask{};
    };

    bool run(std::vector<Task> tasks, int numThreads);
};