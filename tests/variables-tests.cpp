#include <gtest/gtest.h>

#include "makefile-parser.h"
#include "variables.h"

TEST(Variables, expandVariables) {
    Variables vars;
    vars.variables = {{"A", "a"},
                      {"unterminated", "$("},
                      {"sub", "__$(A)__"},
                      {"=", "equals"},
                      {"space space", "spacespace"},
                      {"VAR5", "x$@$^$<y"},
                      {"three   space", "threespace"},
                      {"$", "$"}};
    std::map<std::string, size_t> linenos;
    std::string output =
        vars.expandVariables("+++$(A)+++$(sub)+++$(space space)  $(=)", 0);
    EXPECT_EQ(output, "+++a+++__a__+++spacespace  equals");

    try {
        output = vars.expandVariables("$(unterminated)", 0);
    } catch (const Variables::VariablesException& e) {
        EXPECT_TRUE(true);
    } catch (...) {
        EXPECT_TRUE(false);
    }

    output = vars.expandVariables("$(VAR5) ", 0);
    EXPECT_EQ(output, "xy ");

    output = vars.expandVariables("$(three   space)", 0);
    EXPECT_EQ(output, "threespace");

    output = vars.expandVariables("$$", 0);
    EXPECT_EQ(output, "$");
}

TEST(MakefileParser, substituteVariables_detectLoop) {
    Variables vars;
    vars.variables = {{"A", "$(B)"}, {"B", "$(C)"}, {"C", "$(A)"}};

    std::string output;
    try {
        output = vars.expandVariables("$(A)", 0);
    } catch (const Variables::VariablesException& e) {
        EXPECT_TRUE(true);
        std::cout << "correctly threw error (should be Recursive variable): "
                  << e.what() << '\n';
    } catch (...) {
        EXPECT_TRUE(false);
    }
}
