#include <gtest/gtest.h>
#include "control/PathPlanner.h"

TEST(PathPlanner, DefaultGridSize) {
    PathPlanner planner;
    EXPECT_EQ(planner.occupancyGrid().size(), 0);
}

TEST(PathPlanner, FindPath) {
    PathPlanner planner;
    auto path = planner.findPath({0, 0}, {10, 10});
    EXPECT_FALSE(path.isEmpty());
    EXPECT_EQ(path.first().x, 0);
    EXPECT_EQ(path.first().y, 0);
    EXPECT_EQ(path.last().x, 10);
    EXPECT_EQ(path.last().y, 10);
}
