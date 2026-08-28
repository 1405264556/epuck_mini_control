#include <gtest/gtest.h>

#include "control/MultiRobotStrategyRegistry.h"

#include <QTemporaryDir>

TEST(MultiRobotStrategyRegistry, EmptyDirectoryLoadsNoStrategies) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    MultiRobotStrategyRegistry registry;
    EXPECT_EQ(registry.loadDirectory(directory.path()), 0);
    EXPECT_TRUE(registry.strategies().isEmpty());
    EXPECT_TRUE(registry.errors().isEmpty());
}

TEST(MultiRobotStrategyRegistry, MissingDirectoryIsHandled) {
    MultiRobotStrategyRegistry registry;
    EXPECT_EQ(registry.loadDirectory("Z:/path/that/does/not/exist"), 0);
    EXPECT_TRUE(registry.strategies().isEmpty());
}
