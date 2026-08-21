#include "frame_codec.h"
#include <QtEndian>

namespace motor::protocol {

namespace {

constexpr int kHeaderSize = 2 + 1 + 4 + 2 + 4 + 8 + 8;

void registerError(DecodeBatch& result, DecodeError error, int& errorsInWindow, QElapsedTimer& window) {
    result.errors.append(error);
    if (!window.isValid() || window.elapsed() >= 10000) {
        errorsInWindow = 0;
        window.start();
    }
    errorsInWindow++;
    if (errorsInWindow >= 3 && window.elapsed() < 10000) {
        result.mustDisconnect = true;
    }
}

}

QByteArray encodeFrame(const Frame& frame) {
    QByteArray result;
    result.reserve(kHeaderSize + frame.payload.size() + 4);

    quint16 magic = qToBigEndian(kMagic);
    result.append(reinterpret_cast<const char*>(&magic), sizeof(magic));

    quint8 version = kVersion;
    result.append(reinterpret_cast<const char*>(&version), sizeof(version));

    quint32 totalLength = static_cast<quint32>(kHeaderSize + frame.payload.size() + 4);
    quint32 totalLengthBE = qToBigEndian(totalLength);
    result.append(reinterpret_cast<const char*>(&totalLengthBE), sizeof(totalLengthBE));

    quint16 typeBE = qToBigEndian(static_cast<quint16>(frame.type));
    result.append(reinterpret_cast<const char*>(&typeBE), sizeof(typeBE));

    quint32 sequenceBE = qToBigEndian(frame.sequence);
    result.append(reinterpret_cast<const char*>(&sequenceBE), sizeof(sequenceBE));

    quint64 requestIdBE = qToBigEndian(frame.requestId);
    result.append(reinterpret_cast<const char*>(&requestIdBE), sizeof(requestIdBE));

    qint64 timestampBE = qToBigEndian(frame.timestampMs);
    result.append(reinterpret_cast<const char*>(&timestampBE), sizeof(timestampBE));

    result.append(frame.payload);

    quint32 crc = crc32(QByteArrayView(result));
    quint32 crcBE = qToBigEndian(crc);
    result.append(reinterpret_cast<const char*>(&crcBE), sizeof(crcBE));

    return result;
}

void FrameDecoder::reset() {
    buffer_.clear();
    errorsInWindow_ = 0;
    errorWindow_.invalidate();
}

DecodeBatch FrameDecoder::append(QByteArrayView bytes) {
    DecodeBatch result;

    if (buffer_.size() + bytes.size() > kMaxReceiveBufferBytes) {
        result.mustDisconnect = true;
        result.discardedBytes += buffer_.size() + bytes.size();
        buffer_.clear();
        return result;
    }

    buffer_.append(bytes.data(), bytes.size());

    while (buffer_.size() >= 2) {
        const uchar* data = reinterpret_cast<const uchar*>(buffer_.constData());
        quint16 magic = qFromBigEndian<quint16>(data);

        if (magic != kMagic) {
            qsizetype searchEnd = qMin(buffer_.size(), qsizetype(16));
            qsizetype nextMagic = -1;
            for (qsizetype i = 1; i < searchEnd; ++i) {
                quint16 candidate = qFromBigEndian<quint16>(data + i);
                if (candidate == kMagic) {
                    nextMagic = i;
                    break;
                }
            }
            qsizetype discard = (nextMagic >= 0) ? nextMagic : buffer_.size();
            result.discardedBytes += discard;
            registerError(result, DecodeError::BadMagic, errorsInWindow_, errorWindow_);
            buffer_.remove(0, static_cast<int>(discard));
            if (result.mustDisconnect) return result;
            continue;
        }

        if (buffer_.size() < 7) {
            break;
        }

        quint32 totalLength = qFromBigEndian<quint32>(data + 2 + 1);
        if (totalLength < kMinFrameBytes || totalLength > kMaxFrameBytes) {
            result.discardedBytes += 2;
            buffer_.remove(0, 2);
            registerError(result, DecodeError::InvalidLength, errorsInWindow_, errorWindow_);
            result.mustDisconnect = true;
            return result;
        }

        if (buffer_.size() < kHeaderSize) {
            break;
        }

        if (buffer_.size() < static_cast<qsizetype>(totalLength)) {
            break;
        }

        quint32 expectedCrc = qFromBigEndian<quint32>(data + totalLength - 4);
        quint32 actualCrc = crc32(QByteArrayView(buffer_.constData(), static_cast<int>(totalLength - 4)));
        if (actualCrc != expectedCrc) {
            result.discardedBytes += static_cast<qsizetype>(totalLength);
            buffer_.remove(0, static_cast<int>(totalLength));
            registerError(result, DecodeError::BadCrc, errorsInWindow_, errorWindow_);
            if (result.mustDisconnect) return result;
            continue;
        }

        quint16 typeRaw = qFromBigEndian<quint16>(data + 2 + 1 + 4);
        MessageType type = static_cast<MessageType>(typeRaw);

        quint32 sequence = qFromBigEndian<quint32>(data + 2 + 1 + 4 + 2);
        quint64 requestId = qFromBigEndian<quint64>(data + 2 + 1 + 4 + 2 + 4);
        qint64 timestamp = qFromBigEndian<qint64>(data + 2 + 1 + 4 + 2 + 4 + 8);

        QByteArray payload = buffer_.mid(kHeaderSize, static_cast<int>(totalLength - kHeaderSize - 4));

        Frame frame{type, sequence, requestId, timestamp, std::move(payload)};
        result.frames.append(std::move(frame));

        buffer_.remove(0, static_cast<int>(totalLength));
    }

    return result;
}

}
