#include "common.h"
#include "stag4000/stag4000.h"

extern int puts(const char *);

SysState Sys_State;

static CdCacheEntry sCacheEntry;
static s32 sAttachCount;

CdCacheEntry *Cd_FindCachedFile(s32 id) {
    return id == sCacheEntry.fileId ? &sCacheEntry : NULL;
}

ActorModel *Gfx_AttachModel(Actor *actor, s32 fileId) {
    (void)fileId;
    sAttachCount++;
    return actor->model;
}

s16 *Gfx_ModelBoneVerts(const ActorModel *model, s32 index) {
    (void)model;
    (void)index;
    return NULL;
}

s16 *Gfx_ModelBoneNormals(const ActorModel *model, s32 index) {
    (void)model;
    (void)index;
    return NULL;
}

ModelQuadSection *Gfx_ModelBonePolys(const ActorModel *model, s32 index) {
    (void)model;
    (void)index;
    return NULL;
}

#include "../src/stag4000/obj.c"

static int fail(const char *message) {
    puts(message);
    return 1;
}

int main(void) {
    Actor owner = { 0 };
    Stg40SideModel *side;
    const sptr firstData = (sptr)0x110000200ULL;
    const sptr reloadedData = (sptr)0x210000200ULL;

    sCacheEntry.fileId = 0x400;
    sCacheEntry.state = 3;
    sCacheEntry.data = firstData;

    side = Stg40_SideModelFor(&owner, sCacheEntry.fileId);
    if (side == NULL || sAttachCount != 1) {
        return fail("initial side-model cache build did not attach the model");
    }

    /* A CD reload can change the host pointer while preserving the same low 32 bits. */
    sCacheEntry.data = reloadedData;
    if ((u32)firstData != (u32)reloadedData) {
        return fail("test fixture must keep the legacy low pointer word unchanged");
    }
    side = Stg40_SideModelFor(&owner, sCacheEntry.fileId);
    if (side == NULL || sAttachCount != 2) {
        return fail("cache reload with matching low bits did not rebuild the side model");
    }

    puts("side_model_cache_pointer_test: PASS");
    return 0;
}
