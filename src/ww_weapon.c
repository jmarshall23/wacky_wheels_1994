#include "ww_weapon.h"

#include <string.h>

enum {
    WW_WEAPON_AMMO_MAXIMUM = 0x63,
    WW_WEAPON_AMMO_PICKUP = 4,
    WW_HOGMIS_NORMAL_OFFSET = 0x2be,
    WW_HOGMIS_TRIPLE_OFFSET = 0x57c,
    WW_HOGMIS_SPLIT_OFFSET = 0x666,
    WW_HOGMIS_TYPE_5_OFFSET = 0x750,
    WW_HOGMIS_TYPE_6_OFFSET = 0x83a,
    WW_HOGMIS_TYPE_7_OFFSET = 0x924,
    WW_HOGMIS_TYPE_8_OFFSET = 0x1506
};

void ww_weapon_reset(WwWeaponState *state)
{
    if (state != NULL) memset(state, 0, sizeof(*state));
}

/* Positive SPRITE.ATR classification branch at loc_2324x.  Class two adds
 * four ordinary shots.  Classes three through eight replace an empty weapon
 * slot; class one is the separately tracked special pickup. */
bool ww_weapon_collect(WwWeaponState *state, int16_t classification,
                       bool *consumed)
{
    if (state == NULL || consumed == NULL) return false;
    *consumed = false;
    if (classification <= 0 || classification > 8) return true;
    if (classification == 1) {
        state->special_pickup = true;
        *consumed = true;
        return true;
    }
    if (state->ammunition >= WW_WEAPON_AMMO_MAXIMUM) return true;
    if (classification == 2) {
        unsigned ammunition = state->ammunition + WW_WEAPON_AMMO_PICKUP;
        if (ammunition > WW_WEAPON_AMMO_MAXIMUM) {
            ammunition = WW_WEAPON_AMMO_MAXIMUM;
        }
        state->ammunition = (uint16_t)ammunition;
        *consumed = true;
    } else if (state->selected_type == 0u) {
        state->selected_type = (uint16_t)classification;
        *consumed = true;
    }
    return true;
}

static unsigned ww_weapon_free_slots(const WwDynamicObjectPool *pool)
{
    unsigned count = 0;
    size_t i;
    for (i = 0; i < WW_DYNAMIC_OBJECT_CAPACITY; ++i) {
        if (!pool->object[i].active) ++count;
    }
    return count;
}

static uint16_t ww_weapon_opposite(uint16_t heading)
{
    heading = (uint16_t)(heading + WW_TRIG_ENTRY_COUNT / 2u);
    if (heading >= WW_TRIG_ENTRY_COUNT) {
        heading = (uint16_t)(heading - WW_TRIG_ENTRY_COUNT);
    }
    return heading;
}

static uint16_t ww_weapon_side(uint16_t heading, bool right)
{
    int result = heading + (right ? 0x1e0 : -0x1e0);
    if (result < 0) result += WW_TRIG_ENTRY_COUNT;
    if (result >= WW_TRIG_ENTRY_COUNT) result -= WW_TRIG_ENTRY_COUNT;
    return (uint16_t)result;
}

static bool ww_weapon_spawn(WwDynamicObjectPool *pool,
                            const WwRenderer *renderer,
                            const WwRacerState *player,
                            unsigned dynamic_type,
                            uint16_t heading_or_mode,
                            size_t source_offset, unsigned frames)
{
    return ww_dynamic_object_spawn(
        pool, WW_RACER_PLAYER_INDEX, dynamic_type,
        player->world_x, player->world_y, player->heading, heading_or_mode,
        source_offset, frames, renderer);
}

/* sub_24364's normal player branch.  The active-by-owner gate preserves its
 * one-volley-at-a-time rule, and the latch makes Space/Enter/controller A an
 * edge just like word +80h in the racer record. */
bool ww_weapon_update_fire(WwWeaponState *state, bool fire_down,
                           WwDynamicObjectPool *pool,
                           const WwRenderer *renderer,
                           const WwRacerState *player)
{
    unsigned needed;
    bool fired = false;
    if (state == NULL || pool == NULL || renderer == NULL || player == NULL ||
        !player->active || player->heading >= WW_TRIG_ENTRY_COUNT) {
        return false;
    }
    if (!fire_down) {
        state->fire_latched = false;
        return true;
    }
    if (state->fire_latched) return true;
    state->fire_latched = true;
    if (pool->active_by_owner[WW_RACER_PLAYER_INDEX] != 0u ||
        (state->selected_type == 0u && state->ammunition == 0u)) {
        return true;
    }
    needed = state->selected_type == 3u ? 3u :
             state->selected_type == 4u ? 2u : 1u;
    if (ww_weapon_free_slots(pool) < needed) return true;

    switch (state->selected_type) {
    case 0:
        fired = ww_weapon_spawn(pool, renderer, player, 0u, 0u,
                                WW_HOGMIS_NORMAL_OFFSET, 3u);
        break;
    case 3:
        fired = ww_weapon_spawn(pool, renderer, player, 0u, 0u,
                                WW_HOGMIS_TRIPLE_OFFSET, 1u) &&
                ww_weapon_spawn(pool, renderer, player, 0u, 1u,
                                WW_HOGMIS_TRIPLE_OFFSET, 1u) &&
                ww_weapon_spawn(pool, renderer, player, 0u, 2u,
                                WW_HOGMIS_TRIPLE_OFFSET, 1u);
        break;
    case 4:
        fired = ww_weapon_spawn(pool, renderer, player, 1u,
                                ww_weapon_side(player->heading, true),
                                WW_HOGMIS_SPLIT_OFFSET, 1u) &&
                ww_weapon_spawn(pool, renderer, player, 1u,
                                ww_weapon_side(player->heading, false),
                                WW_HOGMIS_SPLIT_OFFSET, 1u);
        break;
    case 5:
        fired = ww_weapon_spawn(pool, renderer, player, 2u,
                                ww_weapon_opposite(player->heading),
                                WW_HOGMIS_TYPE_5_OFFSET, 1u);
        break;
    case 6:
        fired = ww_weapon_spawn(pool, renderer, player, 3u,
                                ww_weapon_opposite(player->heading),
                                WW_HOGMIS_TYPE_6_OFFSET, 1u);
        break;
    case 7:
        fired = ww_weapon_spawn(pool, renderer, player, 4u,
                                ww_weapon_opposite(player->heading),
                                WW_HOGMIS_TYPE_7_OFFSET, 1u);
        break;
    case 8:
        fired = ww_weapon_spawn(pool, renderer, player, 5u, 0u,
                                WW_HOGMIS_TYPE_8_OFFSET, 3u);
        break;
    default:
        return true;
    }
    if (!fired) return false;
    if (state->selected_type == 0u) {
        --state->ammunition;
    } else {
        state->selected_type = 0u;
    }
    return true;
}
