#include "common.h"
#include "stag1100/stag1100.h"
#include "psyq/psyq_log.h"
#include "psyq/psyq_vblank.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int sCardStores;

void Psyq_Log(int *seen, const char *func, const char *fmt, ...) {
    (void)seen;
    (void)func;
    (void)fmt;
}

int Host_CardLoad(int port, unsigned char *buf, int size) {
    (void)port;
    (void)buf;
    (void)size;
    return 0;
}

int Host_CardStore(int port, const unsigned char *buf, int size) {
    (void)port;
    (void)buf;
    (void)size;
    sCardStores++;
    return 1;
}

int main(void) {
    Stg11SaveWork *work = calloc(1, sizeof(*work));
    int result;

    if (work == NULL || (uptr)work <= UINT32_MAX) {
        return 1;
    }

    strcpy((char *)work->fileName, "BASLUS-01193 DMW2");
    work->u34.s.blocks = STG11_SAVE_FILE_BLOCKS;
    work->u34.s.version = STG11_SAVE_VERSION;
    Stg11_CardWork = work;
    MemCardInit(0);
    MemCardStart();

    /* Run the production create-file path; it reaches MemCardCreateFile's strncpy. */
    result = Stg11_CardFileOp(work, 6, 0);

    if (result != 9) {
        (void)write(1, (u8 *)"card_filename_pointer_test: FAIL (create-file result)\n",
                    sizeof("card_filename_pointer_test: FAIL (create-file result)\n") - 1);
        return 2;
    }

    if (Stg11_CardAsyncOp(work, 3, 0) != -1) {
        return 3;
    }
    for (int i = 0; i <= STG11_SAVE_FILE_BYTES / 0x80; i++) {
        Psyq_CardVBlank();
    }
    Stg11_CardAsyncOp(work, 3, 0);
    if (sCardStores != 3) {
        (void)write(1, (u8 *)"card_filename_pointer_test: FAIL (save write did not complete)\n",
                    sizeof("card_filename_pointer_test: FAIL (save write did not complete)\n") - 1);
        return 4;
    }

    free(work);
    (void)write(1, (u8 *)"card_filename_pointer_test: PASS\n",
                sizeof("card_filename_pointer_test: PASS\n") - 1);
    return 0;
}
