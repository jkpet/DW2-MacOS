#ifndef STAG4000_ITEMSELECTION_H
#define STAG4000_ITEMSELECTION_H

#include "common.h"

void Stg40_ItemSelectionReset(void);
void Stg40_ItemSelectionSet(u8 *legacyIds, s32 index, s32 itemId);
s32 Stg40_ItemSelectionGet(s32 index);
void Stg40_ItemSelectionSetSelected(u8 *legacyId, s32 itemId);
s32 Stg40_ItemSelectionGetSelected(void);

#endif /* STAG4000_ITEMSELECTION_H */
