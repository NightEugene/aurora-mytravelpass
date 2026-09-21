#include "podorozhnik.h"

#include <QDateTime>
#include <QLocale>

namespace Podorozhnik {

const QByteArray keyDefault  = QByteArray::fromHex("FFFFFFFFFFFF");
const QByteArray keyMad      = QByteArray::fromHex("A0A1A2A3A4A5");
const QByteArray sector4KeyA = QByteArray::fromHex("E56AC127DD45");
const QByteArray sector4KeyB = QByteArray::fromHex("19FC84A3784B");
const QByteArray sector5KeyA = QByteArray::fromHex("77DABC9825E1");
const QByteArray sector5KeyB = QByteArray::fromHex("9764FEC3154A");
const QByteArray sector8KeyA  = QByteArray::fromHex("26973EA74321");
const QByteArray sector8KeyB  = QByteArray::fromHex("D27058C6E2C7");
const QByteArray sector9KeyA  = QByteArray::fromHex("EB0A8FF88ADE");
const QByteArray sector9KeyB  = QByteArray::fromHex("578A9ADA41E3");
const QByteArray sector10KeyA = QByteArray::fromHex("EA0FD73CB149");
const QByteArray sector10KeyB = QByteArray::fromHex("29C35FA068FB");
const QByteArray sector11KeyA = QByteArray::fromHex("C76BF71A2509");
const QByteArray sector11KeyB = QByteArray::fromHex("9BA241DB3F56");
const QByteArray sector12KeyA = QByteArray::fromHex("ACFFFFFFFFFF");
const QByteArray sector12KeyB = QByteArray::fromHex("71F3A315AD26");

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

QString minutesToDateTimeText(quint32 minutes)
{
    const QDateTime epoch(QDate(2010, 1, 1), QTime(0, 0), Qt::OffsetFromUTC, 3 * 3600);
    return epoch.addSecs(qint64(minutes) * 60).toString(QStringLiteral("dd.MM.yyyy HH:mm"));
}

TripInfo tripFromBlock(const QByteArray &block)
{
    TripInfo trip;
    if (block.size() < 10)
        return trip;
    trip.timeMinutes = quint32(leUInt(block, 0, 3));
    trip.transport = quint8(block.at(3));
    trip.validator = quint16(leUInt(block, 4, 2));
    trip.fareKopecks = quint32(leUInt(block, 6, 4));
    trip.valid = trip.timeMinutes != 0;
    return trip;
}

QString transportName(quint8 transport, quint16 validator)
{
    switch (transport) {
    case 1: // метро; validator == 0 — криво настроенный валидатор наземного
        return validator == 0 ? QStringLiteral("Наземный")
                              : QStringLiteral("Метро");
    case 3: // автобус с переносным валидатором
    case 4: // автобус со стационарным валидатором
        return QStringLiteral("Наземный");
    case 7:
        return QStringLiteral("Маршрутное такси");
    default:
        return QStringLiteral("Транспорт %1").arg(transport);
    }
}

TopupInfo topupFromBlock(const QByteArray &block)
{
    TopupInfo topup;
    if (block.size() < 11)
        return topup;
    topup.timeMinutes = quint32(leUInt(block, 2, 3));
    topup.amountKopecks = quint32(leUInt(block, 8, 3));
    topup.valid = topup.timeMinutes != 0;
    return topup;
}

bool countersFromBlocks(const QByteArray &block1, const QByteArray &block2,
                        int &subway, int &ground, quint32 &tsMinutes)
{
    if (block1.size() < 5 || block2.size() < 5)
        return false;
    const QByteArray &fresh = leUInt(block2, 2, 3) > leUInt(block1, 2, 3)
            ? block2 : block1;
    subway = quint8(fresh.at(0));
    ground = quint8(fresh.at(1));
    tsMinutes = quint32(leUInt(fresh, 2, 3));
    return true;
}

QString monthYearText(quint32 minutes)
{
    const QDateTime epoch(QDate(2010, 1, 1), QTime(0, 0), Qt::OffsetFromUTC, 3 * 3600);
    const QDate date = epoch.addSecs(qint64(minutes) * 60).date();
    const QLocale ru(QLocale::Russian, QLocale::Russia);
    return ru.standaloneMonthName(date.month(), QLocale::LongFormat)
            + QLatin1Char(' ') + QString::number(date.year());
}

QDate passExpiryFromBlock(const QByteArray &block)
{
    if (block.size() < 13)
        return QDate();
    const int year  = 2000 + quint8(block.at(10));
    const int month = quint8(block.at(11));
    const int day   = quint8(block.at(12));
    const QDate date(year, month, day);
    // Отсекаем пустое поле (00 00 00 / FF FF FF) и явный мусор
    if (!date.isValid() || year < 2015 || year > 2040)
        return QDate();
    return date;
}

QDate passStartFromBlock(const QByteArray &block)
{
    if (block.size() < 3)
        return QDate();
    const int year  = 2000 + quint8(block.at(0));
    const int month = quint8(block.at(1));
    const int day   = quint8(block.at(2));
    const QDate date(year, month, day);
    if (!date.isValid() || year < 2015 || year > 2040)
        return QDate();
    return date;
}

bool passRidesFromBlock(const QByteArray &block, quint32 &rides)
{
    if (block.size() < 12)
        return false;
    const quint32 v1 = quint32(leUInt(block, 0, 4));
    const quint32 v2 = quint32(leUInt(block, 4, 4));
    const quint32 v3 = quint32(leUInt(block, 8, 4));
    if (v1 != v3 || v2 != ~v1) // value block: значение, ~значение, значение
        return false;
    rides = v1;
    return true;
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
