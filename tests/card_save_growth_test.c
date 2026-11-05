#include <stdio.h>
#include <string.h>

#include "common.h"
#include "psyq/libmcrd.h"

#define TEST_CARD_BYTES 0x20000
#define TEST_SAVE_NAME "BASLUS-01193 DMW2"

static u8 card_image[TEST_CARD_BYTES];
static int card_exists;

void Psyq_Log(int *seen, const char *func, const char *fmt, ...) {
    (void)seen;
    (void)func;
    (void)fmt;
}

int Host_CardLoad(int port, unsigned char *buf, int size) {
    (void)port;
    if (!card_exists) return 0;
    memcpy(buf, card_image, (size_t)size);
    return 1;
}

int Host_CardStore(int port, const unsigned char *buf, int size) {
    (void)port;
    memcpy(card_image, buf, (size_t)size);
    card_exists = 1;
    return 1;
}

void Psyq_CardVBlank(void);

static u16 checksum(const u8 *bytes, int file_size) {
    const u16 *words = (const u16 *)bytes;
    int remaining = file_size / 2 - 1;
    u16 sum = 0;
    int i = 0;

    while (remaining-- > 0) {
        if ((i & 1) == 0) sum ^= words[i];
        else sum += words[i];
        i++;
    }
    return sum;
}

static int complete_async(int bytes, int expected_command) {
    int command, result, i;
    for (i = 0; i < bytes / 0x80 + 1; i++) Psyq_CardVBlank();
    return MemCardSync(1, &command, &result) == 1 && command == expected_command && result == 0;
}

static int fail(const char *why) {
    printf("card_save_growth_test: FAIL (%s)\n", why);
    return 1;
}

int main(void) {
    u8 legacy[0x4000] = { 0 };
    u8 extended[0x8000] = { 0 };
    u8 loaded[0x8000] = { 0 };
    int i;

    MemCardInit(0);
    MemCardStart();
    if (MemCardCreateFile(0, TEST_SAVE_NAME, 2) != 0) return fail("create two-block retail save");

    legacy[0] = 'S';
    legacy[1] = 'C';
    legacy[3] = 2;
    legacy[0x200] = 0x5A; /* Retail save-list data must survive migration. */
    legacy[0x3FFC] = 0x02;
    legacy[0x3FFD] = 0x01;
    {
        u16 sum = checksum(legacy, sizeof(legacy));
        legacy[0x3FFE] = (u8)sum;
        legacy[0x3FFF] = (u8)(sum >> 8);
    }
    if (MemCardWriteFile(0, TEST_SAVE_NAME, (u_long *)legacy, 0, sizeof(legacy)) != 1 ||
        !complete_async(sizeof(legacy), McFuncWriteFile)) {
        return fail("write retail save payload");
    }

    if (MemCardReadFile(0, TEST_SAVE_NAME, (u_long *)loaded, 0, sizeof(loaded)) != 1 ||
        !complete_async(sizeof(loaded), McFuncReadFile)) {
        return fail("read retail save into expanded buffer");
    }
    if (memcmp(loaded, legacy, sizeof(legacy)) != 0) return fail("legacy bytes preserved on read");
    for (i = sizeof(legacy); i < (int)sizeof(loaded); i++) {
        if (loaded[i] != 0) return fail("legacy read leaves extension area zeroed");
    }

    memcpy(extended, legacy, sizeof(legacy));
    memset(extended + sizeof(legacy), 0xA7, sizeof(extended) - sizeof(legacy));
    extended[3] = 4;
    extended[0x7FFC] = 3;
    extended[0x7FFD] = 1;
    {
        u16 sum = checksum(extended, sizeof(extended));
        extended[0x7FFE] = (u8)sum;
        extended[0x7FFF] = (u8)(sum >> 8);
    }
    if (MemCardWriteFile(0, TEST_SAVE_NAME, (u_long *)extended, 0, sizeof(extended)) != 1 ||
        !complete_async(sizeof(extended), McFuncWriteFile)) {
        return fail("grow legacy file and write extended save");
    }

    memset(loaded, 0, sizeof(loaded));
    if (MemCardReadFile(0, TEST_SAVE_NAME, (u_long *)loaded, 0, sizeof(loaded)) != 1 ||
        !complete_async(sizeof(loaded), McFuncReadFile) || memcmp(loaded, extended, sizeof(loaded)) != 0) {
        return fail("read back extended save");
    }
    puts("card_save_growth_test: PASS");
    return 0;
}
