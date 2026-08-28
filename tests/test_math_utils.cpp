#include <gtest/gtest.h>
#include "util/MathUtils.h"

TEST(MathUtils, NormalizeAngle) {
    EXPECT_NEAR(math::normalizeAngle(3.14), 3.14, 0.01);
    EXPECT_NEAR(math::normalizeAngle(5.0), -1.283, 0.01);
    EXPECT_NEAR(math::normalizeAngle(-5.0), 1.283, 0.01);
}

TEST(MathUtils, Clamp) {
    EXPECT_EQ(math::clamp(5.0, 0.0, 10.0), 5.0);
    EXPECT_EQ(math::clamp(-1.0, 0.0, 10.0), 0.0);
    EXPECT_EQ(math::clamp(15.0, 0.0, 10.0), 10.0);
}

TEST(MathUtils, Lerp) {
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 0.5), 5.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 1.0), 10.0);
}

TEST(PID, BasicOperation) {
    math::PID pid(1.0, 0.1, 0.05);
    double out = pid.update(10.0, 0.0, 0.1);
    EXPECT_GT(out, 0.0);
    pid.reset();
    EXPECT_DOUBLE_EQ(pid.update(0.0, 0.0, 0.1), 0.0);
}
