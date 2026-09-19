#pragma once

#include <QByteArray>
#include <QString>

// Парсинг данных проездного «Подорожник» (Санкт-Петербург).
// Карта — MIFARE Classic (1K/4K). Раскладка секторов и ключи подтверждены
// независимыми открытыми источниками:
//  - metrodroid: transit/podorozhnik/PodorozhnikTransitData.kt
//    (md5-дайджесты ключей сектора 4 с солью «podorozhnik»);
//  - Flipper Zero firmware: applications/main/nfc/plugins/supported_cards/plantain.c
//    (полные таблицы ключей 1K/4K);
//  - plantain_parser (github.com/rozetkinrobot/plantain_parser).
namespace Podorozhnik {

// Ключи секторов, используемые приложением (только чтение)
extern const QByteArray keyDefault;   // FFFFFFFFFFFF — сектор 0 (номер карты)
extern const QByteArray sector4KeyA;  // E56AC127DD45 — сектор 4 (баланс)
extern const QByteArray sector4KeyB;  // 19FC84A3784B — запасной ключ сектора 4
extern const QByteArray sector5KeyA;  // 77DABC9825E1 — сектор 5 (поездки)
extern const QByteArray sector5KeyB;  // 9764FEC3154A — запасной ключ сектора 5

// Печатный номер карты: «9643 3078 » + 7-байтовый LE-номер из блока 0
// + контрольная цифра Луна (как в metrodroid getSerial()).
QString cardNumberFromBlock0(const QByteArray &block0);

// Баланс в копейках: сектор 4, блок 0, байты 0–3, uint32 LE.
quint32 balanceFromBlock(const QByteArray &block);

// Время на карте — минуты с 01.01.2010 00:00 МСК (metrodroid
// PODOROZHNIK_EPOCH); формат «dd.MM.yyyy HH:mm».
QString minutesToDateTimeText(quint32 minutes);

// Последняя поездка: сектор 5, блок 0 (метки времени/транспорта/тарифа —
// как в metrodroid decodeSector5)
struct TripInfo {
    quint32 timeMinutes = 0;   // 0 — поездки не было
    quint8  transport = 0;
    quint16 validator = 0;
    quint32 fareKopecks = 0;
    bool valid = false;
};
TripInfo tripFromBlock(const QByteArray &block);

// Название вида транспорта по коду (metrodroid PodorozhnikTrip):
// 1 — метро (но validator 0 = кривой валидатор наземного),
// 3/4 — автобус/троллейбус/трамвай, 7 — маршрутное такси.
QString transportName(quint8 transport, quint16 validator);

// Последнее пополнение: сектор 4, блок 2 (metrodroid decodeSector4)
struct TopupInfo {
    quint32 timeMinutes = 0;   // 0 — пополнений не было
    quint32 amountKopecks = 0;
    bool valid = false;
};
TopupInfo topupFromBlock(const QByteArray &block);

// Счётчики поездок (метро/наземный): сектор 5, блоки 1 и 2 дублируют
// друг друга — берутся с более свежего по метке времени. Счётчики
// месячные: сбрасываются при первой поездке нового месяца, месяц
// определяется меткой времени свежего блока (возвращается в tsMinutes).
bool countersFromBlocks(const QByteArray &block1, const QByteArray &block2,
                        int &subway, int &ground, quint32 &tsMinutes);

// «август 2026» — месяц и год по метке времени счётчиков
QString monthYearText(quint32 minutes);

// «1 234,56 ₽»
QString formatBalance(quint32 kopecks);

QString luhnCheckDigit(const QString &digits);

// CRC_A по ISO/IEC 14443-3 (полином 0x8408, инициализация 0x6363).
// nfcd требует CRC в кадре: без него Transceive возвращает ошибку передачи.
quint16 crcA(const QByteArray &data);

// Дописывает CRC_A к кадру (2 байта, младший первым).
QByteArray withCrc(QByteArray frame);

} // namespace Podorozhnik
