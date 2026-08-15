#ifndef WW_WEAPON_H
#define WW_WEAPON_H

#include "ww_dynamic_object.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct WwWeaponState {
    uint16_t selected_type;
    uint16_t ammunition;
    bool special_pickup;
    bool fire_latched;
} WwWeaponState;

void ww_weapon_reset(WwWeaponState *state);
bool ww_weapon_collect(WwWeaponState *state, int16_t classification,
                       bool *consumed);
bool ww_weapon_update_fire(WwWeaponState *state, bool fire_down,
                           WwDynamicObjectPool *pool,
                           const WwRenderer *renderer,
                           const WwRacerState *player);

#endif
