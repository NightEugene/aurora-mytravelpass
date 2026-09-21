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
    Q_PROPERTY(Kind cardKind READ cardKind NOTIFY dataChanged)
    Q_PROPERTY(QString cardTypeName READ cardTypeName NOTIFY dataChanged)
    Q_PROPERTY(bool nfcEnabled READ nfcEnabled NOTIFY nfcEnabledChanged)
    Q_PROPERTY(QString balanceText READ balanceText NOTIFY dataChanged)
    Q_PROPERTY(quint32 balanceKopecks READ balanceKopecks NOTIFY dataChanged)
    Q_PROPERTY(QString lastReadTime READ lastReadTime NOTIFY dataChanged)
    Q_PROPERTY(QString cardNumber READ cardNumber NOTIFY dataChanged)
    Q_PROPERTY(QString uidText READ uidText NOTIFY dataChanged)
    Q_PROPERTY(QString lastTripWhen READ lastTripWhen NOTIFY dataChanged)
    Q_PROPERTY(QString lastTripFare READ lastTripFare NOTIFY dataChanged)
    Q_PROPERTY(QString lastTripTransport READ lastTripTransport NOTIFY dataChanged)
    Q_PROPERTY(bool lastTripIsMetro READ lastTripIsMetro NOTIFY dataChanged)
    Q_PROPERTY(QString lastTopupWhen READ lastTopupWhen NOTIFY dataChanged)
    Q_PROPERTY(QString lastTopupAmount READ lastTopupAmount NOTIFY dataChanged)
    Q_PROPERTY(int subwayTrips READ subwayTrips NOTIFY dataChanged)
    Q_PROPERTY(int groundTrips READ groundTrips NOTIFY dataChanged)
    Q_PROPERTY(QString tripsPeriod READ tripsPeriod NOTIFY dataChanged)
    Q_PROPERTY(int passDaysLeft READ passDaysLeft NOTIFY dataChanged)
    Q_PROPERTY(QString passExpiryText READ passExpiryText NOTIFY dataChanged)
    Q_PROPERTY(QString passRides READ passRides NOTIFY dataChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)

public:
    enum State { Waiting, Reading, Result, Error };
    Q_ENUM(State)

    // Распознанный тип карты (KindUnknown — карта ещё не читалась)
    enum Kind { KindUnknown, KindPodorozhnik, KindTroika };
    Q_ENUM(Kind)

    explicit CardReader(QObject *parent = nullptr);

    // Сбросить результат и прочитать карту заново (кнопка «Обновить»)
    Q_INVOKABLE void refresh();
    // Очистить историю чтений (кнопка с корзиной)
    Q_INVOKABLE void clearHistory();

    State state() const { return m_state; }
    Kind cardKind() const { return m_cardKind; }
    QString cardTypeName() const;
    bool nfcEnabled() const { return m_nfcEnabled; }
    QString balanceText() const { return m_balanceText; }
    quint32 balanceKopecks() const { return m_balanceKopecks; }
    QString lastReadTime() const { return m_lastReadTime; }
    QString cardNumber() const { return m_cardNumber; }
    QString uidText() const { return m_uidText; }
    QString lastTripWhen() const { return m_lastTripWhen; }
    QString lastTripFare() const { return m_lastTripFare; }
    QString lastTripTransport() const { return m_lastTripTransport; }
    bool lastTripIsMetro() const { return m_lastTripIsMetro; }
    QString lastTopupWhen() const { return m_lastTopupWhen; }
    QString lastTopupAmount() const { return m_lastTopupAmount; }
    int subwayTrips() const { return m_subwayTrips; }
    int groundTrips() const { return m_groundTrips; }
    QString tripsPeriod() const { return m_tripsPeriod; }
    int passDaysLeft() const { return m_passDaysLeft; }
    QString passExpiryText() const { return m_passExpiryText; }
    QString passRides() const { return m_passRides; }
    QString errorText() const { return m_errorText; }
    QStringList history() const { return m_history; }

signals:
    void stateChanged();
    void nfcEnabledChanged();
    void dataChanged();
    void errorChanged();
    void historyChanged();

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
    // uidLeft: в ST-auth — первые 4 байта UID (binder-HAL MediaTek)
    // или последние (ST21NFC). err(true) = NXP-auth отвергнут стеком
    // (пустой ответ, мусор или D-Bus-ошибка); err(false) = прочее.
    void authAndRead(int flavor, bool keyB, bool uidLeft, char block,
                     const QByteArray &key,
                     std::function<void(const QByteArray &)> ok,
                     std::function<void(bool nxpRejected)> err);
    void readCardNumber();
    // Дальнейшие блоки читаются цепочкой после номера карты; любая ошибка
    // здесь не фатальна — обрываем цепочку и показываем, что есть
    void readTopup();
    void readTripBlocks(int step, bool keyB, const QByteArray &tripBlock = QByteArray(),
                        const QByteArray &counterBlock1 = QByteArray(),
                        const QByteArray &counterBlock2 = QByteArray());
    // Билетная зона «Единого» (сектора 8-12): читаются после поездок,
    // ошибки не фатальны. s8b0/s8b1/s9b0/s11b0 — накопленные блоки
    // секторов 8, 9 и 11 (известные поля по plantain_parser и дампу БСК)
    void readPassBlocks(int step, int failCount = 0, const QByteArray &s8b0 = QByteArray(),
                        const QByteArray &s8b1 = QByteArray(),
                        const QByteArray &s9b0 = QByteArray(),
                        const QByteArray &s11b0 = QByteArray());
    // Тройка: auth сектора 8 + проверка магии записи, затем дочитка
    // блоков 33-34 и разбор 48-байтовой записи (troika.cpp)
    void tryTroikaKeys(int index);
    void readTroikaBlocks(int step, const QByteArray &record = QByteArray());

    QString m_adapterPath;
    QString m_tagPath;
    QByteArray m_uid;
    State m_state = Waiting;
    Kind m_cardKind = KindUnknown;
    int m_flavor = -1; // формат кадров: flavorNxp/flavorSt, -1 = не определён
    // ST-пробы разрешены только после отвергнутого NXP-auth (пустой ответ
    // или D-Bus-ошибка): сырые ST-кадры вешают прошивку NXP-чипа
    // (EIO на /dev/nxpnfc, PN7160)
    bool m_nxpRejectedSeen = false;
    bool m_uidLeft = false; // в ST-auth: первые 4 байта UID (иначе последние)
    bool m_nfcEnabled = true;
    QString m_balanceText;
    quint32 m_balanceKopecks = 0;
    QString m_lastReadTime;
    QString m_cardNumber;
    QString m_uidText;
    QString m_lastTripWhen;
    QString m_lastTripFare;
    QString m_lastTripTransport;
    bool m_lastTripIsMetro = false;
    QString m_lastTopupWhen;
    QString m_lastTopupAmount;
    int m_subwayTrips = 0;
    int m_groundTrips = 0;
    QString m_tripsPeriod;
    int m_passDaysLeft = -1; // дней до конца проездного; -1 — нет/истёк
    QString m_passExpiryText;  // «12.10.2026» — дата окончания проездного
    QString m_passRides;
    QString m_errorText;
    QStringList m_history;
};
