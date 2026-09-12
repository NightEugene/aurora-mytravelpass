// Smoke-тест прямого NCI-обмена с NXP NFCC через /dev/nxpnfc.
// Шлёт CORE_RESET_CMD и CORE_INIT_CMD (NCI 1.0), печатает ответы.
// Сборка: armv7hl-meego-linux-gnueabi-gcc -static -o nci-smoke nci-smoke.c
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *dev = "/dev/nxpnfc";

static int xchg(int fd, const unsigned char *cmd, int cmdlen,
                unsigned char *resp, int *resplen, const char *name)
{
    if (write(fd, cmd, cmdlen) != cmdlen) {
        printf("%s: write failed: %s\n", name, strerror(errno));
        return -1;
    }
    usleep(150000);
    int n = read(fd, resp, 260);
    if (n < 0) {
        printf("%s: read failed: %s\n", name, strerror(errno));
        return -1;
    }
    printf("%s: %d bytes:", name, n);
    for (int i = 0; i < n; i++)
        printf(" %02x", resp[i]);
    printf("\n");
    return 0;
}

int main(void)
{
    /* NCI 1.0: CORE_RESET_CMD(Keep Config), CORE_INIT_CMD */
    static const unsigned char reset[] = { 0x20, 0x00, 0x01, 0x00 };
    static const unsigned char init[]  = { 0x20, 0x01, 0x00 };
    unsigned char buf[260];
    int n;

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        printf("open %s failed: %s\n", dev, strerror(errno));
        return 1;
    }
    printf("opened %s\n", dev);

    if (xchg(fd, reset, sizeof(reset), buf, &n, "CORE_RESET_CMD") < 0)
        return 1;
    /* Может прийти CORE_RESET_NTF следом — читаем ещё раз */
    usleep(150000);
    n = read(fd, buf, 260);
    if (n > 0) {
        printf("extra: %d bytes:", n);
        for (int i = 0; i < n; i++)
            printf(" %02x", buf[i]);
        printf("\n");
    }
    if (xchg(fd, init, sizeof(init), buf, &n, "CORE_INIT_CMD") < 0)
        return 1;

    close(fd);
    printf("OK: NFCC отвечает по прямому NCI\n");
    return 0;
}
