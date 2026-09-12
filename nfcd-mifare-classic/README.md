# Поддержка MIFARE Classic в nfcd

В ОС Аврора поддержка MIFARE Classic в NFC-стеке ограничена: системный форк
nfcd от ОМП распознаёт такие метки и отдаёт их UID через D-Bus-интерфейс
`org.sailfishos.nfc.TagClassic`, но чтение секторов приложением — через
`org.sailfishos.nfc.Tag.Transceive()` — зависит от NFC-контроллера и его
вендорского HAL.

Этот компонент — рабочее решение для устройств с контроллером **NXP PN7160**
(проверено на T800, Аврора 5.1.4): чтение секторов MIFARE Classic из
приложения через штатный D-Bus API nfcd.

## Как это устроено (T800 / PN7160)

Вендорский Android-HAL (`android.hardware.nfc@1.2-service`) отвергает
MIFARE-кадры, поэтому стек идёт в обход него:

- `src/nfcd-pn54x-plugin` (mer-hybris, 1.0.3 + патч `patches/nfcd-pn54x-plugin-t800-power-ioctl.patch`) —
  плагин nfcd, говорящий с чипом напрямую через `/dev/nxpnfc` (сырой NCI).
- `tools/nci-init.c` (aarch64, статик) — включение чипа и загрузка
  вендорской NCI-конфигурации из `/vendor/etc/libnfc-nxp.conf`.
  Питание чипа — 64-битный ioctl `0x4008E901`; у драйвера нет
  `.compat_ioctl`, поэтому 32-битные вызовы получают ENOTTY, а nci-init
  собран именно под aarch64. Запускается из systemd drop-in
  (`ExecStartPre=+...nci-init smoke`) перед nfcd.
- `src/libncicore` (1.1.23 + `patches/libncicore-mifare-proprietary-map.patch`) —
  в discovery-map добавлен `PROPRIETARY/Poll/PROPRIETARY` (0x80/0x80):
  без этого метка активируется как T2T и Crypto1 недоступен.

## Протокол обмена с картой (важно!)

PN7160 реализует MIFARE Classic через проприетарные команды NXP в NCI DATA:

- **Авторизация** `MfcAuthReq`: `{0x40, номер_СЕКТОРА, тип_ключа, ключ[6]}`.
  Тип: `0x10` = Key A, `0x90` = Key B. **Адрес — номер сектора, а не блока**
  (ошибка адресации блоком стоила многих часов: с блоком 0 она не видна).
  Ответ: `{0x40, статус}`.
- **Чтение** `MfRawDataXchgHdr`: `{0x10, 0x30, номер_блока}`.
  Ответ: `{0x10, данные[16], статус}`.
- Кадры данных чип принимает с **conn id 0** (байт 0 заголовка NCI DATA);
  байт 1 игнорируется. С корректным динамическим conn id из
  RF_INTF_ACTIVATED_NTF (`01 00 …`) чип отвечает ошибкой `{0xC0, 0x83}`.
- Приложению через `Transceive` ответы приходят со **срезанным байтом
  статуса** (так делает finish в libnciplugin): `{0x40}` — auth ok,
  `{0x10, данные[16]}` — прочитанный блок. При ошибке opcode приходит
  с битом 0x80 (`{0xC0}`, `{0x90}`).
- После неудачной авторизации карта замолкает до re-select
  (RF_DEACTIVATE → повторная активация).

## Сборка и установка

```sh
nfcd-mifare-classic/scripts/build-pn54x-plugin.sh
```

Собирает в Docker-образе Аврора BT: `pn54x.so` (armv7hl), `nci-init`
(aarch64), патченую `libncicore.so.1.1.23` и пакует в подписанный RPM
(payload в `/usr/share/nfcd-pn54x-plugin/` — APM при установке проставляет
файлам `security.ima`, без которого IMA не даст их исполнять/маппить).
Версию в скрипте (`VERSION=`) поднимать каждую итерацию — APM отказывает
ту же версию.

Установка и включение на устройстве (root):

```sh
ssh defaultuser@<device> 'sdk-deploy-rpm --silent /tmp/nfcd-pn54x-plugin-*.rpm'
# далее под root:
mv /usr/lib/nfcd/plugins/binder.so /root/binder.so.bak   # убрать binder-адаптер
ln -sf /usr/share/nfcd-pn54x-plugin/pn54x.so /usr/lib/nfcd/plugins/pn54x.so
ln -sf /usr/share/nfcd-pn54x-plugin/libncicore.so.1.1.23 /usr/lib/libncicore.so.1
mkdir -p /etc/systemd/system/nfcd.service.d
printf '[Service]\nExecStartPre=+/usr/share/nfcd-pn54x-plugin/nci-init smoke\n' \
    > /etc/systemd/system/nfcd.service.d/pn54x.conf
systemctl daemon-reload && systemctl restart nfcd
```

Вендорский HAL при этом может остаться запущенным — он открывает
`/dev/nxpnfc` только по запросу binder-плагина, а тот убран.

## Диагностика: nci-init

`nci-init [on|off|smoke|mfctest|keyprobe|authall|fftest|seqtest]`:

- `smoke` (по умолчанию) — VEN-цикл, CORE_RESET/INIT, вендорская конфигурация;
- `authall` — прогон таблицы ключей «Подорожника» по приложенной карте;
- `mfctest` — то же + перебор ключей сектора 4 из `/tmp/mfc_keys.txt`;
- `keyprobe`/`fftest`/`seqtest` — форматы MfcAuthReq, честность прошивки,
  секторная адресация (история отладки).

Если nci-init «зависает»: `read()` этого драйвера блокируется и не
прерывается сигналами; poll() бесполезен (данные армируются только read'ом).
Восстановление драйвера после убитого посреди обмена процесса:
`setprop ctl.start vendor.nfc_hal_service; sleep 5; setprop ctl.stop vendor.nfc_hal_service`.

## Известные особенности

- **«Classic reset»**: nfcd ОМП для меток Classic делает цикл
  deactivate→reactivate примерно раз в секунду (детект изъятия карты).
  Цикл рвёт Crypto1-сессию между транзакциями приложения; на практике
  чтение баланса (4 коротких Transceive) успевает между сбросами,
  но возможны гонки — лечится повторным поднесением.
- **Один экземпляр приложения**: при двух одновременно запущенных
  экземплярах (например, ручной запуск через invoker параллельно с иконкой)
  гонки по Transceive могут вернуть пустой ответ на NXP-кадр, открыть
  ST-пробу и завесить прошивку PN7160 (EIO на /dev/nxpnfc; лечится
  `systemctl restart nfcd`). Штатный запуск с иконки одноэкземплярен —
  lipstick фокусирует уже открытое окно.
- **IMA**: неподписанные бинарники не запускаются и не маппятся.
  Поэтому всё ставится только RPM'ом через APM.
- Вторая машина: MP-67A27 (Fplus, Helio G99, NFCC **ST21NFC**) — MIFARE
  Classic тоже работает (приложение 1.0.0 читает баланс обеих карт), но
  формат кадров другой. ST лицензировала Crypto1 у NXP для поколения
  ST21NFCD/ST54 (на нём же Pixel 6/7 с рабочим MIFARE Classic Tool);
  Crypto1 исполняет прошивка чипа. Метка активируется с проприетарным
  RF-протоколом/интерфейсом ST `0x90` (в `/vendor/etc/libnfc-hal-st.conf`:
  `NFA_PROPRIETARY_CFG` byte[5] = `NCI_PROTOCOL_MIFARE` = 0x90). Кадры —
  как в AOSP `rw_mfc.c`, БЕЗ CRC (его добавляет чип):
  auth `{0x60/0x61 (A/B), номер БЛОКА, UID[4], ключ[6]}` (12 байт; успех =
  пустой ответ, неверный ключ = ошибка; неудачный auth «отравляет» сессию
  до ближайшей реактивации метки), read `{0x30, блок}` → 15 байт (стек
  срезает последний байт блока). Air-кадры (`60 xx CRC_A`) стек ST
  отвергает молча — поэтому первоначальный вывод «только UID» был неверен:
  мы слали эфирный формат вместо хост-формата.
- Отличие адресации: у NXP в auth — номер СЕКТОРА, у ST — номер БЛОКА.

## Апстрим-патчи (справочно)

- `patches/nfcd-pr31-mifare-classic.patch` — [sailfishos/nfcd PR #31](https://github.com/sailfishos/nfcd/pull/31):
  `NFC_PROTOCOL_MIFARE_CLASSIC` и `nfc_adapter_add_tag_mifare_classic()`
  (в nfcd ОМП это уже есть).
- `patches/libnciplugin-pr19-mifare-classic.patch` — [mer-hybris/libnciplugin PR #19](https://github.com/mer-hybris/libnciplugin/pull/19):
  распознавание активации `PROPRIETARY/PROPRIETARY` как MIFARE Classic
  (в libnciplugin ОМП тоже уже есть; заменять системную библиотеку не
  потребовалось — достаточно discovery-map в libncicore).
- `scripts/fetch-nfc-stack.sh` — клонирует `nfcd`, `libncicore`,
  `libnciplugin`, `nfcd-binder-plugin`, `nfcd-pn54x-plugin` в `src/`
  (не чистить: клоны содержат рабочие правки, патчи в `patches/`).

## Требования

- NFC-контроллер с Crypto1: NXP (PN544/PN547/PN553/PN7150/PN7160/SN100 и т.п.)
  либо STMicroelectronics поколения ST21NFCD/ST54/ST54L (Crypto1 лицензирован,
  исполняется прошивкой; формат кадров отличается, см. «Известные особенности»).
  На контроллерах без Crypto1 (Broadcom и др.) MIFARE Classic читается
  только на уровне UID — ограничение железа, а не ПО.
- Aurora SDK (BT-образ `aurora-build-tools`) для сборки; ключи подписи
  пакетов в `~/.local/share/aurora-sdk/package-signing/`.
- Устройство с режимом разработчика (доступ по SSH, root по ключу).
