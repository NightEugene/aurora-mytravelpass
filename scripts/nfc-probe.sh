#!/bin/sh
# Диагностический пробник MIFARE Classic через busctl на устройстве ОС Аврора.
# Ждёт появления метки (до 240 с), затем прогоняет кадры auth/read через
# org.sailfishos.nfc.Tag.Transceive и печатает все ответы.
#
# Использование: scp на устройство, затем `sh nfc-probe.sh` и приложить карту.
S=org.sailfishos.nfc.daemon

pkill -f MyTravelPass 2>/dev/null
sleep 1

tag=""
i=0
while [ $i -lt 240 ]; do
    tag=$(busctl call $S /nfc0 org.sailfishos.nfc.Adapter GetTags 2>/dev/null \
          | grep -o '/nfc0/[a-zA-Z0-9]*' | head -1)
    [ -n "$tag" ] && break
    i=$((i+1))
    sleep 1
done
if [ -z "$tag" ]; then
    echo "TIMEOUT: метка не появилась за 240 с"
    exit 1
fi
echo "TAG: $tag"

echo "== interfaces =="
busctl introspect $S $tag 2>&1 | grep -E '^org\.|Transceive|GetSerial' | head -15

echo "== GetType =="
busctl call $S $tag org.sailfishos.nfc.Tag GetType 2>&1

echo "== GetSerial (TagClassic) =="
ser=$(busctl call $S $tag org.sailfishos.nfc.TagClassic GetSerial 2>&1)
echo "$ser"
n=$(echo "$ser" | awk '{print $2}')
uid=$(echo "$ser" | cut -d' ' -f3-)
set -- $uid
case "${n:-0}" in
    ''|*[!0-9]*) echo "UID не распознан"; exit 1 ;;
esac
[ "$n" -ge 7 ] && shift $((n-4))
echo "UID для auth (4 байта): $1 $2 $3 $4"

echo "== Acquire =="
busctl call $S $tag org.sailfishos.nfc.Tag Acquire b false 2>&1

echo "== auth сектор 4 Key A (без CRC): 60 10 E56AC127DD45 uid =="
busctl call $S $tag org.sailfishos.nfc.Tag Transceive ay 12 96 16 229 106 193 39 221 69 $1 $2 $3 $4 2>&1

echo "== read блок 16 (без CRC): 30 10 =="
busctl call $S $tag org.sailfishos.nfc.Tag Transceive ay 2 48 16 2>&1

echo "== read блок 16 (с CRC 83 B8): 30 10 83 B8 =="
busctl call $S $tag org.sailfishos.nfc.Tag Transceive ay 4 48 16 131 184 2>&1

echo "== auth сектор 4 Key B (без CRC): 61 10 19FC84A3784B uid =="
busctl call $S $tag org.sailfishos.nfc.Tag Transceive ay 12 97 16 25 252 132 163 120 75 $1 $2 $3 $4 2>&1

echo "== read блок 16 после Key B =="
busctl call $S $tag org.sailfishos.nfc.Tag Transceive ay 2 48 16 2>&1

echo "== Release =="
busctl call $S $tag org.sailfishos.nfc.Tag Release 2>&1
echo "== done =="
