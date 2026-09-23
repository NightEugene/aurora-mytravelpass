#include "troika.h"

namespace Troika {

const QByteArray sector8KeyA = QByteArray::fromHex("A73F5DC1D333");
const QByteArray sector8KeyB = QByteArray::fromHex("E35173494A81");
const QByteArray sector7KeyA = QByteArray::fromHex("AE3D65A3DAD4");
const QByteArray sector7KeyB = QByteArray::fromHex("0F1C63013DBA");
const QByteArray sector4KeyA = QByteArray::fromHex("73068F118C13");
const QByteArray sector4KeyB = QByteArray::fromHex("2B7F3253FAC5");
const QByteArray sector1KeyA = QByteArray::fromHex("A82607B01C0D");
const QByteArray sector1KeyB = QByteArray::fromHex("2910989B6880");

// Биты от начала записи, старшим вперёд (как bit_lib_get_bits у Flipper
// и getBitsFromBuffer у metrodroid); многобитовые поля — big-endian
static quint64 getBits(const QByteArray &record, int offset, int length)
{
    quint64 value = 0;
    for (int i = 0; i < length; ++i) {
        const int bit = offset + i;
        if (bit / 8 >= record.size())
            break;
        value = (value << 1)
                | ((quint8(record.at(bit / 8)) >> (7 - (bit % 8))) & 1);
    }
    return value;
}

// Эпохи отсчётов на карте (московское время, UTC+3)
static QDateTime epoch(int year)
{
    return QDateTime(QDate(year, 1, 1), QTime(0, 0), Qt::OffsetFromUTC, 3 * 3600);
}

bool isTransportRecord(const QByteArray &record)
{
    if (record.size() < 8)
        return false;
    const quint16 dept = quint16(getBits(record, 0, 10));
    switch (dept) {
    case 0x106: case 0x108: case 0x10A:
    case 0x10E: case 0x110: case 0x117:
        return true;
    default:
        return false;
    }
}

PurseInfo purseFromSector8(const QByteArray &record)
{
    PurseInfo info;
    if (!isTransportRecord(record))
        return info;

    // Печатный номер карты: биты 20-51, десятичный с ведущими нулями
    info.cardNumber = QStringLiteral("%1")
            .arg(getBits(record, 20, 32), 10, 10, QLatin1Char('0'));

    // Раскладка записи: биты 52-55; 0xE — расширенная, подтип в битах 56-60
    const int layout = int(getBits(record, 52, 4));
    const int subLayout = layout == 0xE ? int(getBits(record, 56, 5)) : -1;

    if (layout == 0xE && subLayout == 3) {
        // E3 — самый распространённый кошелёк (metrodroid TroikaPurseE3,
        // flipper parse_layout_E3)
        info.balanceKopecks = quint32(getBits(record, 188, 22));
        info.balanceValid = true;
        info.blocked = getBits(record, 212, 1) != 0;
        const quint32 expiryDays = quint32(getBits(record, 61, 16));
        if (expiryDays != 0) // дни от 01.01.1992, 1-based (metrodroid)
            info.expiry = epoch(1992).addDays(qint64(expiryDays) - 1).date();
        info.lastValidator = quint32(getBits(record, 128, 16));
        const quint32 validationMin = quint32(getBits(record, 144, 23));
        if (validationMin != 0) {
            // минуты от 01.01.2016, но отсчёт со смещением +1 сутки
            // (metrodroid convertDateTime2016: dayMinute(-1, mins))
            info.lastTrip = epoch(2016)
                    .addSecs((qint64(validationMin) - 1440) * 60);
            // Тип транспорта: биты 178-179; при 1 — уточняющий код 180-181
            switch (getBits(record, 178, 2)) {
            case 1:
                switch (getBits(record, 180, 2)) {
                case 1:  info.lastTripTransport = QStringLiteral("Метро"); break;
                case 2:  info.lastTripTransport = QStringLiteral("Монорельс"); break;
                case 3:  info.lastTripTransport = QStringLiteral("МЦК"); break;
                default: break;
                }
                break;
            case 2:
                info.lastTripTransport = QStringLiteral("Наземный");
                break;
            default:
                break;
            }
        }
    } else if (layout == 0xE && subLayout == 5) {
        // E5 — кошелёк карт ~2019+ (metrodroid TroikaPurseE5,
        // flipper parse_layout_E5); отсчёты от 01.01.2019
        info.balanceKopecks = quint32(getBits(record, 167, 19));
        info.balanceValid = true;
        info.blocked = getBits(record, 202, 1) != 0;
        const quint32 expiryDays = quint32(getBits(record, 61, 13));
        if (expiryDays != 0) // дни от 01.01.2019, 1-based (metrodroid)
            info.expiry = epoch(2019).addDays(qint64(expiryDays) - 1).date();
        // Минутные поля E5 хранят отсчёт со смещением +1 сутки
        // (metrodroid convertDateTime2019: dayMinute(-1, mins))
        const quint32 validationMin = quint32(getBits(record, 128, 23));
        if (validationMin != 0)
            info.lastTrip = epoch(2019)
                    .addSecs((qint64(validationMin) - 1440) * 60);
        const quint32 refillMin = quint32(getBits(record, 84, 23));
        if (refillMin != 0)
            info.lastTopup = epoch(2019)
                    .addSecs((qint64(refillMin) - 1440) * 60);
        info.lastValidator = quint32(getBits(record, 186, 16));
        info.tripsOnPurse = int(getBits(record, 216, 7));
        info.refillCounter = int(getBits(record, 107, 10));
    } else if (layout == 0xE && subLayout == 1) {
        // E1 — старый кошелёк (до ~2016): дата в днях от 01.01.1992,
        // время в минутах от начала суток
        info.balanceKopecks = quint32(getBits(record, 196, 19));
        info.balanceValid = true;
        const quint32 tripDays = quint32(getBits(record, 144, 16));
        const quint32 tripMin = quint32(getBits(record, 160, 11));
        if (tripDays != 0)
            info.lastTrip = epoch(1992).addDays(qint64(tripDays) - 1)
                    .addSecs(qint64(tripMin) * 60);
    }
    // Остальные layout'ы (билеты E2/E4/A/D/…): показываем только номер

    return info;
}

// Имя билета по коду типа (metrodroid TroikaBlock.getHeader)
static QString ticketName(quint16 ticketType)
{
    switch (ticketType) {
    case 0x5d9a:
    case 0x5d9b: return QStringLiteral("1 поездка");
    case 0x5d9c: return QStringLiteral("2 поездки");
    case 0x5da0: return QStringLiteral("20 поездок");
    case 0x5dd3: return QStringLiteral("60 поездок");
    case 0x183d:
    case 0x2129: return QStringLiteral("Карта дружинника");
    default:     return QStringLiteral("Проездной");
    }
}

// Дни от 01.01.1992, 1-based; 0 — пустое поле (metrodroid
// convertDateTime1992)
static QDateTime days1992(quint32 days)
{
    return days == 0 ? QDateTime() : epoch(1992).addDays(qint64(days) - 1);
}

TicketInfo ticketFromRecord(const QByteArray &record)
{
    TicketInfo info;
    if (!isTransportRecord(record))
        return info;

    info.ticketType = quint16(getBits(record, 4, 16));
    switch (info.ticketType) {
    // Пустые билетные держатели (metrodroid getHeader/TroikaLayout2)
    case 0x5d3d: case 0x5d3e: case 0x5d48:
    case 0x2135: case 0x2141:
    case 0x5db1: // кошелёк — не билет
        return info;
    default:
        break;
    }

    const int layout = int(getBits(record, 52, 4));
    const int subLayout = layout == 0xE ? int(getBits(record, 56, 5)) : -1;
    QDateTime validityStart;
    switch (layout) {
    case 0x2: {
        // Placeholder-запись сектора 7/4 (metrodroid TroikaLayout2):
        // только сроки действия, поездок нет
        validityStart = days1992(quint32(getBits(record, 157, 16)));
        info.validityEnd = days1992(quint32(getBits(record, 173, 16)));
        break;
    }
    case 0xA: {
        // Билеты на 1-2 поездки (metrodroid TroikaLayoutA), отсчёт от 2016
        const quint32 startDays = quint32(getBits(record, 67, 9));
        const qint64 lengthMin = qint64(getBits(record, 76, 19));
        if (startDays == 0 || lengthMin == 0)
            return info;
        validityStart = epoch(2016).addDays(qint64(startDays) - 1);
        info.validityEnd = validityStart.addSecs((lengthMin - 1) * 60);
        info.remainingTrips = int(getBits(record, 128, 8));
        break;
    }
    case 0xD: {
        // Старые билеты на несколько поездок (metrodroid TroikaLayoutD)
        validityStart = days1992(quint32(getBits(record, 128, 16)));
        info.validityEnd = days1992(quint32(getBits(record, 64, 16)));
        info.remainingTrips = int(getBits(record, 166, 10));
        break;
    }
    case 0xE:
        if (subLayout != 2)
            return info; // E1/E3/E5 — кошельки, остальное неизвестно
        // Новые билеты на несколько поездок (metrodroid TroikaLayoutE2)
        validityStart = days1992(quint32(getBits(record, 97, 16)));
        if (!validityStart.isValid())
            return info;
        info.validityEnd = validityStart.addSecs(
                    (qint64(getBits(record, 131, 20)) - 1) * 60);
        info.remainingTrips = int(getBits(record, 167, 10));
        break;
    default:
        return info;
    }

    // Действующий билет: есть поездки или неистёкший срок
    if (info.remainingTrips <= 0
            && (!info.validityEnd.isValid()
                || info.validityEnd.date() < QDate::currentDate()))
        return info;

    info.present = true;
    info.ticketName = ticketName(info.ticketType);
    return info;
}

} // namespace Troika
