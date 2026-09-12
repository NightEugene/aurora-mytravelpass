#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QList>
#include <QVariantList>

#include <functional>

// Читает карту «Подорожник» через D-Bus API nfcd (org.sailfishos.nfc.*).
// Следит за метками NFC-адаптера; при появлении метки MIFARE Classic
// авторизует сектор баланса и читает его, затем номер карты из сектора 0.
//
// Используемые интерфейсы (документация ОС Аврора, раздел NFCD):
//  - org.sailfishos.nfc.Daemon  (объект /)       — список адаптеров;
//  - org.sailfishos.nfc.Adapter (объект /nfc0)   — метки и питание;
//  - org.sailfishos.nfc.Tag     (объект /nfc0/tagN) — тип, Acquire/Transceive;
//  - org.sailfishos.nfc.TagClassic (на объекте метки) — UID.
class CardReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool nfcEnabled READ nfcEnabled NOTIFY nfcEnabledChanged)
    Q_PROPERTY(QString balanceText READ balanceText NOTIFY dataChanged)
    Q_PROPERTY(QString cardNumber READ cardNumber NOTIFY dataChanged)
    Q_PROPERTY(QString uidText READ uidText NOTIFY dataChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)

public:
    enum State { Waiting, Reading, Result, Error };
    Q_ENUM(State)

    explicit CardReader(QObject *parent = nullptr);

    State state() const { return m_state; }
    bool nfcEnabled() const { return m_nfcEnabled; }
    QString balanceText() const { return m_balanceText; }
    QString cardNumber() const { return m_cardNumber; }
    QString uidText() const { return m_uidText; }
    QString errorText() const { return m_errorText; }

signals:
    void stateChanged();
    void nfcEnabledChanged();
    void dataChanged();
    void errorChanged();

private slots:
    void onServiceRegistered();
    void onTargetPresentChanged(bool present);
    void onPoweredChanged(bool powered);
    void onTagRemoved();

private:
    void connectDaemon();
    void connectAdapter(const QString &adapterPath);
    void readTag(const QString &tagPath);
    void finishOk();
    void fail(const QString &message);
    void setState(State state);

    // Асинхронный вызов метода nfcd; в done передаётся список выходных
    // аргументов либо текст ошибки.
    void asyncCall(const QString &path, const QString &iface, const QString &method,
                   const QVariantList &args,
                   std::function<void(const QList<QVariant> &, const QString &)> done);
    void transceive(const QByteArray &command,
                    std::function<void(const QByteArray &)> ok,
                    std::function<void(const QString &)> err);
    void acquireTag(std::function<void()> next);
    void releaseTag();
    void readUid(std::function<void(const QByteArray &)> ok,
                 std::function<void(const QString &)> err);
    void tryBalanceKeys(int index);
    // Авторизует сектор блока flavor-форматом и читает блок;
    // ok получает данные блока (16 байт NXP / 15 байт ST).
    // err(true) = NXP-стек вернул ПУСТОЙ ответ на auth (признак ST-стека);
    // err(false) = любая другая ошибка.
    void authAndRead(int flavor, bool keyB, char block, const QByteArray &key,
                     std::function<void(const QByteArray &)> ok,
                     std::function<void(bool emptyReply)> err);
    void readCardNumber();

    QString m_adapterPath;
    QString m_tagPath;
    QByteArray m_uid;
    State m_state = Waiting;
    int m_flavor = -1; // формат кадров: flavorNxp/flavorSt, -1 = не определён
    // ST-пробы разрешены только после пустого ответа на NXP-кадр: сырые
    // ST-кадры вешают прошивку NXP-чипа (EIO на /dev/nxpnfc, PN7160)
    bool m_nxpEmptySeen = false;
    bool m_nfcEnabled = true;
    QString m_balanceText;
    QString m_cardNumber;
    QString m_uidText;
    QString m_errorText;
};
