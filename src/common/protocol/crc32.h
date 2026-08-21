#pragma once

#include <QByteArray>

namespace motor::protocol {

quint32 crc32(QByteArrayView data);

}
