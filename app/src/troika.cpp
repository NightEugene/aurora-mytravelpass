#include "troika.h"

namespace Troika {

const QByteArray sector8KeyA = QByteArray::fromHex("A73F5DC1D333");
const QByteArray sector8KeyB = QByteArray::fromHex("E35173494A81");

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
        const quint32 validationMin = quint32(getBits(record, 144, 23));
        if (validationMin != 0) {
            info.lastTrip = epoch(2016).addSecs(qint64(validationMin) * 60);
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
        const quint32 validationMin = quint32(getBits(record, 128, 23));
        if (validationMin != 0)
            info.lastTrip = epoch(2019).addSecs(qint64(validationMin) * 60);
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

} // namespace Troika
