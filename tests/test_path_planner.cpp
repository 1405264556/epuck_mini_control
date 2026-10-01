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

TEST(PathPlanner, DoesNotCutBetweenBlockedCorners) {
    PathPlanner planner;
    planner.setGridSize(3,3);
    planner.setObstacles({{1,0}, {0,1}}, 0);
    EXPECT_TRUE(planner.findPath({0,0}, {2,2}).isEmpty());
}

TEST(PathPlanner, RejectsNegativeCoordinatesAndResetsObstacles) {
    PathPlanner planner;
    planner.setGridSize(10,10);
    EXPECT_TRUE(planner.findPath({-0.1,0}, {5,5}).isEmpty());
    planner.setObstacles({{5,5}}, 0);
    EXPECT_TRUE(planner.findPath({0,0}, {5,5}).isEmpty());
    planner.setObstacles({}, 0);
    EXPECT_FALSE(planner.findPath({0,0}, {5,5}).isEmpty());
}
