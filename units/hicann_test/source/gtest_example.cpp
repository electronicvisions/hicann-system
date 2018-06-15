#include "gtest/gtest.h"

TEST(DISABLED_Example, FailingTest1) {
	EXPECT_EQ(1,2);
	EXPECT_EQ(2,3);
	// hard fail via ASSERT_*
}

TEST(DISABLED_Example, WorkingTest2) {
}

TEST(DISABLED_Example, Test2) {
}
