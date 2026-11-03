#include "common.h"
#include "stag2000/stag2000.h"
#include "stag2000/partsupgrade.h"
#include "stag2000/shoplist.h"

#include <stdio.h>
#include <sys/mman.h>

#define TEST_REGION_SIZE 0x1000
#define ITEM_NAME_OFFSET 0x200

u_char *Ps1_Ram;
GameState Save_GameState;
Stg20MenuState Stg20_MenuState;
static u8 sShopList[] = { 1, 0 };
static u8 *sItemName;

EntA0 *Cd_GetFileEntry(u32 id) {
    (void)id;
    return (EntA0 *)sShopList;
}

s32 Item_GetNameText(s32 itemId) {
    (void)itemId;
    return (s32)(uptr)sItemName;
}

s32 Item_GetPrice(s32 itemId) { return itemId * 100; }

static int fail(const char *message) {
    fprintf(stderr, "shop_item_name_pointer_test: %s\n", message);
    return 1;
}

int main(void) {
    u8 *region = mmap((void *)(uptr)0x110000000, TEST_REGION_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
    Actor actor = { 0 };
    Stg20ShopListWork work = { 0 };
    Stg20ItemWork upgradeWork = { 0 };

    if (region == MAP_FAILED) {
        return fail("could not allocate item-name data");
    }
    if ((uptr)region <= UINT32_MAX) {
        munmap(region, TEST_REGION_SIZE);
        return fail("test requires item-name data above the 32-bit address range");
    }

    Ps1_Ram = region;
    sItemName = region + ITEM_NAME_OFFSET;
    sItemName[0] = 0x2A;
    sItemName[1] = 0x2B;
    sItemName[2] = 0xFF;
    actor.work = (ActorWork *)&work;

    Stg20_LoadShopBuyList(&actor, 0);

    if (work.count != 1 || Stg20_ShopItems.items[0] != 1 ||
        Stg20_ShopItems.names[0][0] != 0x2A || Stg20_ShopItems.names[0][1] != 0x2B) {
        munmap(region, TEST_REGION_SIZE);
        return fail("vendor list did not copy the item name");
    }

    Save_GameState.slotItems[1] = 1;
    actor.work = (ActorWork *)&upgradeWork;
    Stg20_BuildUpgradeList(&actor);
    if (upgradeWork.recs[0].name[0] != 0x2A || upgradeWork.recs[0].name[1] != 0x2B) {
        munmap(region, TEST_REGION_SIZE);
        return fail("upgrade list did not restore the item-name pointer");
    }

    munmap(region, TEST_REGION_SIZE);
    puts("shop_item_name_pointer_test: PASS");
    return 0;
}
