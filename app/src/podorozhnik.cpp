#include "podorozhnik.h"

namespace Podorozhnik {

const QByteArray keyDefault  = QByteArray::fromHex("FFFFFFFFFFFF");
const QByteArray sector4KeyA = QByteArray::fromHex("E56AC127DD45");
const QByteArray sector4KeyB = QByteArray::fromHex("19FC84A3784B");

// Все многобайтовые целые на карте — little-endian (metrodroid byteArrayToIntReversed)
static quint64 leUInt(const QByteArray &data, int offset, int size)
{
    quint64 value = 0;
    for (int i = size - 1; i >= 0; --i)
        value = (value << 8) | quint8(data.at(offset + i));
    return value;
}

QString luhnCheckDigit(const QString &digits)
{
    // Контрольная цифра, при которой сумма Луна (digits + цифра) делится на 10.
    // Удваиваются цифры на нечётных позициях справа (позиция 0 — сама цифра проверки).
    const QString extended = digits + QLatin1Char('0');
    int sum = 0;
    for (int i = 0; i < extended.size(); ++i) {
        int d = extended.at(extended.size() - 1 - i).digitValue();
        if (i % 2 == 1) {
            d *= 2;
            if (d > 9)
                d -= 9;
        }
        sum += d;
    }
    return QString::number((10 - (sum % 10)) % 10);
}

QString cardNumberFromBlock0(const QByteArray &block0)
{
    if (block0.size() < 7)
        return QString();

    const QString base = QStringLiteral("96433078")
            + QString::number(leUInt(block0, 0, 7));
    const QString full = base + luhnCheckDigit(base);

    // Группы по 4 цифры: «9643 3078 XXXX XXXX …» (как formatNumber в metrodroid)
    QString grouped;
    for (int i = 0; i < full.size(); ++i) {
        if (i > 0 && i % 4 == 0)
            grouped += QLatin1Char(' ');
        grouped += full.at(i);
    }
    return grouped;
}

quint32 balanceFromBlock(const QByteArray &block)
{
    return block.size() < 4 ? 0 : quint32(leUInt(block, 0, 4));
}

QString formatBalance(quint32 kopecks)
{
    QString rubles = QString::number(kopecks / 100);
    for (int i = rubles.size() - 3; i > 0; i -= 3)
        rubles.insert(i, QLatin1Char(' '));
    return QStringLiteral("%1,%2 ₽").arg(rubles).arg(kopecks % 100, 2, 10, QLatin1Char('0'));
}

quint16 crcA(const QByteArray &data)
{
    quint16 crc = 0x6363;
    for (const char byte : data) {
        crc ^= quint8(byte);
        for (int i = 0; i < 8; ++i)
            crc = (crc & 1) ? quint16((crc >> 1) ^ 0x8408) : quint16(crc >> 1);
    }
    return crc;
}

QByteArray withCrc(QByteArray frame)
{
    const quint16 crc = crcA(frame);
    frame.append(char(crc & 0xFF));
    frame.append(char(crc >> 8));
    return frame;
}

} // namespace Podorozhnik
