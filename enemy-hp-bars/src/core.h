/* SPDX-License-Identifier: MPL-2.0 */
#ifndef ENEMY_HP_CORE_H
#define ENEMY_HP_CORE_H
#define EHP_ACTORS 128
#define EHP_POPUPS 64
#define EHP_BARS 32
#define EHP_LIFETIME 1.0f
typedef struct { float x, y, z; } EhpVec;
typedef struct { unsigned address, id, profile; } EhpKey;
typedef struct {
    EhpKey key;
    EhpVec position;
    int used, hp, max_hp;
    float unseen, since_hit;
} EhpEnemy;
typedef struct { EhpVec position; int damage; float age; unsigned serial; } EhpPopup;
typedef struct {
    EhpEnemy enemies[EHP_ACTORS];
    EhpPopup popups[EHP_POPUPS];
    unsigned cursor;
} EhpState;
void ehp_clear(EhpState *s);
void ehp_tick(EhpState *s, float dt);
void ehp_observe(EhpState *s, EhpKey key, EhpVec position, int hp, int maximum, int numbers);
void ehp_forget(EhpState *s, EhpKey key);
float ehp_distance2(EhpVec a, EhpVec b);
int ehp_nearest(const EhpState *s, EhpVec player, float distance, int after_hit, int *indices);
#endif
