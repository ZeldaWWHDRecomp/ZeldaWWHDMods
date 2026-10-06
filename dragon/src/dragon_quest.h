#pragma once
#include "ppc.h"
#include <string>
namespace dragon::quest {
struct Hud { bool visible=false; unsigned revision=0; std::string title,line1,line2,line3,action; };
Hud hud();
bool tick(Cpu*,uint32_t player,uint32_t pressed,bool dragonIdle);
bool song_allowed(uint32_t player);
void learned();
void collect(Cpu*,PpcFunc);
void card_load(Cpu*,PpcFunc);
void card_new(Cpu*,PpcFunc);
void gong_create(Cpu*,PpcFunc);
void gong_execute(Cpu*,PpcFunc);
void gong_delete(Cpu*,PpcFunc);
void medli_create(Cpu*,PpcFunc);
void medli_execute(Cpu*,PpcFunc);
void medli_delete(Cpu*,PpcFunc);
}
