#include "common.h"
#include "main/digidata.h"

extern int puts(const char *);

static int multiplier_percent = 100;

int Host_ExpMultiplierPercent(void) {
    return multiplier_percent;
}

static int check(const char *name, s32 got, s32 want) {
    if (got != want) {
        puts(name);
        return 0;
    }
    return 1;
}

int main(void) {
    static const int multipliers[] = { 100, 200, 500, 1000, 2000 };
    static const s32 scaled[] = { 1234, 2468, 6170, 12340, 24680 };
    int ok = 1;
    int i;

    for (i = 0; i < (int)(sizeof(multipliers) / sizeof(multipliers[0])); i++) {
        multiplier_percent = multipliers[i];
        ok &= check("scaled gain", Digi_ScaleExpGain(1234), scaled[i]);
    }
    multiplier_percent = 2000;
    ok &= check("scaled gain cap", Digi_ScaleExpGain(99999999), 99999999);
    ok &= check("negative gain unchanged", Digi_ScaleExpGain(-25), -25);
    ok &= check("total EXP cap", Digi_AddExp(99999990, 20), 99999999);
    ok &= check("total EXP wide addition", Digi_AddExp(2000000000, 2000000000), 99999999);
    ok &= check("multi-level threshold count", Digi_CountLevelUps(1, 5, 57), 4);
    ok &= check("max-level threshold stop", Digi_CountLevelUps(1, 4, 57), 3);
    ok &= check("no earned level", Digi_CountLevelUps(1, 100, 5), 0);

    if (!ok) {
        return 1;
    }
    puts("exp_gain_test: all passed");
    return 0;
}
