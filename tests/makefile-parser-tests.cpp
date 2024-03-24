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

TEST(MakefileParser, identifyLineWhitespace) {
    /* test on whitespace */
    MakefileParser parser("tests/empty.mk");
    MakefileParser::LineType t;

    std::tie(t, std::ignore) = parser.identifyLine("\t", true);
    EXPECT_EQ(t, MakefileParser::NOOP);

    std::tie(t, std::ignore) = parser.identifyLine(" ", false);
    EXPECT_EQ(t, MakefileParser::NOOP);

    std::tie(t, std::ignore) = parser.identifyLine("\t", false);
    EXPECT_EQ(t, MakefileParser::NOOP);

    std::tie(t, std::ignore) = parser.identifyLine("    X = 1", true);
    EXPECT_EQ(t, MakefileParser::VARIABLE);

    std::tie(t, std::ignore) = parser.identifyLine("    X = 1", false);
    EXPECT_EQ(t, MakefileParser::VARIABLE);

    std::tie(t, std::ignore) = parser.identifyLine("\tX = 1", true);
    EXPECT_EQ(t, MakefileParser::RECIPE);

    std::tie(t, std::ignore) = parser.identifyLine("\tX = 1", false);
    EXPECT_EQ(t, MakefileParser::INVALID);
}

TEST(MakefileParser, getVariables) {
    MakefileParser parser("tests/empty.mk");
    auto substrings = parser.getVariables("$(A)filler$(B)filler$@$^$$$(");
    std::vector<std::tuple<std::string, int>> correctSubstrings{
        {"A", 1}, {"filler", 0},
        {"B", 1}, {"filler", 0},
        {"@", 1}, {"^", 1},
        {"$", 1}, {"unterminated variable reference", -1}};
    EXPECT_EQ(substrings, correctSubstrings);

    substrings = parser.getVariables("$");
    correctSubstrings = {{"$", 0}};
    EXPECT_EQ(substrings, correctSubstrings);
}

TEST(MakefileParser, substituteVariables) {
    MakefileParser parser("tests/empty.mk");
    std::map<std::string, std::string> subValues = {
        {"A", "a"},
        {"unterminated", "$("},
        {"sub", "__$(A)__"},
        {"=", "equals"},
        {"space space", "spacespace"},
        {"VAR5", "x$@$^$<y"},
        {"three   space", "threespace"},
        {"$", "$"}};
    std::set<std::string> seen;
    std::map<std::string, size_t> linenos;
    auto [output, success] = parser.substituteVariables(
        "+++$(A)+++$(sub)+++$(space space)  $(=)", 0, subValues, linenos, seen);
    EXPECT_TRUE(success);
    EXPECT_EQ(output, "+++a+++__a__+++spacespace  equals");

    seen.clear();
    std::tie(output, success) = parser.substituteVariables(
        "$(unterminated)", 0, subValues, linenos, seen);
    EXPECT_FALSE(false);

    seen.clear();
    std::tie(output, success) =
        parser.substituteVariables("$(VAR5) ", 0, subValues, linenos, seen);
    EXPECT_TRUE(success);
    EXPECT_EQ(output, "xy ");

    seen.clear();
    std::tie(output, success) = parser.substituteVariables(
        "$(three   space)", 0, subValues, linenos, seen);
    EXPECT_TRUE(success);
    EXPECT_EQ(output, "threespace");

    seen.clear();
    std::tie(output, success) =
        parser.substituteVariables("$$", 0, subValues, linenos, seen);
    EXPECT_TRUE(success);
    EXPECT_EQ(output, "$");
}

TEST(MakefileParser, substituteVariables_detectLoop) {
    MakefileParser parser("tests/empty.mk");
    std::map<std::string, std::string> subValues = {
        {"A", "$(B)"}, {"B", "$(C)"}, {"C", "$(A)"}};
    std::map<std::string, size_t> linenos;

    std::set<std::string> seen;
    auto [output, success] =
        parser.substituteVariables("$(A)", 0, subValues, linenos, seen);

    EXPECT_FALSE(success);
    std::cout << "correctly returned error (should be Recursive variable): "
              << output << '\n';
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

TEST(MakefileParser, hasLoop) {
    MakefileParser parser("tests/empty.mk");
    std::string target = "t";
    std::vector<std::string> prereqs = {"p1", "p2", "p2"};
    parser.makefilePrereqs = {
        {target, prereqs}, {"p1", {}}, {"p2", {"p3"}}, {"p3", {}}};

    std::set<std::string> visited;
    EXPECT_FALSE(parser.hasLoop(target, visited));

    parser.makefilePrereqs["p3"] = {"p2"};
    visited.clear();
    EXPECT_TRUE(parser.hasLoop(target, visited));
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