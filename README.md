# Подорожник — приложение для ОС Аврора

Приложение показывает баланс проездного «Подорожник» (Санкт-Петербург),
когда карту прикладывают к телефону по NFC. Карта работает по технологии
MIFARE Classic (Crypto1); её поддержка в ОС Аврора ограничена, поэтому
вторым компонентом проекта ведётся доработка NFC-стека (nfcd).

<p align="center">
  <img src="screenshots/app-waiting.png" width="200"
       alt="Ожидание карты" />
  <img src="screenshots/app-balance.png" width="200"
       alt="Баланс и счётчики поездок" />
  <img src="screenshots/app-history.png" width="200"
       alt="История чтений" />
  <img src="screenshots/app-info.png" width="200"
       alt="Информация о карте" />
</p>

## Структура репозитория

- `app/` — приложение для ОС Аврора (Qt5/QML, Silica),
  пакет `ru.nighteugene.PodorozhnikBalance`.
- `nfcd-mifare-classic/` — поддержка MIFARE Classic в nfcd: патчи на
  `sailfishos/nfcd` и `mer-hybris/libnciplugin`, скрипт получения исходников,
  инструкция по сборке в Platform SDK.
- `scripts/aurora_mcp.py` — CLI-клиент MCP-сервера developer.auroraos.ru
  (поиск по документации и примерам кода), используется при разработке.
- `scripts/gen_icons.py` — генератор иконок приложения (без зависимостей).

## Как это работает

1. Приложение слушает системную шину D-Bus: `org.sailfishos.nfc.Adapter`
   (сигнал `TargetPresentChanged`, метод `GetTags`).
2. У найденной метки проверяет `org.sailfishos.nfc.Tag.GetType()`
   == `MIFARE_CLASSIC` (0x02).
3. Через `org.sailfishos.nfc.TagClassic.GetSerial()` получает UID и далее
   методом `Tag.Transceive()` отправляет команды проприетарного протокола
   контроллера (Crypto1 всегда выполняет чип; формат определяется перебором
   и кэшируется, см. `app/src/cardreader.cpp`):
   - NXP: `MfcAuthReq {0x40, сектор, тип ключа, ключ}` — авторизация,
     `{0x10, 0x30, блок}` — чтение блока (16 байт);
   - ST (как в AOSP `rw_mfc.c`, без CRC): `auth {0x60/0x61, блок, UID[4], ключ}`
     (успех = пустой ответ), `{0x30, блок}` — чтение (15 байт).
4. Сектор 4, блок 16, байты 0–3: баланс, uint32 LE, в копейках
   (ключ A `E56AC127DD45`, запасной ключ B `19FC84A3784B`).
   Печатный номер карты — из блока 0 (ключ по умолчанию `FFFFFFFFFFFF`):
   `«9643 3078 » + 7-байтовый LE номер + контрольная цифра Луна`.

Раскладка и ключи подтверждены независимыми открытыми источниками:
[metrodroid](https://github.com/metrodroid/metrodroid),
[Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware)
(`applications/main/nfc/plugins/supported_cards/plantain.c`),
[plantain_parser](https://github.com/rozetkinrobot/plantain_parser).

## Сборка и установка приложения

Сборка — через Aurora SDK (BT): `apptool` в Docker-образе Build Tools
(в этом образе Qt 5.6, учитывайте при написании кода):

```sh
cd app
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/sources -w /sources \
    aurora-build-tools-<пользователь>:5.2.1.200 apptool build
# Пакеты (aarch64, armv7hl, x86_64), подписанные и проверенные rpm-validator'ом,
# появятся в app/RPMS/
```

Установка на устройство (не через `rpm -i` — Аврора ставит сторонние пакеты
только через APM; `sdk-deploy-rpm` — обёртка над ним). Версию пакета при
каждой итерации поднимайте в `rpm/*.spec` — APM откажется ставить ту же версию:

```sh
scp app/RPMS/ru.nighteugene.PodorozhnikBalance-<версия>.aarch64.rpm defaultuser@<device>:/tmp/
ssh defaultuser@<device> 'sdk-deploy-rpm /tmp/ru.nighteugene.PodorozhnikBalance-<версия>.aarch64.rpm'
```

## Проверено на устройствах (сентябрь 2026)

**T800 (NXP PN7160, ОС Аврора 5.1.4) — работает.** Приложение читает
баланс обеих тестовых карт (MIFARE Classic 1K и 4K) со стандартными
ключами «Подорожника». Так как вендорский HAL отвергает MIFARE-кадры,
используется обходной путь из `nfcd-mifare-classic/`: nfcd работает через
pn54x-плагин напрямую с `/dev/nxpnfc`, чип конфигурирует `nci-init`
(aarch64) из systemd drop-in. См. `nfcd-mifare-classic/README.md`.

**Fplus MP-67A27 (MediaTek Helio G99, NFCC ST21NFC, Аврора 5.2) — тоже
работает**, причём на стоковом nfcd (компонент `nfcd-mifare-classic` не
нужен): ST лицензировала Crypto1 у NXP, его исполняет прошивка чипа.
Метка активируется с proprietary RF-интерфейсом ST `0x90`, кадры — в
формате AOSP `rw_mfc.c` (см. выше). Air-кадры с CRC_A стек ST молча
отвергает — поэтому первоначальный вывод «только UID» был ошибочным.

Итог: для чтения «Подорожника» нужен NFC-контроллер с Crypto1 — NXP
(PN54x/PN71xx/SN1xx/SN2xx, плюс компонент `nfcd-mifare-classic`) либо
STMicroelectronics поколения ST21NFCD/ST54 (стоковый стек). Контроллеры
без Crypto1 (Broadcom и др.) читают только UID.


## MCP-сервер Авроры при разработке

Сервер `https://developer.auroraos.ru/api/mcp` (имя сервера — `dev-aurora`,
аутентификация не требуется) — официальный источник документации и примеров:

```sh
python3 scripts/aurora_mcp.py tools
python3 scripts/aurora_mcp.py call search '{"query":"nfcd Tag Transceive","index":"docs"}'
python3 scripts/aurora_mcp.py call get_document \
    '{"path":"doc/software_development/reference/communication/nfcd/tag","index":"docs"}'
```

## Лицензия

Код проекта — BSD-3-Clause (см. [LICENSE](LICENSE)). Патчи в
`nfcd-mifare-classic/patches/` — сторонние изменения из открытых pull request'ов
(см. `nfcd-mifare-classic/README.md`), на них действуют лицензии исходных проектов.
