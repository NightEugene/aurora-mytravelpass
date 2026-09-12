#!/bin/sh
# Собирает nfcd-pn54x-plugin (транспорт /dev/nxpnfc, прямой NCI) под armv7hl
# в Docker-образе Аврора BT и упаковывает в RPM для установки через APM.
#
# Плагин кладётся в /usr/share/nfcd-pn54x-plugin/: APM при установке
# проставляет файлам security.ima, после чего на устройстве делается
# symlink /usr/lib/nfcd/plugins/pn54x.so -> /usr/share/nfcd-pn54x-plugin/pn54x.so
# (см. README.md, раздел про установку).
#
# Требует в stage/ и src/:
#   stage/hdr/include/*.h        — заголовки libncicore 1.1.23 + libnciplugin 1.1.5
#   stage/hdr/core/include/*.h   — заголовки nfcd 1.1.15 (core/include)
#   stage/lib/*.so*              — библиотеки с целевого устройства
#   src/nfcd-pn54x-plugin/       — исходники плагина (1.0.3 + наши патчи)
#   src/libglibutil/include/*.h  — заголовки libglibutil
#
# Использование: nfcd-mifare-classic/scripts/build-pn54x-plugin.sh
set -e

VERSION=1.0.37
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
IMAGE=$(docker images --format '{{.Repository}}:{{.Tag}}' | grep aurora-build-tools | head -1)
[ -n "$IMAGE" ] || { echo "Образ aurora-build-tools не найден" >&2; exit 1; }
KEYS="$HOME/.local/share/aurora-sdk/package-signing"

docker run --rm -i -e HOME=/tmp -e VERSION="$VERSION" \
    -v "$ROOT":/work -v "$KEYS":/keys:ro -w /work "$IMAGE" sh -ex <<'EOF'
SR=/opt/cross/armv7hl-meego-linux-gnueabi/sys-root
PCDIR=$SR/usr/lib/pkgconfig

# Заголовки
mkdir -p $SR/usr/include/ncicore $SR/usr/include/nciplugin \
         $SR/usr/include/nfcd $SR/usr/include/gutil $PCDIR
cp stage/hdr/include/*.h $SR/usr/include/ncicore/
cp stage/hdr/include/*.h $SR/usr/include/nciplugin/
cp stage/hdr/core/include/*.h $SR/usr/include/nfcd/
cp -r stage/hdr/core/include/internal $SR/usr/include/nfcd/ 2>/dev/null || true
cp src/libglibutil/include/*.h $SR/usr/include/gutil/

# Библиотеки с устройства + линкерные symlink'и
cp -a stage/lib/*.so* $SR/usr/lib/
for l in ncicore nciplugin glibutil; do
    ln -sf lib$l.so.1 $SR/usr/lib/lib$l.so
done

# pkg-config
cat > $PCDIR/libncicore.pc <<PC
libdir=/usr/lib
includedir=/usr/include
Name: libncicore
Description: NCI state machine development library
Version: 1.1.23
Libs: -L\${libdir} -lncicore
Cflags: -I\${includedir} -I\${includedir}/ncicore
PC
cat > $PCDIR/libnciplugin.pc <<PC
libdir=/usr/lib
includedir=/usr/include
Name: libnciplugin
Description: Support library for NCI-based nfcd plugins
Version: 1.1.5
Libs: -L\${libdir} -lnciplugin
Cflags: -I\${includedir} -I\${includedir}/nciplugin
PC
cat > $PCDIR/libglibutil.pc <<PC
libdir=/usr/lib
includedir=/usr/include
Name: libglibutil
Description: GLib utilites
Version: 1.0.72
Libs: -L\${libdir} -lglibutil
Cflags: -I\${includedir} -I\${includedir}/gutil
PC
cat > $PCDIR/nfcd-plugin.pc <<PC
includedir=/usr/include
Name: nfcd-plugin
Description: Header files for building nfcd plugins
Version: 1.1.15
Cflags: -I\${includedir} -I\${includedir}/nfcd
PC

# Сборка
cd src/nfcd-pn54x-plugin
export PKG_CONFIG_SYSROOT_DIR=$SR
export PKG_CONFIG_LIBDIR=$PCDIR:$SR/usr/share/pkgconfig
make release CROSS_COMPILE=armv7hl-meego-linux-gnueabi- 2>&1 | tail -3
armv7hl-meego-linux-gnueabi-readelf -d build/release/pn54x.so | head -9

# nci-init (aarch64, статик): включение чипа + NCI smoke-тест
cd /work
aarch64-meego-linux-gnu-gcc -static -O2 -o /tmp/nci-init tools/nci-init.c

# libncicore с записью MIFARE Classic (Proprietary 0x80) в discovery-map
# и подменой conn id для proprietary-соединения (nci_core.c)
cd /work/src/libncicore
make release CROSS_COMPILE=armv7hl-meego-linux-gnueabi- \
    STRIP=armv7hl-meego-linux-gnueabi-strip 2>&1 | tail -3
ls -la build/release/libncicore.so.1.1.23

# RPM: payload в /usr/share (APM проставит security.ima при установке)
mkdir -p /tmp/rpmbuild/BUILD /tmp/rpmbuild/RPMS /tmp/rpmbuild/SPECS \
         /tmp/payload/usr/share/nfcd-pn54x-plugin
cp /work/src/nfcd-pn54x-plugin/build/release/pn54x.so /tmp/payload/usr/share/nfcd-pn54x-plugin/
cp /tmp/nci-init /tmp/payload/usr/share/nfcd-pn54x-plugin/
cp /work/src/libncicore/build/release/libncicore.so.1.1.23 /tmp/payload/usr/share/nfcd-pn54x-plugin/
cat > /tmp/rpmbuild/SPECS/pn54x.spec <<SPEC
Name:       nfcd-pn54x-plugin
Summary:    Direct /dev/nxpnfc transport plugin for nfcd (MIFARE Classic path)
Version:    $VERSION
Release:    1
Group:      System/Libraries
License:    BSD-3-Clause
%description
Adapter plugin for nfcd talking raw NCI directly to NXP NFCC
via /dev/nxpnfc, bypassing the Android NFC HAL. mer-hybris
nfcd-pn54x-plugin 1.0.3 + ENOTTY-tolerant power handling
(for the nxpnfc driver). Payload for manual symlink into
/usr/lib/nfcd/plugins/ (see project README).

%install
cp -r /tmp/payload/* %{buildroot}/

%files
%defattr(644,root,root,-)
/usr/share/nfcd-pn54x-plugin/pn54x.so
%defattr(755,root,root,-)
/usr/share/nfcd-pn54x-plugin/nci-init
/usr/share/nfcd-pn54x-plugin/libncicore.so.1.1.23
SPEC
rpmbuild -bb --target=armv7hl-meego-linux \
    --define '_topdir /tmp/rpmbuild' --define '_builddir /tmp' \
    /tmp/rpmbuild/SPECS/pn54x.spec 2>&1 | grep -E 'Wrote: /tmp.*pn54x-plugin-[0-9]' | head -2

# Подпись regular-ключом из SDK
RPM=$(ls /tmp/rpmbuild/RPMS/armv7hl/nfcd-pn54x-plugin-$VERSION-1.armv7hl.rpm)
apptool sign -k /keys/regular_key.pem -c /keys/regular_cert.pem "$RPM" 2>&1 | tail -1
mkdir -p /work/RPMS
cp "$RPM" /work/RPMS/
EOF
echo "== Готово:"
ls -la "$ROOT/RPMS/nfcd-pn54x-plugin-$VERSION-1.armv7hl.rpm"
