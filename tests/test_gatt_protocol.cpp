#include <gtest/gtest.h>
#include "comm/EpuckGATTProtocol.h"

TEST(GATTProtocol, EncodeMotorCommand) {
    QByteArray cmd = EpuckGATTProtocol::encodeMotorCommand(0.5, -0.3);
    EXPECT_EQ(cmd.size(), 4);
    // int16 LE encoding: 0.5*1000=500, -0.3*1000=-300
    EXPECT_EQ(static_cast<uint8_t>(cmd[0]), 500 & 0xFF);     // left low
    EXPECT_EQ(static_cast<uint8_t>(cmd[1]), (500 >> 8) & 0xFF);
}

TEST(GATTProtocol, EncodeLEDCommand) {
    QByteArray cmd = EpuckGATTProtocol::encodeLEDCommand(2, 255, 128, 64);
    EXPECT_EQ(cmd.size(), 5);
    EXPECT_EQ(static_cast<uint8_t>(cmd[0]), 2);
    EXPECT_EQ(static_cast<uint8_t>(cmd[1]), 255);
    EXPECT_EQ(static_cast<uint8_t>(cmd[2]), 128);
    EXPECT_EQ(static_cast<uint8_t>(cmd[3]), 64);
}

TEST(GATTProtocol, IsEpuckMiniName) {
    EXPECT_TRUE(EpuckGATTProtocol::isEpuckMiniName("e-puck-mini-0012"));
    EXPECT_TRUE(EpuckGATTProtocol::isEpuckMiniName("EPUCK_MINI_0012"));
    EXPECT_FALSE(EpuckGATTProtocol::isEpuckMiniName("Random Device"));
}

TEST(GATTProtocol, EncodeOTAStart) {
    QByteArray cmd = EpuckGATTProtocol::encodeOTAStartCommand(1024, 0xDEADBEEF);
    EXPECT_EQ(cmd.size(), 8);
}

TEST(GATTProtocol, ParseProximity) {
    SensorData d;
    QByteArray data(16, '\0');
    data[0] = 0x10; data[1] = 0x0F; // 0x0F10 = 3856
    EpuckGATTProtocol::parseSensorData(EpuckGATTProtocol::CHAR_PROXIMITY, data, d);
    EXPECT_EQ(d.proximity[0], 0x0F10);
}
