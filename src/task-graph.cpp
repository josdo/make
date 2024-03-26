#include "task-graph.h"

#include <atomic>
#include <deque>
#include <future>
#include <map>
#include <semaphore>
#include <set>
#include <string>

namespace TaskGraph {

/**
 * @brief Run each task after its parents. Independent tasks will be run
 * concurrently.
 *
 * @return true Every task ran and returned with success.
 * @return false Some tasks could not run due to circular dependency, or a task
 * ran but returned failure.
 */
bool run(std::vector<Task> tasks, int numThreads) {
    /* SCHEDULE TASKS. */
    /* For each task, this stores the number of parent tasks that still need to
     * be run before it can be run. */
    std::map<std::string, std::atomic<int>> numUntilReady;

    /* Tasks that are ready to run and have not yet been started. */
    std::deque<std::string> ready;

    /* Each task's function that does its work. */
    std::map<std::string, std::function<bool(std::string)>> work;

    /* For each task, the other tasks that it is a parent for. */
    std::map<std::string, std::vector<std::string>> children;

    std::set<std::string> exists;
    for (const Task& task : tasks) {
        exists.insert(task.task);
    }

    for (const Task& task : tasks) {
        if (numUntilReady.contains(task.task)) {
            /* Task already seen, so ignore it. */
            continue;
        }

        /* Only consider parents with an associated task. */
        int numRunnableParents = 0;
        for (const std::string& parent : task.parentTasks) {
            if (!exists.contains(parent)) {
                continue;
            }
            numRunnableParents++;
            children[parent].push_back(task.task);
        }
        numUntilReady[task.task] = numRunnableParents;

        /* Indicate if ready to run. */
        if (numRunnableParents == 0) {
            ready.push_back(task.task);
        }

        /* Point to work. */
        work[task.task] = task.runTask;
    }

    /* RUN TASKS. */
    /* The name and result of each task that is being or has been run but not
     * yet handled . */
    std::map<std::string, std::future<bool>> taskFutures;

    /* True if any task returned failure (i.e. false) in its future. */
    bool taskFailed = false;

    /* The number of threads that have finished and whose results have not been
     * handled yet. */
    std::atomic<int> threadsFinished = 0;

    /* Acquired when launching each task and released when the task completes.
     * An acquire blocks until a thread is available to do another task. */
    std::counting_semaphore taskLimiter(numThreads);

    while (true) {
        /* There are tasks ready to run. Launch the next ready task in a new
         * thread. When no threads are available, block until a thread finishes.
         */
        while (!ready.empty()) {
            taskLimiter.acquire();
            std::string taskName = ready.front();
            ready.pop_front();

            auto runTask = work[taskName];

            taskFutures[taskName] =
                std::async(std::launch::async,
                           [taskName, runTask, &taskLimiter, &threadsFinished] {
                               bool success = runTask(taskName);
                               threadsFinished++;
                               threadsFinished.notify_one();
                               taskLimiter.release();
                               return success;
                           });
        }

        /* We can stop if nothing is ready and nothing is running. */
        if (taskFutures.empty()) {
            break;
        }

        /* There are no tasks ready to run. Block until a task finishes. The
         * compare and wait is atomic to a notify event, for why see
         * https://stackoverflow.com/a/65563565. */
        threadsFinished.wait(0);

        /* For each finished task, get its future, queue up its ready children
         * to run, then remove it. If it failed, stop running tasks. */
        std::vector<std::string> toRemove;
        for (auto& [taskName, taskFuture] : taskFutures) {
            bool finished = taskFuture.wait_for(std::chrono::seconds(0)) ==
                            std::future_status::ready;
            if (!finished) {
                continue;
            }

            threadsFinished--;
            taskFailed = !taskFuture.get();
            /* Remove tasks whose futures have already been gotten, or else a
             * subsequent get will access an invalid future state. */
            toRemove.push_back(taskName);
            if (taskFailed) {
                break;
            }

            for (const std::string& child : children[taskName]) {
                numUntilReady[child]--;
                if (numUntilReady[child] == 0) {
                    ready.push_back(child);
                }
            }
        }

        for (const std::string& taskName : toRemove) {
            taskFutures.erase(taskName);
        }

        if (taskFailed) {
            break;
        }
    }

    /* Wait for remaining tasks to finish. */
    for (auto& [taskName, taskFuture] : taskFutures) {
        taskFuture.wait();
    }

    /* Confirm no task failed. */
    if (taskFailed) {
        return false;
    }

    /* All tasks that ran succeeded. Now confirm all tasks were run. */
    bool runnedAll = true;
    for (const auto& [taskName, count] : numUntilReady) {
        if (count > 0) {
            runnedAll = false;
        }
    }
    return runnedAll;
}

}  // namespace TaskGraph