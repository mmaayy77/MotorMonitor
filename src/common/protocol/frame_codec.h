#pragma once

#include "frame.h"
#include "crc32.h"

namespace motor::protocol {

QByteArray encodeFrame(const Frame& frame);

class FrameDecoder {
public:
    DecodeBatch append(QByteArrayView bytes);
    void reset();
private:
    QByteArray buffer_;
    int errorsInWindow_{0};
    QElapsedTimer errorWindow_;
};

}
