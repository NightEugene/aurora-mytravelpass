#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

// Парсинг транспортной карты «Тройка» (Москва), MIFARE Classic 1K/4K.
// Ключи секторов статические (одинаковые у всех карт); раскладка
// 48-байтовых транспортных записей и ключи подтверждены независимыми
// открытыми источниками:
//  - Flipper Zero firmware: applications/main/nfc/plugins/supported_cards/troika.c
//    и api/mosgortrans/mosgortrans_util.c;
//  - metrodroid: transit/troika/* (TroikaBlock, TroikaPurseE3, TroikaPurseE5).
//
// Транспортная запись — 48 байт (3 блока данных сектора). Поля адресуются
// БИТАМИ от начала записи, старшим битом вперёд (bit 0 = MSB байта 0),
// многобитовые поля — big-endian.
namespace Troika {

// Ключ A сектора 8 (основная запись: кошелёк, последняя поездка)
extern const QByteArray sector8KeyA;  // A73F5DC1D333
extern const QByteArray sector8KeyB;  // E35173494A81 — запасной
// Билетные записи — сектора 7, 4 и 1 (ключи из troika.c Flipper Zero)
extern const QByteArray sector7KeyA;  // AE3D65A3DAD4
extern const QByteArray sector7KeyB;  // 0F1C63013DBA
extern const QByteArray sector4KeyA;  // 73068F118C13
extern const QByteArray sector4KeyB;  // 2B7F3253FAC5
extern const QByteArray sector1KeyA;  // A82607B01C0D
extern const QByteArray sector1KeyB;  // 2910989B6880

// Магия заголовка записи: биты 0-9 — код отдела транспорта
bool isTransportRecord(const QByteArray &record);

struct PurseInfo {
    QString cardNumber;              // печатный номер, 10 цифр
    quint32 balanceKopecks = 0;
    bool balanceValid = false;
    QDateTime lastTrip;              // невалидна, если поездок не было
    QString lastTripTransport;       // «Метро»/«Наземный»/…, может быть пустым
    QDateTime lastTopup;             // E5: время последнего пополнения
    QDate expiry;                    // срок действия карты
    quint32 lastValidator = 0;       // номер последнего валидатора
    int tripsOnPurse = -1;           // E5: счётчик поездок по кошельку
    int refillCounter = -1;          // E5: счётчик пополнений
    bool blocked = false;
};

// record — 48 байт: три блока данных сектора 8. На ST-стеке чтение
// возвращает 15 байт на блок (последний срезается стеком) — вызывающий
// дополняет каждый блок нулевым байтом до 16, чтобы сохранить смещения.
// Парсятся layout'ы электронного кошелька: E3, E5 (2019+) и E1 (старые);
// для остальных возвращается только номер карты.
PurseInfo purseFromSector8(const QByteArray &record);

// Билетная запись (проездной) из секторов 7/4/1. Layout'ы 2, A, D, E2
// (metrodroid TroikaLayout2/A/D/E2): тип билета, срок действия,
// остаток поездок.
struct TicketInfo {
    bool present = false;      // действующий билет (не пустой держатель)
    quint16 ticketType = 0;
    QString ticketName;        // «60 поездок», «Проездной»…
    QDateTime validityEnd;     // конец срока действия
    int remainingTrips = -1;   // остаток поездок; -1 — нет данных/безлимит
};
TicketInfo ticketFromRecord(const QByteArray &record);

} // namespace Troika
