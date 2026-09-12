// nci-init: включение NXP NFCC (PN7160) через драйвер /dev/nxpnfc,
// отправка вендорской NCI-конфигурации из /vendor/etc/libnfc-nxp.conf
// и smoke-проверка (CORE_RESET → ответ).
//
// Зачем: после цикла VEN чип теряет конфигурацию, включая параметры,
// активирующие MIFARE Classic engine. Вендорский HAL шлёт эти блоки
// при старте (они и есть готовые NCI-кадры); libncicore далее делает
// CORE_RESET Keep Configuration, так что конфиг переживает запуск nfcd.
//
// Использование: nci-init [on|off|smoke|mfctest|keyprobe]  (по умолчанию smoke)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define NXPNFC_SET_PWR  0x4008E901
#define PWR_OFF 0
#define PWR_ON  1

static const char *dev = "/dev/nxpnfc";
static const char *conf = "/vendor/etc/libnfc-nxp.conf";

static int set_power(int fd, int on)
{
    if (ioctl(fd, NXPNFC_SET_PWR, on ? PWR_ON : PWR_OFF) < 0) {
        printf("ioctl SET_PWR(%d): %s\n", on, strerror(errno));
        return -1;
    }
    printf("VEN %s ok\n", on ? "on" : "off");
    return 0;
}

// poll() у этого драйвера бесполезен: данные приходят только через read(),
// который и армирует IRQ и ждёт (блокирующе, но прерываемо сигналом).
// Таймаут — через ualarm: read прерывается с EINTR.
static volatile sig_atomic_t got_alrm;
static void on_alrm(int sig) { (void)sig; got_alrm = 1; }

// >0 = данные, 0 = таймаут, <0 = ошибка
static int wait_read(int fd, unsigned char *buf, int timeout_ms)
{
    got_alrm = 0;
    signal(SIGALRM, on_alrm);
    ualarm((useconds_t)timeout_ms * 1000, 0);
    int n = read(fd, buf, 260);
    ualarm(0, 0);
    if (n < 0 && (errno == EINTR || got_alrm))
        return 0;
    return n;
}

// Отправить NCI-кадр и прочитать ответ (с таймаутом). Печатает результат.
static int xchg(int fd, const unsigned char *cmd, int len, const char *name)
{
    if (write(fd, cmd, len) != len) {
        printf("%s: write: %s\n", name, strerror(errno));
        return -1;
    }
    unsigned char buf[260];
    int n = wait_read(fd, buf, 2000);
    if (n <= 0) {
        printf("%s: %s\n", name, n == 0 ? "таймаут ответа" : strerror(errno));
        return -1;
    }
    int shown = n < 12 ? n : 12;
    printf("%s: %d байт:", name, n);
    for (int i = 0; i < shown; i++)
        printf(" %02x", buf[i]);
    printf("%s\n", n > shown ? " …" : "");
    return n;
}

// Прочитать блок `NAME={ XX, XX, … }` из конфига в buf. Возвращает длину или -1.
static int read_conf_block(const char *text, const char *name,
                           unsigned char *out, int maxlen)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s={", name);
    const char *p = strstr(text, pattern);
    if (!p)
        return -1;
    p += strlen(pattern);
    int n = 0;
    while (*p && *p != '}' && n < maxlen) {
        unsigned int v;
        if (sscanf(p, " %x ,", &v) == 1 || sscanf(p, " %x", &v) == 1) {
            out[n++] = (unsigned char)v;
            while (*p && *p != ',') {
                if (*p == '}')
                    return n;
                p++;
            }
            if (*p == ',')
                p++;
        } else {
            break;
        }
    }
    return n;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;
    static char text[16384];
    size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = 0;
    return text;
}

// Прочитать кадр, пропуская служебные NTF (CORE_CONN_CREDITS). Печатает всё.
static int read_frame(int fd, unsigned char *buf, int timeout_ms, const char *what)
{
    for (int guard = 0; guard < 8; guard++) {
        int n = wait_read(fd, buf, timeout_ms);
        if (n == 0)
            return 0;
        if (n < 0)
            return -1;
        int shown = n < 24 ? n : 24;
        printf("  RX %d:", n);
        for (int i = 0; i < shown; i++)
            printf(" %02x", buf[i]);
        printf("%s\n", n > shown ? " …" : "");
        if (buf[0] == 0x60 && buf[1] == 0x06)
            continue; /* credits — пропускаем */
        (void)what;
        return n;
    }
    return 0;
}

// MfcAuthReq (0x40): {cmd, sector, bKey, key6}. Возвращает статус (0 = OK) или -1.
// ВНИМАНИЕ: адрес — НОМЕР СЕКТОРА (не блока)! Проверено на PN7160 (T800):
// auth с верным ключом проходит только при секторной адресации.
// Proprietary-интерфейс (0x80) использует динамическое соединение: conn id —
// это RF Discovery ID из RF_INTF_ACTIVATED_NTF, а не 0.
static unsigned char g_conn;

static int mfc_auth(int fd, unsigned char block, const unsigned char *key6,
                    unsigned char bkey)
{
    unsigned char cmd[12] = { 0x00, 0x00, 0x09, 0x40, 0x00, 0x00 };
    unsigned char buf[260];
    cmd[1] = g_conn;
    cmd[4] = block;
    cmd[5] = bkey;
    memcpy(cmd + 6, key6, 6);
    if (write(fd, cmd, sizeof(cmd)) != sizeof(cmd))
        return -1;
    int n = read_frame(fd, buf, 2000, "auth");
    if (n >= 5 && buf[0] == 0x00 && buf[3] == 0x40)
        return buf[4];
    return -1;
}

// MfRawDataXchgHdr(0x10) + read 0x30. Копирует 16 байт в out16. 0 = ок.
static int mfc_read(int fd, unsigned char block, unsigned char *out16)
{
    unsigned char cmd[6] = { 0x00, 0x00, 0x03, 0x10, 0x30, 0x00 };
    unsigned char buf[260];
    cmd[1] = g_conn;
    cmd[5] = block;
    if (write(fd, cmd, sizeof(cmd)) != sizeof(cmd))
        return -1;
    int n = read_frame(fd, buf, 2000, "read");
    /* кадр: 00 00 len {0x10, data(len-2), status}; драйвер дополняет буфер */
    if (n >= 5 && buf[0] == 0x00 && buf[2] >= 3 && buf[3] == 0x10
            && buf[2 + buf[2]] == 0x00) {
        memcpy(out16, buf + 4, 16);
        return 0;
    }
    return -1;
}

// Активация карты: RF_DISCOVER + ожидание RF_INTF_ACTIVATED_NTF (0x61 0x05).
// Заполняет uid/uid_len. 0 = карта активирована, -1 = таймаут.
static int discover_tag(int fd, unsigned char *uid, int *uid_len, int wait_s)
{
    static const unsigned char discover[] = {
        0x21, 0x03, 0x07, 0x03,
        0x00, 0x01,              /* Passive Poll A, period 1 */
        0x01, 0x01,              /* Active Poll A */
        0x02, 0x01               /* Passive Poll B */
    };
    unsigned char buf[260];
    *uid_len = 0;
    if (write(fd, discover, sizeof(discover)) != sizeof(discover))
        return -1;
    for (int t = 0; t < wait_s && !*uid_len; t++) {
        int n = read_frame(fd, buf, 1000, "act");
        if (n > 0 && buf[0] == 0x61 && buf[1] == 0x05) {
            g_conn = buf[3]; /* RF Discovery ID = conn id динамического соединения */
            /* Ищем NFCID1: длина 0x07 и первый байт 0x04 (NXP) либо 4-байтовый */
            for (int i = 3; i + 11 < n; i++) {
                if ((buf[i + 2] == 0x07 && buf[i + 3] == 0x04)
                        || (buf[i + 2] == 0x04 && buf[i + 3] == 0x04)) {
                    *uid_len = buf[i + 2];
                    memcpy(uid, buf + i + 3, *uid_len);
                    break;
                }
            }
        }
    }
    return *uid_len ? 0 : -1;
}

// RF_DEACTIVATE_CMD (Idle) — для повторной активации после неудачной auth:
// карта MIFARE Classic после неверного ключа замолкает до re-select.
static int deactivate_idle(int fd)
{
    unsigned char cmd[] = { 0x21, 0x06, 0x01, 0x00 };
    unsigned char buf[260];
    if (write(fd, cmd, sizeof(cmd)) != sizeof(cmd))
        return -1;
    return read_frame(fd, buf, 1000, "deact") > 0 ? 0 : -1;
}

// MfcAuthReq с произвольным payload (для перебора форматов).
static int mfc_auth_raw(int fd, const unsigned char *payload, int len)
{
    unsigned char cmd[20] = { 0x00, 0x00, 0x00 };
    unsigned char buf[260];
    cmd[1] = g_conn;
    cmd[2] = (unsigned char)len;
    memcpy(cmd + 3, payload, len);
    if (write(fd, cmd, 3 + len) != 3 + len)
        return -1;
    int n = read_frame(fd, buf, 2000, "auth");
    if (n >= 5 && buf[0] == 0x00 && buf[3] == 0x40)
        return buf[4];
    return -1;
}

// Проверка форматов MfcAuthReq: blk0/FFFF работает, а заведомо верный
// ключ сектора 4 (E56AC127DD45, подтверждён MCT на Android) — нет.
// Перебираем варианты payload с реактивацией перед каждой попыткой.
static int mfc_key_probe(int fd)
{
    static const unsigned char discover_map[] = {
        0x21, 0x00, 0x07, 0x02,
        0x02, 0x01, 0x01,        /* T2T/Poll/Frame */
        0x80, 0x01, 0x80         /* Proprietary/Poll/Proprietary */
    };
    static const unsigned char ka[6] = { 0xE5, 0x6A, 0xC1, 0x27, 0xDD, 0x45 };
    static const unsigned char kb[6] = { 0x19, 0xFC, 0x84, 0xA3, 0x78, 0x4B };
    static const unsigned char wrong[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE };
    static const unsigned char keyFF[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    unsigned char buf[260], uid[7], blk[16];
    int uid_len;

    printf("== RF_DISCOVER_MAP_CMD ==\n");
    if (write(fd, discover_map, sizeof(discover_map)) != sizeof(discover_map)
            || read_frame(fd, buf, 2000, "map") <= 0)
        return -1;
    printf("== поднесите карту 042F23 (15 с) ==\n");
    if (discover_tag(fd, uid, &uid_len, 15) < 0) {
        printf("активация не произошла\n");
        return -1;
    }
    printf("UID (%d):", uid_len);
    for (int i = 0; i < uid_len; i++)
        printf(" %02x", uid[i]);
    printf("  conn=%d\n", g_conn);

    /* Sanity: блок 0 + осмысленность auth */
    int st = mfc_auth(fd, 0, keyFF, 0x10);
    printf("auth blk0 FFFF      -> %d\n", st);
    if (st == 0 && mfc_read(fd, 0, blk) == 0) {
        printf("blk0:");
        for (int i = 0; i < 16; i++)
            printf(" %02x", blk[i]);
        printf("\n");
    }
    printf("read blk1  (нужна auth s0)  -> %s\n",
           mfc_read(fd, 1, blk) == 0 ? "OK" : "FAIL");
    printf("read blk17 (нужна auth s4)  -> %s (ожидаем FAIL)\n",
           mfc_read(fd, 17, blk) == 0 ? "OK" : "FAIL");

    /* Матрица форматов auth, перед каждой — реактивация */
    unsigned char p[16];
    static const struct {
        const char *name;
        unsigned char block, type;
        const unsigned char *key;
        int with_uid4;
    } matrix[] = {
        { "blk0  неверный ключ DEADBEEFCAFE", 0, 0x10, wrong, 0 },
        { "blk16 type=10 keyA",              16, 0x10, ka, 0 },
        { "blk16 type=10 keyA +uid4",        16, 0x10, ka, 1 },
        { "blk16 type=60 keyA +uid4",        16, 0x60, ka, 1 },
        { "blk16 type=61 keyB +uid4",        16, 0x61, kb, 1 },
        { "blk16 type=90 keyB",              16, 0x90, kb, 0 },
        { "blk16 type=90 keyB +uid4",        16, 0x90, kb, 1 },
        { "blk16 type=10 keyB (крест)",      16, 0x10, kb, 0 },
        { "blk16 type=90 keyA (крест)",      16, 0x90, ka, 0 },
        { "blk19 type=10 keyA (трейлер)",    19, 0x10, ka, 0 },
        { "blk19 type=10 keyA +uid4",        19, 0x10, ka, 1 },
    };
    for (unsigned i = 0; i < sizeof(matrix) / sizeof(matrix[0]); i++) {
        deactivate_idle(fd);
        if (discover_tag(fd, uid, &uid_len, 5) < 0) {
            printf("карта потеряна\n");
            return -1;
        }
        int n = 0;
        p[n++] = 0x40;
        p[n++] = matrix[i].block;
        p[n++] = matrix[i].type;
        memcpy(p + n, matrix[i].key, 6);
        n += 6;
        if (matrix[i].with_uid4) {
            memcpy(p + n, uid + uid_len - 4, 4);
            n += 4;
        }
        st = mfc_auth_raw(fd, p, n);
        printf("%-32s -> %d%s\n", matrix[i].name, st,
               st == 0 ? "  << AUTH OK" : "");
        if (st == 0) {
            for (int b = 16; b < 20; b++) {
                if (mfc_read(fd, b, blk) == 0) {
                    printf("blk%d:", b);
                    for (int j = 0; j < 16; j++)
                        printf(" %02x", blk[j]);
                    printf("\n");
                }
            }
            unsigned kop = (unsigned)blk[0] | ((unsigned)blk[1] << 8)
                    | ((unsigned)blk[2] << 16) | ((unsigned)blk[3] << 24);
            printf("БАЛАНС: %u.%02u руб\n", kop / 100, kop % 100);
            return 0;
        }
    }
    printf("ни один формат не сработал\n");
    return -1;
}

// Прогон известной таблицы ключей Подорожника по приложенной карте
// (1K: сектора 4-5; 4K: ещё 8-12). Реактивация перед каждой попыткой.
static int mfc_auth_all(int fd)
{
    static const unsigned char discover_map[] = {
        0x21, 0x00, 0x07, 0x02,
        0x02, 0x01, 0x01,
        0x80, 0x01, 0x80
    };
    static const struct { unsigned char sector; unsigned char key[6]; } pl[] = {
        { 4,  { 0xE5, 0x6A, 0xC1, 0x27, 0xDD, 0x45 } }, /* s4  баланс  */
        { 5,  { 0x77, 0xDA, 0xBC, 0x98, 0x25, 0xE1 } }, /* s5  поездки */
        { 8,  { 0x26, 0x97, 0x3E, 0xA7, 0x43, 0x21 } }, /* s8          */
        { 9,  { 0xEB, 0x0A, 0x8F, 0xF8, 0x8A, 0xDE } }, /* s9          */
        { 10, { 0xEA, 0x0F, 0xD7, 0x3C, 0xB1, 0x49 } }, /* s10         */
        { 11, { 0xC7, 0x6B, 0xF7, 0x1A, 0x25, 0x09 } }, /* s11         */
        { 12, { 0xAC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } }, /* s12         */
    };
    unsigned char buf[260], uid[7], blk[16];
    int uid_len;

    if (write(fd, discover_map, sizeof(discover_map)) != sizeof(discover_map)
            || read_frame(fd, buf, 2000, "map") <= 0)
        return -1;
    printf("== поднесите карту (15 с) ==\n");
    if (discover_tag(fd, uid, &uid_len, 15) < 0) {
        printf("активация не произошла\n");
        return -1;
    }
    printf("UID (%d):", uid_len);
    for (int i = 0; i < uid_len; i++)
        printf(" %02x", uid[i]);
    printf("\n");

    int ok = 0;
    for (unsigned i = 0; i < sizeof(pl) / sizeof(pl[0]); i++) {
        deactivate_idle(fd);
        if (discover_tag(fd, uid, &uid_len, 5) < 0) {
            printf("карта потеряна\n");
            return -1;
        }
        int st = mfc_auth(fd, pl[i].sector, pl[i].key, 0x10);
        printf("s%-2d keyA -> %d", pl[i].sector, st);
        if (st == 0) {
            ok++;
            printf("  OK");
            /* MIFARE 1K: 4 блока в секторе; 4K: сектора 0-31 по 4, 32-39 по 16 */
            int per = pl[i].sector < 32 ? 4 : 16;
            int base = pl[i].sector < 32 ? pl[i].sector * 4
                                         : 128 + (pl[i].sector - 32) * 16;
            for (int b = base; b < base + per && b < base + 1; b++)
                if (mfc_read(fd, b, blk) == 0) {
                    printf("  blk%d:", b);
                    for (int j = 0; j < 16; j++)
                        printf(" %02x", blk[j]);
                }
        }
        printf("\n");
    }
    printf("открыто секторов: %d\n", ok);
    return ok ? 0 : -1;
}

// Проверка честности FW: auth с НЕВЕРНЫМ ключом на сектор, где ключ точно
// FFFFFFFFFFFF (по MCT-дампу карты 042F23 это сектора 2 и 3). Если статус 0 —
// прошивка подставляет FFFF вместо переданного ключа. Если 3 — честна.
static int mfc_ff_test(int fd)
{
    static const unsigned char discover_map[] = {
        0x21, 0x00, 0x07, 0x02,
        0x02, 0x01, 0x01,
        0x80, 0x01, 0x80
    };
    static const unsigned char keyFF[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static const unsigned char wrong[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE };
    static const struct { unsigned char block; const unsigned char *key;
                          const char *what; } t[] = {
        { 8,  keyFF, "blk8  FFFF (s2, ключ точно FFFF)" },
        { 8,  wrong, "blk8  DEAD (если 0 — FW подменяет ключ)" },
        { 12, keyFF, "blk12 FFFF (s3, ключ точно FFFF)" },
        { 12, wrong, "blk12 DEAD (если 0 — FW подменяет ключ)" },
        { 16, keyFF, "blk16 FFFF (s4, ключ НЕ FFFF)" },
        { 16, wrong, "blk16 DEAD" },
    };
    unsigned char buf[260], uid[7], blk[16];
    int uid_len;

    if (write(fd, discover_map, sizeof(discover_map)) != sizeof(discover_map)
            || read_frame(fd, buf, 2000, "map") <= 0)
        return -1;
    printf("== поднесите карту 042F23 (15 с) ==\n");
    if (discover_tag(fd, uid, &uid_len, 15) < 0) {
        printf("активация не произошла\n");
        return -1;
    }
    printf("UID:");
    for (int i = 0; i < uid_len; i++)
        printf(" %02x", uid[i]);
    printf("\n");

    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        deactivate_idle(fd);
        if (discover_tag(fd, uid, &uid_len, 5) < 0) {
            printf("карта потеряна\n");
            return -1;
        }
        int st = mfc_auth(fd, t[i].block, t[i].key, 0x10);
        printf("%-40s -> %d", t[i].what, st);
        if (st == 0 && mfc_read(fd, t[i].block, blk) == 0) {
            printf("  данные:");
            for (int j = 0; j < 16; j++)
                printf(" %02x", blk[j]);
        }
        printf("\n");
    }
    return 0;
}

// Гипотезы: (A) байт адреса в MfcAuthReq — номер СЕКТОРА, а не блока;
// (B) в сессии сначала обязателен auth блока 0, дальше любые.
static int mfc_seq_test(int fd)
{
    static const unsigned char discover_map[] = {
        0x21, 0x00, 0x07, 0x02,
        0x02, 0x01, 0x01,
        0x80, 0x01, 0x80
    };
    static const unsigned char keyFF[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static const unsigned char ka[6] = { 0xE5, 0x6A, 0xC1, 0x27, 0xDD, 0x45 };
    static const unsigned char k5[6] = { 0x77, 0xDA, 0xBC, 0x98, 0x25, 0xE1 };
    unsigned char buf[260], uid[7], blk[16];
    int uid_len, st;

    if (write(fd, discover_map, sizeof(discover_map)) != sizeof(discover_map)
            || read_frame(fd, buf, 2000, "map") <= 0)
        return -1;
    printf("== поднесите карту 042F23 (15 с) ==\n");
    if (discover_tag(fd, uid, &uid_len, 15) < 0) {
        printf("активация не произошла\n");
        return -1;
    }
    printf("UID:");
    for (int i = 0; i < uid_len; i++)
        printf(" %02x", uid[i]);
    printf("\n");

    printf("-- фаза B: одна сессия, сначала blk0 --\n");
    printf("auth blk0  FFFF -> %d\n", mfc_auth(fd, 0, keyFF, 0x10));
    printf("auth blk8  FFFF -> %d\n", mfc_auth(fd, 8, keyFF, 0x10));
    st = mfc_auth(fd, 16, ka, 0x10);
    printf("auth blk16 E56A -> %d\n", st);
    if (st == 0) {
        for (int b = 16; b < 20; b++)
            if (mfc_read(fd, b, blk) == 0) {
                printf("blk%d:", b);
                for (int j = 0; j < 16; j++)
                    printf(" %02x", blk[j]);
                printf("\n");
            }
    }

    printf("-- фаза A: байт адреса = номер сектора --\n");
    static const struct { unsigned char addr; const unsigned char *key;
                          const char *what; } t[] = {
        { 2, keyFF, "addr=2 (сектор 2) FFFF" },
        { 3, keyFF, "addr=3 (сектор 3) FFFF" },
        { 4, ka,    "addr=4 (сектор 4) E56AC127DD45" },
        { 5, k5,    "addr=5 (сектор 5) 77DABC9825E1" },
    };
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        deactivate_idle(fd);
        if (discover_tag(fd, uid, &uid_len, 5) < 0) {
            printf("карта потеряна\n");
            return -1;
        }
        st = mfc_auth(fd, t[i].addr, t[i].key, 0x10);
        printf("%-32s -> %d\n", t[i].what, st);
        if (st == 0 && t[i].addr >= 4) {
            int base = t[i].addr * 4;
            for (int b = base; b < base + 4; b++)
                if (mfc_read(fd, b, blk) == 0) {
                    printf("blk%d:", b);
                    for (int j = 0; j < 16; j++)
                        printf(" %02x", blk[j]);
                    printf("\n");
                }
            unsigned kop = (unsigned)blk[0] | ((unsigned)blk[1] << 8)
                    | ((unsigned)blk[2] << 16) | ((unsigned)blk[3] << 24);
            printf("возможный БАЛАНС: %u.%02u руб\n", kop / 100, kop % 100);
        }
    }
    return 0;
}

// Перебор ключей сектора 4 из /tmp/mfc_keys.txt (hex, по строке).
// NB: MfcAuthReq адресуется НОМЕРОМ СЕКТОРА, чтение — номером блока.
static int mfc_key_sweep(int fd)
{
    FILE *f = fopen("/tmp/mfc_keys.txt", "r");
    if (!f) {
        printf("нет /tmp/mfc_keys.txt\n");
        return -1;
    }
    char line[64];
    unsigned char blk[16];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strlen(line) < 12)
            continue;
        unsigned char key[6];
        int ok = 1;
        for (int i = 0; i < 6 && ok; i++)
            ok = sscanf(line + i * 2, "%2hhx", &key[i]) == 1;
        if (!ok)
            continue;
        printf("%.12s: ", line);
        int found = 0;
        for (int pass = 0; pass < 2 && !found; pass++) {
            int st = mfc_auth(fd, 4, key, pass ? 0x90 : 0x10);
            printf("%s%d ", pass ? "B" : "A", st);
            if (st == 0) {
                printf("<- AUTH OK ");
                if (mfc_read(fd, 16, blk) == 0) {
                    unsigned kop = (unsigned)blk[0] | ((unsigned)blk[1] << 8)
                            | ((unsigned)blk[2] << 16) | ((unsigned)blk[3] << 24);
                    printf("\n  КЛЮЧ НАЙДЕН (%s): БАЛАНС %u коп = %u.%02u руб\n",
                           pass ? "B" : "A", kop, kop / 100, kop % 100);
                    fclose(f);
                    return 0;
                }
            } else {
                /* После неверного ключа карта замолкает: реактивация */
                unsigned char uid[7];
                int uid_len;
                deactivate_idle(fd);
                if (discover_tag(fd, uid, &uid_len, 5) < 0) {
                    printf("\nкарта потеряна\n");
                    fclose(f);
                    return -1;
                }
            }
        }
        printf("\n");
    }
    fclose(f);
    printf("ни один ключ не подошёл\n");
    return -1;
}


static int mfc_test(int fd)
{
    static const unsigned char discover_map[] = {
        0x21, 0x00, 0x07, 0x02,
        0x02, 0x01, 0x01,        /* T2T/Poll/Frame */
        0x80, 0x01, 0x80         /* Proprietary/Poll/Proprietary */
    };
    static const unsigned char discover[] = {
        0x21, 0x03, 0x07, 0x03,
        0x00, 0x01,              /* Passive Poll A, period 1 */
        0x01, 0x01,              /* Active Poll A */
        0x02, 0x01               /* Passive Poll B */
    };
    unsigned char buf[260];

    printf("== RF_DISCOVER_MAP_CMD ==\n");
    if (write(fd, discover_map, sizeof(discover_map)) != sizeof(discover_map)
            || read_frame(fd, buf, 2000, "map") <= 0)
        return -1;
    printf("== RF_DISCOVER_CMD (поднесите карту, 15 с) ==\n");
    if (write(fd, discover, sizeof(discover)) != sizeof(discover))
        return -1;

    /* Ждём RF_INTF_ACTIVATED_NTF (61 03 …) */
    unsigned char uid[7];
    int uid_len = 0;
    for (int t = 0; t < 15 && !uid_len; t++) {
        int n = read_frame(fd, buf, 1000, "act");
        if (n > 0 && buf[0] == 0x61 && buf[1] == 0x05) {
            /* Ищем NFCID1: длина 0x07 и первый байт 0x04 (NXP) либо 4-байтовый */
            for (int i = 3; i + 11 < n; i++) {
                if ((buf[i + 2] == 0x07 && buf[i + 3] == 0x04)
                        || (buf[i + 2] == 0x04 && buf[i + 3] == 0x04)) {
                    uid_len = buf[i + 2];
                    memcpy(uid, buf + i + 3, uid_len);
                    break;
                }
            }
        }
    }
    if (!uid_len) {
        printf("активация не произошла\n");
        return -1;
    }
    printf("UID (%d):", uid_len);
    for (int i = 0; i < uid_len; i++)
        printf(" %02x", uid[i]);
    printf("\n");

    /* Sanity: блок 0 с ключом по умолчанию FFFFFFFFFFFF (всегда читается) */
    static const unsigned char keyFF[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    unsigned char blk[16];
    int st = mfc_auth(fd, 0, keyFF, 0x10);
    printf("auth block0 FFFFFFFFFFFF -> статус %d\n", st);
    if (st == 0 && mfc_read(fd, 0, blk) == 0) {
        printf("block0:");
        for (int i = 0; i < 16; i++)
            printf(" %02x", blk[i]);
        printf("\n");
    }

    /* Ключи Подорожника по своим секторам (адрес = номер сектора!) */
    static const struct { unsigned char sector; unsigned char key[6]; } plantain[] = {
        { 4,  { 0xE5, 0x6A, 0xC1, 0x27, 0xDD, 0x45 } }, /* s4  баланс  */
        { 5,  { 0x77, 0xDA, 0xBC, 0x98, 0x25, 0xE1 } }, /* s5  поездки */
        { 8,  { 0x26, 0x97, 0x3E, 0xA7, 0x43, 0x21 } }, /* s8          */
        { 9,  { 0xEB, 0x0A, 0x8F, 0xF8, 0x8A, 0xDE } }, /* s9          */
        { 10, { 0xEA, 0x0F, 0xD7, 0x3C, 0xB1, 0x49 } }, /* s10         */
        { 11, { 0xC7, 0x6B, 0xF7, 0x1A, 0x25, 0x09 } }, /* s11         */
        { 12, { 0xAC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } }, /* s12         */
    };
    for (unsigned i = 0; i < sizeof(plantain) / sizeof(plantain[0]); i++) {
        int st = mfc_auth(fd, plantain[i].sector, plantain[i].key, 0x10);
        printf("sector key @%d -> статус %d\n", plantain[i].sector, st);
    }

    /* Перебор ключей сектора 4 */
    return mfc_key_sweep(fd);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *mode = argc > 1 ? argv[1] : "smoke";
    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        printf("open %s: %s\n", dev, strerror(errno));
        return 1;
    }

    if (!strcmp(mode, "off")) {
        int rc = set_power(fd, PWR_OFF);
        close(fd);
        return rc ? 1 : 0;
    }

    /* Полный цикл включения, как делает NXP HAL: off → 10 мс → on */
    set_power(fd, PWR_OFF);
    usleep(10000);
    if (set_power(fd, PWR_ON) < 0)
        return 1;
    usleep(20000); /* загрузка прошивки чипа после VEN on */
    if (!strcmp(mode, "on")) {
        close(fd);
        return 0;
    }

    /* NCI init */
    static const unsigned char reset[] = { 0x20, 0x00, 0x01, 0x00 };
    static const unsigned char init[]  = { 0x20, 0x01, 0x00 };
    if (xchg(fd, reset, sizeof(reset), "CORE_RESET_CMD") < 0)
        return 1;
    usleep(50000); /* может прийти CORE_RESET_NTF — сольётся в след. чтение */
    xchg(fd, init, sizeof(init), "CORE_INIT_CMD");

    /* Вендорская конфигурация из libnfc-nxp.conf */
    char *text = read_file(conf);
    if (!text) {
        printf("%s: %s\n", conf, strerror(errno));
        return 1;
    }
    static const char *blocks[] = {
        "NXP_CORE_CONF",
        "NXP_CORE_CONF_EXTN",
        "NXP_NFC_PROFILE_EXTN",
        "NXP_ACT_PROP_EXTN",
        "NXP_RF_CONF_BLK_1",
        "NXP_RF_CONF_BLK_2",
    };
    unsigned char frame[600];
    for (unsigned i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
        int n = read_conf_block(text, blocks[i], frame, sizeof(frame));
        if (n < 3) {
            printf("%s: не найден\n", blocks[i]);
            continue;
        }
        xchg(fd, frame, n, blocks[i]);
    }

    if (!strcmp(mode, "mfctest")) {
        int rc = mfc_test(fd);
        close(fd);
        return rc ? 1 : 0;
    }
    if (!strcmp(mode, "keyprobe")) {
        int rc = mfc_key_probe(fd);
        close(fd);
        return rc ? 1 : 0;
    }
    if (!strcmp(mode, "authall")) {
        int rc = mfc_auth_all(fd);
        close(fd);
        return rc ? 1 : 0;
    }
    if (!strcmp(mode, "fftest")) {
        int rc = mfc_ff_test(fd);
        close(fd);
        return rc ? 1 : 0;
    }
    if (!strcmp(mode, "seqtest")) {
        int rc = mfc_seq_test(fd);
        close(fd);
        return rc ? 1 : 0;
    }

    printf("OK: чип включён и сконфигурирован, можно запускать nfcd\n");
    close(fd);
    return 0;
}
