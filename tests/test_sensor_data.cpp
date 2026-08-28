#include <gtest/gtest.h>
#include <QDateTime>
#include "core/SensorData.h"

TEST(SensorData, DefaultInvalid) {
    SensorData d;
    EXPECT_FALSE(d.isValid());
}

TEST(SensorData, Clear) {
    SensorData d;
    d.timestamp = 12345;
    d.proximity[0] = 100;
    d.clear();
    EXPECT_FALSE(d.isValid());
    EXPECT_EQ(d.proximity[0], 0);
}

TEST(SensorData, ValidWhenTimestampSet) {
    SensorData d;
    d.timestamp = QDateTime::currentMSecsSinceEpoch();
    EXPECT_TRUE(d.isValid());
}
