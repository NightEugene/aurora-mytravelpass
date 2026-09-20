#!/usr/bin/env python3
"""Генератор иконок приложения «Подорожник».

Рисует иконку (карта с чипом и символом бесконтактной связи на градиенте)
без внешних зависимостей и сохраняет PNG в app/icons/<N>x<N>/.

Использование: python3 scripts/gen_icons.py
"""

import math
import os
import struct
import zlib

SIZES = (86, 108, 128, 172)
ICON_NAME = "ru.nighteugene.MyTravelPass.png"

BG_TOP = (16, 78, 139)      # синий (цвет карты Подорожник)
BG_BOTTOM = (0, 151, 136)   # бирюзовый
CARD = (245, 245, 245)
CHIP = (216, 178, 92)       # контактный чип
WAVE = (0, 121, 107)        # волны NFC на карте
CORNER = 0.16               # радиус скругления фона (доля стороны)


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def in_round_rect(x, y, x0, y0, w, h, r):
    if not (x0 <= x <= x0 + w and y0 <= y <= y0 + h):
        return False
    dx = max(x0 + r - x, 0.0, x - (x0 + w - r))
    dy = max(y0 + r - y, 0.0, y - (y0 + h - r))
    return dx * dx + dy * dy <= r * r


def shape(x, y):
    """Цвет пикселя (r, g, b, a) в нормализованных координатах [0, 1)."""
    if not in_round_rect(x, y, 0.0, 0.0, 1.0, 1.0, CORNER):
        return (0, 0, 0, 0)
    color = lerp(BG_TOP, BG_BOTTOM, y)

    # Карта
    if in_round_rect(x, y, 0.17, 0.29, 0.66, 0.42, 0.06):
        color = CARD
        # Чип
        if in_round_rect(x, y, 0.26, 0.455, 0.11, 0.09, 0.015):
            color = CHIP
        # Волны бесконтактной связи (три дуги, раскрыв вправо)
        wx, wy = 0.56, 0.5
        angle = math.degrees(math.atan2(y - wy, x - wx))
        dist = math.hypot(x - wx, y - wy)
        if -60.0 <= angle <= 60.0:
            for radius in (0.055, 0.095, 0.135):
                if abs(dist - radius) <= 0.012:
                    color = WAVE
                    break
    return color + (255,)


def write_png(path, size):
    raw = bytearray()
    ss = 2  # суперсэмплинг 2x2 для сглаживания краёв
    for py in range(size):
        raw.append(0)  # фильтр 0
        for px in range(size):
            acc = [0, 0, 0, 0]
            for sy in range(ss):
                for sx in range(ss):
                    nx = (px + (sx + 0.5) / ss) / size
                    ny = (py + (sy + 0.5) / ss) / size
                    pixel = shape(nx, ny)
                    for i in range(4):
                        acc[i] += pixel[i]
            raw += bytes(v // (ss * ss) for v in acc)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)  # RGBA, 8 бит
    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", ihdr))
        out.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        out.write(chunk(b"IEND", b""))


def main():
    icons_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "..", "app", "icons")
    for size in SIZES:
        directory = os.path.normpath(
            os.path.join(icons_dir, "{0}x{0}".format(size)))
        os.makedirs(directory, exist_ok=True)
        path = os.path.join(directory, ICON_NAME)
        write_png(path, size)
        print(path)


if __name__ == "__main__":
    main()
