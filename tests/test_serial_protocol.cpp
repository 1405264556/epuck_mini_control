#include <gtest/gtest.h>
#include "comm/EpuckSerComProtocol.h"
#include "comm/EpuckGATTProtocol.h"
#include "core/RobotInstance.h"
#include <cstring>
#include <limits>

TEST(SerialProtocol, OfficialCommandBytesAndTermination) {
    EXPECT_EQ(EpuckSerComProtocol::buildTelemetryPollCommand().toHex(), "b2bf9faf8b00");
    EXPECT_EQ(EpuckSerComProtocol::buildMotorCommand(100, -100).toHex(), "bc64009cff00");
    EXPECT_EQ(EpuckSerComProtocol::buildLEDCommand(9, 1).toHex(), "b4090100");
}

TEST(SerialProtocol, ZeroValuesAndNegativeProximityAreValid) {
    SensorData data;
    QByteArray bytes(28, '\0'); bytes[0] = char(0xff); bytes[1] = char(0xff);
    EXPECT_TRUE(EpuckSerComProtocol::parseSensorResponse(bytes, data));
    EXPECT_TRUE(data.has(SensorData::Proximity));
    EXPECT_TRUE(data.has(SensorData::AccelSpherical));
    EXPECT_EQ(data.proximity[0], 0);
}

TEST(SerialProtocol, MicrophoneIsNotMistakenForAcceleration) {
    SensorData data;
    QByteArray bytes(6, '\0'); bytes[0] = 10;
    ASSERT_TRUE(EpuckSerComProtocol::parseBinaryResponse(EpuckSerComProtocol::CMD_MIC, bytes, data));
    EXPECT_TRUE(data.has(SensorData::Microphone));
    EXPECT_FALSE(data.has(SensorData::Accelerometer));
    EXPECT_EQ(data.rawMicrophones[0], 10);
    EXPECT_EQ(data.microphoneChannels, 3);
}

TEST(SerialProtocol, CombinedFramePreservesEmbeddedNewlines) {
    SensorData data;
    QByteArray bytes(44, '\0'); bytes[0] = 10; bytes[34] = 10; bytes[38] = 10;
    ASSERT_TRUE(EpuckSerComProtocol::parseTelemetryResponse(bytes, data));
    EXPECT_EQ(data.proximity[0], 10);
    EXPECT_EQ(data.encoders[0], 10);
    EXPECT_EQ(data.rawMicrophones[0], 10);
    EXPECT_TRUE(data.has(SensorData::Accelerometer));
}

TEST(SerialProtocol, RejectsTruncatedAndNonFiniteFrames) {
    SensorData data;
    EXPECT_FALSE(EpuckSerComProtocol::parseTelemetryResponse(QByteArray(43, '\0'), data));
    EXPECT_FALSE(data.isValid());
    QByteArray bytes(28, '\0'); const float nan = std::numeric_limits<float>::quiet_NaN();
    std::memcpy(bytes.data()+16, &nan, 4);
    EXPECT_FALSE(EpuckSerComProtocol::parseSensorResponse(bytes, data));
    EXPECT_FALSE(data.isValid());
}

TEST(SerialProtocol, NonSquareCameraRotatesEveryPixel) {
    QByteArray bytes; bytes.append(char(0)); bytes.append(char(3)); bytes.append(char(2));
    for (int i = 1; i <= 6; ++i) bytes.append(char(i));
    const QImage image = EpuckSerComProtocol::parseImageResponse(bytes);
    ASSERT_EQ(image.size(), QSize(2, 3));
    int total = 0;
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) total += qGray(image.pixel(x,y));
    EXPECT_EQ(total, 21);
}

TEST(SensorData, PartialMergeAcceptsZeroAndPreservesOtherChannels) {
    SensorData accumulator, first, zero;
    first.timestamp = 100; first.proximity.fill(900); first.tof.fill(123);
    first.mark(SensorData::Proximity); first.mark(SensorData::ToF);
    accumulator.merge(first);
    zero.timestamp = 200; zero.mark(SensorData::Proximity);
    accumulator.merge(zero);
    EXPECT_EQ(accumulator.proximity[0], 0);
    EXPECT_EQ(accumulator.tof[0], 123);
    EXPECT_EQ(accumulator.fieldTimestamps[SensorData::ToF], 100);
    EXPECT_TRUE(accumulator.isFresh(SensorData::Proximity, 300, 100));
    EXPECT_FALSE(accumulator.isFresh(SensorData::ToF, 300, 100));
    EXPECT_FALSE(accumulator.has(SensorData::Gyroscope));
    accumulator.merge(first);
    EXPECT_EQ(accumulator.proximity[0], 0);
}

TEST(GATTProtocol, TruncatedNotificationDoesNotRefreshOldData) {
    SensorData data;
    EpuckGATTProtocol::parseSensorData(EpuckGATTProtocol::CHAR_PROXIMITY, QByteArray(3,'\0'), data);
    EXPECT_FALSE(data.isValid());
    EpuckGATTProtocol::parseSensorData(EpuckGATTProtocol::CHAR_BATTERY, QByteArray(1,char(80)), data);
    EXPECT_EQ(data.batteryPercent, 80);
    EXPECT_EQ(data.batteryVoltage, 0);
}

TEST(RobotTelemetry, EncodersDrivePoseAndHandleWraparound) {
    BLEDeviceInfo info; info.portName = "COM90";
    RobotInstance robot(info);
    SensorData baseline; baseline.timestamp = 100; baseline.encoders = {32760,32760}; baseline.mark(SensorData::Encoders);
    robot.updateSensorData(baseline);
    robot.recordMotorCommand(1, 1);
    EXPECT_DOUBLE_EQ(robot.position().x, 0);
    SensorData next; next.timestamp = 200; next.encoders = {-32760,-32760}; next.mark(SensorData::Encoders);
    robot.updateSensorData(next);
    EXPECT_NEAR(robot.position().x, 16*4.1*3.141592653589793/1000, 1e-8);
    EXPECT_DOUBLE_EQ(robot.position().y, 0);
    robot.updateSensorData(next);
    EXPECT_NEAR(robot.position().x, 16*4.1*3.141592653589793/1000, 1e-8);
}
