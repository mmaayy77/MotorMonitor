#include <gtest/gtest.h>
#include "protocol/frame.h"
#include "protocol/frame_codec.h"
#include "protocol/crc32.h"
#include <QByteArray>
#include <QtEndian>

using namespace motor::protocol;

TEST(FrameCodecTest, RoundTripsFrame) {
    Frame input;
    input.type = MessageType::TelemetryReport;
    input.sequence = 1234;
    input.requestId = 5678;
    input.timestampMs = 1234567890;
    input.payload = QByteArray("hello world", 11);

    QByteArray encoded = encodeFrame(input);
    EXPECT_GE(encoded.size(), kMinFrameBytes);
    EXPECT_LE(encoded.size(), kMaxFrameBytes);

    FrameDecoder decoder;
    DecodeBatch result = decoder.append(encoded);

    EXPECT_FALSE(result.mustDisconnect);
    EXPECT_EQ(result.discardedBytes, 0);
    EXPECT_TRUE(result.errors.isEmpty());
    EXPECT_EQ(result.frames.size(), 1);

    const Frame& output = result.frames[0];
    EXPECT_EQ(output.type, input.type);
    EXPECT_EQ(output.sequence, input.sequence);
    EXPECT_EQ(output.requestId, input.requestId);
    EXPECT_EQ(output.timestampMs, input.timestampMs);
    EXPECT_EQ(output.payload, input.payload);
}

TEST(FrameCodecTest, WaitsForSecondHalf) {
    Frame full;
    full.type = MessageType::ConnectRequest;
    full.sequence = 1;
    full.requestId = 100;
    full.timestampMs = 123;
    full.payload = QByteArray("MOTOR-0001", 10);
    QByteArray encoded = encodeFrame(full);

    QByteArray firstHalf = encoded.left(encoded.size() / 2);
    QByteArray secondHalf = encoded.mid(encoded.size() / 2);

    FrameDecoder decoder;
    DecodeBatch first = decoder.append(firstHalf);
    EXPECT_EQ(first.frames.size(), 0);
    EXPECT_FALSE(first.mustDisconnect);

    DecodeBatch second = decoder.append(secondHalf);
    EXPECT_EQ(second.frames.size(), 1);
    EXPECT_FALSE(second.mustDisconnect);
}

TEST(FrameCodecTest, ExtractsStickyFrames) {
    Frame f1;
    f1.type = MessageType::HeartbeatRequest;
    f1.sequence = 1;
    f1.requestId = 0;
    f1.timestampMs = 1;
    f1.payload = QByteArray();
    Frame f2;
    f2.type = MessageType::HeartbeatResponse;
    f2.sequence = 2;
    f2.requestId = 0;
    f2.timestampMs = 2;
    f2.payload = QByteArray();

    QByteArray both = encodeFrame(f1) + encodeFrame(f2);

    FrameDecoder decoder;
    DecodeBatch result = decoder.append(both);
    EXPECT_EQ(result.frames.size(), 2);
    EXPECT_EQ(result.frames[0].type, MessageType::HeartbeatRequest);
    EXPECT_EQ(result.frames[1].type, MessageType::HeartbeatResponse);
    EXPECT_TRUE(result.errors.isEmpty());
}

TEST(FrameCodecTest, RejectsLengthAbove65536) {
    QByteArray buffer;
    quint16 magic = qToBigEndian(kMagic);
    buffer.append(reinterpret_cast<const char*>(&magic), 2);
    buffer.append(static_cast<char>(kVersion));
    quint32 bigLen = qToBigEndian(quint32(100'000));
    buffer.append(reinterpret_cast<const char*>(&bigLen), 4);

    FrameDecoder decoder;
    DecodeBatch result = decoder.append(buffer);

    EXPECT_TRUE(result.mustDisconnect);
    EXPECT_GE(result.errors.count(), 1);
    EXPECT_EQ(result.errors[0], DecodeError::InvalidLength);
}

TEST(FrameCodecTest, RejectsBadCrc) {
    Frame input;
    input.type = MessageType::ConnectRequest;
    input.sequence = 1;
    input.requestId = 1;
    input.timestampMs = 1;
    input.payload = QByteArray("X", 1);
    QByteArray encoded = encodeFrame(input);
    encoded[encoded.size() - 1] = static_cast<char>(encoded[encoded.size() - 1] ^ 0xFF);

    FrameDecoder decoder;
    DecodeBatch result = decoder.append(encoded);

    EXPECT_EQ(result.errors.count(), 1);
    EXPECT_EQ(result.errors[0], DecodeError::BadCrc);
    EXPECT_EQ(result.frames.size(), 0);
}

TEST(FrameCodecTest, ResynchronizesAtNextMagic) {
    Frame good;
    good.type = MessageType::TelemetryReport;
    good.sequence = 1;
    good.requestId = 1;
    good.timestampMs = 1;
    good.payload = QByteArray();
    QByteArray goodEncoded = encodeFrame(good);

    QByteArray garbage = QByteArray("abc", 3);

    QByteArray combined = garbage + goodEncoded;

    FrameDecoder decoder;
    DecodeBatch result = decoder.append(combined);
    EXPECT_EQ(result.discardedBytes, 3);
    EXPECT_EQ(result.errors.count(), 1);
    EXPECT_EQ(result.errors[0], DecodeError::BadMagic);
    EXPECT_EQ(result.frames.count(), 1);
    EXPECT_FALSE(result.mustDisconnect);
}

TEST(FrameCodecTest, DisconnectsAtBufferLimit) {
    QByteArray big;
    big.resize(kMaxReceiveBufferBytes + 100);
    big.fill('X');

    FrameDecoder decoder;
    DecodeBatch result = decoder.append(big);

    EXPECT_TRUE(result.mustDisconnect);
    EXPECT_GT(result.discardedBytes, 0);
}

TEST(FrameCodecTest, DisconnectsAfterThreeErrorsInTenSeconds) {
    FrameDecoder decoder;

    QByteArray bad1;
    bad1.append(static_cast<char>(0x00));
    bad1.append(static_cast<char>(0x00));
    DecodeBatch r1 = decoder.append(bad1);
    EXPECT_FALSE(r1.mustDisconnect);

    QByteArray bad2;
    bad2.append(static_cast<char>(0x00));
    bad2.append(static_cast<char>(0x00));
    DecodeBatch r2 = decoder.append(bad2);
    EXPECT_FALSE(r2.mustDisconnect);

    QByteArray bad3;
    bad3.append(static_cast<char>(0x00));
    bad3.append(static_cast<char>(0x00));
    DecodeBatch r3 = decoder.append(bad3);
    EXPECT_TRUE(r3.mustDisconnect);
}
