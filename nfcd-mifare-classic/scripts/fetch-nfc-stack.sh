#!/bin/sh
# Клонирует компоненты NFC-стека Sailfish/mer-hybris в src/ и применяет
# патчи поддержки MIFARE Classic из patches/.
#
# Использование: nfcd-mifare-classic/scripts/fetch-nfc-stack.sh
set -e

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SRC="$ROOT/src"
mkdir -p "$SRC"

clone() {
    if [ -d "$SRC/$2" ]; then
        echo "== $2 уже склонирован, пропускаю"
    else
        git clone "$1" "$SRC/$2"
    fi
}

clone https://github.com/sailfishos/nfcd.git nfcd
clone https://github.com/mer-hybris/libncicore.git libncicore
clone https://github.com/mer-hybris/libnciplugin.git libnciplugin
clone https://github.com/mer-hybris/nfcd-binder-plugin.git nfcd-binder-plugin

apply() {
    repo="$1"
    patch="$2"
    echo "== $repo: применяю $(basename "$patch")"
    if git -C "$SRC/$repo" apply --check "$patch" 2>/dev/null; then
        git -C "$SRC/$repo" apply "$patch"
        echo "   ok"
    else
        echo "!! Патч не накладывается на текущий master $repo." >&2
        echo "   Возможно, изменения уже приняты в апстрим или master ушёл вперёд." >&2
        echo "   Проверьте статус PR и при необходимости отребейзьте патч." >&2
    fi
}

apply nfcd "$ROOT/patches/nfcd-pr31-mifare-classic.patch"
apply libnciplugin "$ROOT/patches/libnciplugin-pr19-mifare-classic.patch"

echo "Готово. Дальше — сборка в Aurora Platform SDK, см. README.md"
