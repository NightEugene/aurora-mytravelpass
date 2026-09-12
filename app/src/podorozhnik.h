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

// Печатный номер карты: «9643 3078 » + 7-байтовый LE-номер из блока 0
// + контрольная цифра Луна (как в metrodroid getSerial()).
QString cardNumberFromBlock0(const QByteArray &block0);

// Баланс в копейках: сектор 4, блок 0, байты 0–3, uint32 LE.
quint32 balanceFromBlock(const QByteArray &block);

// «1 234,56 ₽»
QString formatBalance(quint32 kopecks);

QString luhnCheckDigit(const QString &digits);

// CRC_A по ISO/IEC 14443-3 (полином 0x8408, инициализация 0x6363).
// nfcd требует CRC в кадре: без него Transceive возвращает ошибку передачи.
quint16 crcA(const QByteArray &data);

// Дописывает CRC_A к кадру (2 байта, младший первым).
QByteArray withCrc(QByteArray frame);

} // namespace Podorozhnik
