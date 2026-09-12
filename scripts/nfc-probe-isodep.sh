#!/bin/sh
# Контрольный пробник ISO-DEP: SELECT PPSE (2PAY.SYS.DDF01).
# Любая банковская карта должна ответить FCI с SW=90 00 — это подтверждает,
# что стек nfcd/Transceive возвращает данные приложению.
S=org.sailfishos.nfc.daemon

pkill -f "PodorozhnikBalanc[e]" 2>/dev/null
sleep 1

tag=""
i=0
while [ $i -lt 240 ]; do
    tag=$(busctl call $S /nfc0 org.sailfishos.nfc.Adapter GetTags 2>/dev/null \
          | grep -o '/nfc0/[a-zA-Z0-9]*' | head -1)
    [ -n "$tag" ] && break
    i=$((i+1)); sleep 1
done
[ -z "$tag" ] && { echo "TIMEOUT: метка не появилась"; exit 1; }
echo "TAG: $tag"

echo "== interfaces =="
busctl introspect $S $tag 2>&1 | grep -E '^org\.' | head -10

echo "== IsoDep Transmit: SELECT PPSE =="
# CLA=0 INS=A4 P1=04 P2=00 data="2PAY.SYS.DDF01" Le=0
busctl call $S $tag org.sailfishos.nfc.IsoDep Transmit yyyyayu \
    0 164 4 0 14 50 80 65 89 46 83 89 83 46 68 68 70 48 49 0 2>&1
echo "== done =="
