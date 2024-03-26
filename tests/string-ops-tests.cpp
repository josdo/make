#include <gtest/gtest.h>

#include "string-ops.h"

TEST(StringOps, trim) {
    EXPECT_EQ(StringOps::trim("   a  b c  "), "a  b c");
    EXPECT_EQ(StringOps::trim("  "), "");
}

TEST(StringOps, split) {
    EXPECT_EQ(StringOps::split("  a   b      c    ", ' '),
              std::vector<std::string>({"a", "b", "c"}));
}
