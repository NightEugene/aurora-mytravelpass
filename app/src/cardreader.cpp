#include "cardreader.h"

#include "podorozhnik.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDateTime>
#include <QDebug>
#include <QPair>
#include <QSettings>
#include <QTimer>

namespace {

const QString nfcService  = QStringLiteral("org.sailfishos.nfc.daemon");
const QString daemonPath  = QStringLiteral("/");
const QString daemonIface = QStringLiteral("org.sailfishos.nfc.Daemon");
const QString adapterIface = QStringLiteral("org.sailfishos.nfc.Adapter");
const QString tagIface = QStringLiteral("org.sailfishos.nfc.Tag");
const QString tagClassicIface = QStringLiteral("org.sailfishos.nfc.TagClassic");

const int dbusTimeoutMs = 5000;

// NFC_TAG_TYPE_MIFARE_CLASSIC (nfc_types.h nfcd)
const quint32 tagTypeMifareClassic = 0x02;

// Два семейства NFC-контроллеров с поддержкой MIFARE Classic различаются
// форматом кадров на proprietary RF-интерфейсе NCI (Crypto1 всегда
// выполняет чип, хост nonce не видит). Формат определяется перебором
// (flavorNxp пробуется первым) и кэшируется в m_flavor.
//
// NXP (PN7160, T800), интерфейс 0x80 — команды MfcAuthReq/MfRawDataXchgHdr
// (см. nfcd-mifare-classic/tools/nci-init.c):
//   Auth: {0x40, номер СЕКТОРА, тип ключа, ключ[6]}  → успех {0x40}
//   Read: {0x10, 0x30, блок}                        → {0x10, данные[16]}
//   При ошибке opcode возвращается с битом 0x80 ({0xC0}/{0x90}).
//
// ST (ST21NFC, MP-67A27), интерфейс 0x90 — формат как в AOSP rw_mfc.c:
//   Auth: {0x60/0x61 (A/B), номер БЛОКА, UID[4], ключ[6]} — без CRC;
//         успех = пустой ответ, неверный ключ = ошибка D-Bus. Неудачный
//         auth «отравляет» сессию до ближайшей реактивации метки (цикл
//         Classic reset ~1 Гц), поэтому между попытками — пауза.
//   Read: {0x30, блок} → 15 байт (стек срезает последний байт блока).
const char mfcCmdAuth   = 0x40;
const char mfcKeyTypeA  = 0x10;
const char mfcKeyTypeB  = 0x90;
const char mfcCmdRaw    = 0x10;
const char mfcCmdRead   = 0x30;

const int flavorNxp = 0;
const int flavorSt  = 1;
const char stCmdAuthA = 0x60;
const char stCmdAuthB = 0x61;
// Переждать цикл Classic reset после неудачного auth на ST-стеке
const int retryDelayMs = 1100;

// Абсолютные номера блоков (MIFARE Classic 1K: 4 блока на сектор;
// у NXP-формата адрес в auth — сектор = блок/4)
const char blockBalance  = 16;
const char blockNumber   = 0;

} // namespace

CardReader::CardReader(QObject *parent)
    : QObject(parent)
{
    // История чтений (вкладка «ИЗМЕНЕНИЯ»): «dd.MM.yyyy HH:mm|баланс»
    m_history = QSettings().value(QStringLiteral("history")).toStringList();

    // nfcd может стартовать позже приложения (например, в эмуляторе его нет вовсе)
    auto *watcher = new QDBusServiceWatcher(nfcService, QDBusConnection::systemBus(),
                                            QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered,
            this, &CardReader::onServiceRegistered);
    connectDaemon();
}

void CardReader::onServiceRegistered()
{
    connectDaemon();
}

void CardReader::refresh()
{
    if (m_state == Reading)
        return; // чтение уже идёт

    releaseTag();
    m_balanceText.clear();
    m_balanceKopecks = 0;
    m_lastReadTime.clear();
    m_cardNumber.clear();
    m_uidText.clear();
    m_errorText.clear();
    setState(Waiting);
    emit dataChanged();
    emit errorChanged();
    // Если карта уже у телефона — начать чтение сразу
    onTargetPresentChanged(true);
}

void CardReader::clearHistory()
{
    if (m_history.isEmpty())
        return;
    m_history.clear();
    QSettings().remove(QStringLiteral("history"));
    emit historyChanged();
}

void CardReader::connectDaemon()
{
    const QDBusReply<bool> registered =
            QDBusConnection::systemBus().interface()->isServiceRegistered(nfcService);
    if (!registered.isValid() || !registered.value()) {
        qWarning() << "Служба" << nfcService << "не запущена";
        return;
    }

    asyncCall(daemonPath, daemonIface, QStringLiteral("GetAdapters"), {},
              [this](const QList<QVariant> &out, const QString &error) {
        if (!error.isEmpty()) {
            qWarning() << "GetAdapters:" << error;
            return;
        }
        const auto adapters = qdbus_cast<QList<QDBusObjectPath> >(out.value(0));
        for (const QDBusObjectPath &adapter : adapters)
            connectAdapter(adapter.path());
    });
}

void CardReader::connectAdapter(const QString &adapterPath)
{
    if (!m_adapterPath.isEmpty())
        return; // достаточно первого адаптера
    m_adapterPath = adapterPath;

    QDBusConnection::systemBus().connect(nfcService, adapterPath, adapterIface,
                                         QStringLiteral("TargetPresentChanged"),
                                         this, SLOT(onTargetPresentChanged(bool)));
    QDBusConnection::systemBus().connect(nfcService, adapterPath, adapterIface,
                                         QStringLiteral("PoweredChanged"),
                                         this, SLOT(onPoweredChanged(bool)));

    asyncCall(adapterPath, adapterIface, QStringLiteral("GetPowered"), {},
              [this](const QList<QVariant> &out, const QString &) {
        onPoweredChanged(out.value(0).toBool());
    });

    // Метка могла быть поднесена до запуска приложения
    onTargetPresentChanged(true);
}

void CardReader::onPoweredChanged(bool powered)
{
    if (m_nfcEnabled == powered)
        return;
    m_nfcEnabled = powered;
    emit nfcEnabledChanged();
}

void CardReader::onTargetPresentChanged(bool present)
{
    if (!present || m_adapterPath.isEmpty())
        return;

    asyncCall(m_adapterPath, adapterIface, QStringLiteral("GetTags"), {},
              [this](const QList<QVariant> &out, const QString &error) {
        if (!error.isEmpty()) {
            qWarning() << "GetTags:" << error;
            return;
        }
        const auto tags = qdbus_cast<QList<QDBusObjectPath> >(out.value(0));
        if (!tags.isEmpty())
            readTag(tags.first().path());
    });
}

void CardReader::readTag(const QString &tagPath)
{
    if (m_state == Reading)
        return;

    m_tagPath = tagPath;
    m_uid.clear();
    m_errorText.clear();
    m_nxpEmptySeen = false;
    setState(Reading);

    QDBusConnection::systemBus().connect(nfcService, tagPath, tagIface,
                                         QStringLiteral("Removed"),
                                         this, SLOT(onTagRemoved()));

    asyncCall(tagPath, tagIface, QStringLiteral("GetType"), {},
              [this](const QList<QVariant> &out, const QString &error) {
        if (!error.isEmpty()) {
            fail(tr("Карта недоступна: %1").arg(error));
            return;
        }
        const quint32 type = out.value(0).toUInt();
        if (type != tagTypeMifareClassic) {
            fail(tr("Это не карта MIFARE Classic (тип 0x%1)")
                 .arg(type, 2, 16, QLatin1Char('0')));
            return;
        }
        acquireTag([this] {
            readUid([this](const QByteArray &uid) {
                m_uid = uid;
                qDebug("UID: %s", uid.toHex().constData());
                QStringList hexBytes;
                for (const char byte : uid)
                    hexBytes << QString::number(quint8(byte), 16)
                                    .rightJustified(2, QLatin1Char('0'));
                m_uidText = hexBytes.join(QLatin1Char(':')).toUpper();
                tryBalanceKeys(0);
            }, [this](const QString &uidError) {
                fail(tr("Не удалось получить UID карты: %1").arg(uidError));
            });
        });
    });
}

void CardReader::acquireTag(std::function<void()> next)
{
    // Эксклюзивный доступ, чтобы системные проверки метки не вклинились
    // в последовательность команд (на Classic-метках nfcd периодически
    // сбрасывает метку, что рвёт обмен). В документации ОС Аврора —
    // Acquire(bool), в апстриме nfcd — Acquire() без аргументов.
    asyncCall(m_tagPath, tagIface, QStringLiteral("Acquire"), { false },
              [this, next](const QList<QVariant> &, const QString &error) {
        if (error.isEmpty()) {
            qDebug("Acquire(false): ok");
            next();
            return;
        }
        qDebug() << "Acquire(false):" << error;
        asyncCall(m_tagPath, tagIface, QStringLiteral("Acquire"), {},
                  [next](const QList<QVariant> &, const QString &error2) {
            qDebug() << "Acquire():" << (error2.isEmpty()
                     ? QStringLiteral("ok") : error2);
            next();
        });
    });
}

void CardReader::releaseTag()
{
    if (m_tagPath.isEmpty())
        return;

    const QString path = m_tagPath;
    m_tagPath.clear();
    asyncCall(path, tagIface, QStringLiteral("Release"), {},
              [](const QList<QVariant> &, const QString &) {});
    QDBusConnection::systemBus().disconnect(nfcService, path, tagIface,
                                            QStringLiteral("Removed"),
                                            this, SLOT(onTagRemoved()));
}

void CardReader::readUid(std::function<void(const QByteArray &)> ok,
                         std::function<void(const QString &)> err)
{
    // Основной путь — интерфейс TagClassic (UID метки MIFARE Classic)
    asyncCall(m_tagPath, tagClassicIface, QStringLiteral("GetSerial"), {},
              [this, ok, err](const QList<QVariant> &out, const QString &error) {
        const QByteArray uid = out.value(0).toByteArray();
        if (error.isEmpty() && uid.size() >= 4) {
            ok(uid);
            return;
        }
        // Запасной путь — низкоуровневые параметры метки (NFCID1)
        asyncCall(m_tagPath, tagIface, QStringLiteral("GetPollParameters"), {},
                  [ok, err](const QList<QVariant> &out2, const QString &error2) {
            if (!error2.isEmpty()) {
                err(error2);
                return;
            }
            const auto params = qdbus_cast<QVariantMap>(out2.value(0));
            const QByteArray uid = params.value(QStringLiteral("NFCID1")).toByteArray();
            if (uid.size() >= 4)
                ok(uid);
            else
                err(QStringLiteral("UID не найден"));
        });
    });
}

void CardReader::tryBalanceKeys(int index)
{
    if (m_state != Reading)
        return; // карта убрана, пока ждали реактивацию

    // Комбинации (формат кадра × ключ). Пока формат стека не определён,
    // перебираем оба; после первого успеха используется только он.
    // ST-кадры посылаются только если NXP-стек ответил на auth ПУСТЫМ
    // ответом (признак ST: настоящий NXP-чип всегда отвечает {0x40}/{0xC0}
    // либо ошибкой) — сырые ST-кадры вешают прошивку PN7160
    // (EIO на /dev/nxpnfc, NFC отваливается до перезапуска nfcd).
    QList<QPair<int, bool> > combos;
    if (m_flavor >= 0) {
        combos << qMakePair(m_flavor, false) << qMakePair(m_flavor, true);
    } else {
        combos << qMakePair(flavorNxp, false) << qMakePair(flavorNxp, true);
        if (m_nxpEmptySeen)
            combos << qMakePair(flavorSt, false) << qMakePair(flavorSt, true);
    }
    if (index >= combos.size()) {
        fail(tr("Не удалось авторизовать сектор баланса: ключ не подошёл"));
        return;
    }

    const int flavor = combos.at(index).first;
    const bool keyB = combos.at(index).second;
    authAndRead(flavor, keyB, blockBalance,
                keyB ? Podorozhnik::sector4KeyB : Podorozhnik::sector4KeyA,
                [this, flavor](const QByteArray &block) {
        m_flavor = flavor;
        m_balanceKopecks = Podorozhnik::balanceFromBlock(block);
        m_balanceText = Podorozhnik::formatBalance(m_balanceKopecks);
        qDebug() << "Баланс:" << m_balanceText
                 << "(формат" << (flavor == flavorSt ? "ST" : "NXP") << ")";
        readCardNumber();
    }, [this, index](bool emptyReply) {
        if (emptyReply)
            m_nxpEmptySeen = true;
        // На ST-стеке неудачный auth ломает сессию до реактивации метки —
        // пережидаем цикл Classic reset; на NXP пауза безвредна.
        QTimer::singleShot(retryDelayMs, this, [this, index] {
            tryBalanceKeys(index + 1);
        });
    });
}

// Авторизует сектор блока и читает сам блок. ok получает данные блока
// (16 байт на NXP, 15 на ST — последний байт стек ST срезает).
// err(true) сигнализирует о пустом ответе на NXP-auth (см. tryBalanceKeys).
void CardReader::authAndRead(int flavor, bool keyB, char block, const QByteArray &key,
                             std::function<void(const QByteArray &)> ok,
                             std::function<void(bool)> err)
{
    QByteArray auth;
    if (flavor == flavorNxp) {
        auth.append(mfcCmdAuth);
        auth.append(char(block / 4)); // у NXP адрес = номер СЕКТОРА
        auth.append(keyB ? mfcKeyTypeB : mfcKeyTypeA);
    } else {
        auth.append(keyB ? stCmdAuthB : stCmdAuthA);
        auth.append(block);           // у ST адрес = номер БЛОКА
        auth.append(m_uid.right(4));
    }
    auth.append(key);

    transceive(auth, [this, flavor, block, ok, err](const QByteArray &resp) {
        if (flavor == flavorNxp &&
                (resp.isEmpty() || quint8(resp.at(0)) != quint8(mfcCmdAuth))) {
            err(resp.isEmpty());
            return;
        }
        // У ST успешный auth — пустой ответ; что auth действительно прошёл,
        // подтвердит чтение (при неверном ключе чтение даёт ошибку).
        QByteArray read;
        if (flavor == flavorNxp)
            read.append(mfcCmdRaw);
        read.append(mfcCmdRead);
        read.append(block);
        transceive(read, [flavor, ok, err](const QByteArray &resp) {
            if (flavor == flavorNxp) {
                if (resp.size() >= 17 && quint8(resp.at(0)) == quint8(mfcCmdRaw)) {
                    ok(resp.mid(1, 16));
                    return;
                }
            } else if (resp.size() >= 15) {
                ok(resp.left(16));
                return;
            }
            err(false);
        }, [err](const QString &) {
            err(false);
        });
    }, [err](const QString &) {
        err(false);
    });
}

void CardReader::readCardNumber()
{
    // Номер карты — необязательные данные: любая ошибка здесь не фатальна
    authAndRead(m_flavor, false, blockNumber, Podorozhnik::keyDefault,
                [this](const QByteArray &block) {
        m_cardNumber = Podorozhnik::cardNumberFromBlock0(block);
        finishOk();
    }, [this](bool) {
        finishOk();
    });
}

void CardReader::finishOk()
{
    releaseTag();
    m_errorText.clear();

    const QDateTime now = QDateTime::currentDateTime();
    m_lastReadTime = now.toString(QStringLiteral("HH:mm"));
    m_history.prepend(now.toString(QStringLiteral("dd.MM.yyyy HH:mm"))
                      + QLatin1Char('|') + m_balanceText);
    while (m_history.size() > 20)
        m_history.removeLast();
    QSettings().setValue(QStringLiteral("history"), m_history);
    emit historyChanged();

    setState(Result);
    emit dataChanged();
}

void CardReader::fail(const QString &message)
{
    qWarning() << message;
    releaseTag();
    m_errorText = message;
    setState(Error);
    emit errorChanged();
}

void CardReader::onTagRemoved()
{
    if (m_state == Reading)
        fail(tr("Карта убрана слишком рано — держите её у телефона до конца чтения"));
}

void CardReader::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void CardReader::transceive(const QByteArray &command,
                            std::function<void(const QByteArray &)> ok,
                            std::function<void(const QString &)> err)
{
    if (m_tagPath.isEmpty()) {
        err(tr("метка отключена"));
        return;
    }
    asyncCall(m_tagPath, tagIface, QStringLiteral("Transceive"),
              { QVariant::fromValue(command) },
              [command, ok, err](const QList<QVariant> &out, const QString &error) {
        if (!error.isEmpty()) {
            qDebug("TX %s -> ERR %s", command.toHex().constData(),
                   error.toUtf8().constData());
            err(error);
            return;
        }
        const QByteArray response = out.value(0).toByteArray();
        qDebug("TX %s -> %s", command.toHex().constData(),
               response.toHex().constData());
        ok(response);
    });
}

void CardReader::asyncCall(const QString &path, const QString &iface, const QString &method,
                           const QVariantList &args,
                           std::function<void(const QList<QVariant> &, const QString &)> done)
{
    QDBusMessage message = QDBusMessage::createMethodCall(nfcService, path, iface, method);
    message.setArguments(args);

    auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::systemBus().asyncCall(message, dbusTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [done](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage)
            done({}, reply.errorMessage());
        else
            done(reply.arguments(), QString());
    });
}
