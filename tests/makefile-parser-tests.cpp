#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include <fstream>

#include "makefile-parser.h"

/* For file paths to work, please run test binary from project repo root
 * directory. */

DEFINE_string(path, "tests/comment.mk", "Path to the make file");

TEST(MakefileParser, constructor) {
    try {
        // MakefileParser("comment.mk");
        MakefileParser parser(FLAGS_path);
    } catch (const MakefileParser::MakefileParserException& e) {
        std::cerr << "error: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "caught an unknown exception\n";
    }
}

TEST(MakefileParser, getRecipes_getPrereqs) {
    MakefileParser parser("tests/empty.mk");
    std::string target = "t";
    std::vector<std::string> prereqs = {"p1", "p2", "p2"};
    std::vector<std::string> recipes = {"r1", "r2", "$@", "$<", "$^"};
    std::map<std::string, std::vector<std::string>> targetPrereqs = {
        {target, prereqs}, {"p1", {}}, {"p2", {"p3"}}, {"p3", {}}};
    std::map<std::string, std::vector<std::string>> targetRecipes = {
        {target, recipes},
    };
    std::map<std::string, std::vector<size_t>> targetRecipeLinenos = {
        {target, {1, 2, 3, 4, 5}},
    };

    EXPECT_EQ(std::get<0>(parser.getRecipes(target)),
              std::vector<std::string>({}));

    parser.makefilePrereqs = targetPrereqs;
    parser.makefileRecipes = targetRecipes;
    parser.makefileRecipeLinenos = targetRecipeLinenos;
    std::vector<std::string> parsedRecipes = {"r1", "r2", target, "p1",
                                              "p1 p2 p2"};

    EXPECT_EQ(std::get<0>(parser.getRecipes(target)), parsedRecipes);
    EXPECT_EQ(parser.getPrereqs(target), prereqs);

    /* Remove the mapping for a prereq that the target depends on. */
    parser.makefilePrereqs.erase("p2");
    bool exceptionThrown = false;
    try {
        parser.getPrereqs(target);
    } catch (const MakefileParser::MakefileParserException& e) {
        exceptionThrown = true;
        std::cout << "correctly threw error (should be No rule): " << e.what()
                  << '\n';
    }
    EXPECT_TRUE(exceptionThrown);
}

TEST(MakefileParser, trim) {
    MakefileParser parser("tests/empty.mk");
    EXPECT_EQ(parser.trim("   a  b c  "), "a  b c");
    EXPECT_EQ(parser.trim("  "), "");
}

TEST(MakefileParser, split) {
    MakefileParser parser("tests/empty.mk");
    EXPECT_EQ(parser.split("  a   b      c    ", ' '),
              std::vector<std::string>({"a", "b", "c"}));
}

TEST(MakefileParser, hasCircularDependency) {
    MakefileParser parser("tests/empty.mk");
    std::string target = "t";
    std::vector<std::string> prereqs = {"p1", "p2", "p2"};
    parser.makefilePrereqs = {
        {target, prereqs}, {"p1", {}}, {"p2", {"p3"}}, {"p3", {}}};

    EXPECT_FALSE(parser.hasCircularDependency(target));

    parser.makefilePrereqs["p3"] = {"p2"};
    EXPECT_TRUE(parser.hasCircularDependency(target));
}

TEST(MakefileParser, outdated) {
    // Create a temporary recent file in the make directory.
    std::string newfile = "tests/new.file";
    std::ofstream file(newfile);
    file.close();

    std::string oldfile = "tests/empty.mk";
    MakefileParser parser(oldfile);

    EXPECT_TRUE(parser.outdated("notpresent.file"));

    parser.makefilePrereqs = {{newfile, {oldfile}}};
    EXPECT_FALSE(parser.outdated(newfile));

    parser.makefilePrereqs = {{oldfile, {newfile}}};
    EXPECT_TRUE(parser.outdated(oldfile));

    // Delete the temporary file
    std::remove(newfile.c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    return RUN_ALL_TESTS();
}