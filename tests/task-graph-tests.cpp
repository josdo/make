#include <gtest/gtest.h>

#include "task-graph.h"

auto printTask = [](const std::string& task) {
    std::cout << task << '\n';
    return true;
};

TEST(TaskGraph, run_simple) {
    std::vector<TaskGraph::Task> tasks;
    auto failedTask = [](std::string) { return false; };

    std::cout << "****** NEW RUN ******\n";
    EXPECT_TRUE(TaskGraph::run(tasks, 1));

    std::cout << "****** NEW RUN ******\n";
    tasks = {{"fail", {}, failedTask}};
    EXPECT_FALSE(TaskGraph::run(tasks, 1));

    std::cout << "****** NEW RUN ******\n";
    tasks = {{"1", {"2"}, printTask}, {"2", {"3"}, printTask}};
    EXPECT_TRUE(TaskGraph::run(tasks, 1));

    std::cout << "****** NEW RUN ******\n";
    tasks = {{"1", {"2"}, printTask}, {"2", {"1"}, printTask}};
    EXPECT_FALSE(TaskGraph::run(tasks, 1));
}

TEST(TaskGraph, run_tree) {
    std::vector<TaskGraph::Task> tasks;

    /* Task tree. Expect 3s in any order, then 2, then 1
     *      1
     *   /     \
     *  3c     2
     *       /   \
     *      3b   3a  <- no deps
     */
    std::cout << "****** NEW RUN ******\n";
    tasks = {{"3a", {}, printTask},
             {"3b", {}, printTask},
             {"3c", {}, printTask},
             {"2", {"3a", "3b"}, printTask},
             {"1", {"3c", "2"}, printTask}};
    EXPECT_TRUE(TaskGraph::run(tasks, 1));

    std::cout << "****** NEW RUN ******\n";
    EXPECT_TRUE(TaskGraph::run(tasks, 3));

    std::cout << "****** NEW RUN ******\n";
    EXPECT_TRUE(TaskGraph::run(tasks, 10));
}

TEST(TaskGraph, run_graph1) {
    std::vector<TaskGraph::Task> tasks;

    /* Same as tree except 3b comes before 3c
     *      1
     *   /     \
     *  3c     2
     *     \  /   \
     *      3b   3a  <- no deps
     */
    std::cout << "****** NEW RUN ******\n";
    tasks = {{"3a", {}, printTask},
             {"3b", {}, printTask},
             {"3c", {"3b"}, printTask},
             {"2", {"3a", "3b"}, printTask},
             {"1", {"3c", "2"}, printTask}};
    EXPECT_TRUE(TaskGraph::run(tasks, 1));

    std::cout << "****** NEW RUN ******\n";
    EXPECT_TRUE(TaskGraph::run(tasks, 3));

    std::cout << "****** NEW RUN ******\n";
    EXPECT_TRUE(TaskGraph::run(tasks, 10));
}

TEST(TaskGraph, run_graph2) {
    std::vector<TaskGraph::Task> tasks;

    /* Expect 3, then 2s, then 1s.
     *      1a     1b
     *   /     \ /
     *  2a     2b
     *     \  /
     *      3  <- no deps
     */
    std::cout << "****** NEW RUN ******\n";
    tasks = {
        {"3", {}, printTask},      {"2a", {"3"}, printTask},
        {"2b", {"3"}, printTask},  {"1a", {"2a", "2b"}, printTask},
        {"1b", {"2b"}, printTask},
    };
    EXPECT_TRUE(TaskGraph::run(tasks, 1));

    std::cout << "****** NEW RUN ******\n";
    EXPECT_TRUE(TaskGraph::run(tasks, 3));

    std::cout << "****** NEW RUN ******\n";
    EXPECT_TRUE(TaskGraph::run(tasks, 10));
}