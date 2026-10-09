#pragma once
#include "ppc.h"
namespace dragon {
bool enabled();
struct Hud { int phase=-1; float altitude=0, minimum=800, maximum=16000; int notes=0; bool conducting=false; };
Hud hud();
void execute(Cpu*, PpcFunc original);
void song_judge(Cpu*, PpcFunc original);
void tact(Cpu*, PpcFunc original);
void solid_heap(Cpu*, PpcFunc original);
void actor_heap(Cpu*, PpcFunc original);
void actor_create(Cpu*, PpcFunc original);
void actor_execute(Cpu*, PpcFunc original);
void actor_draw(Cpu*, PpcFunc original);
void actor_delete(Cpu*, PpcFunc original);
void camera(Cpu*, PpcFunc original);
void position(Cpu*, PpcFunc original);
void resource_phase(Cpu*, PpcFunc original);
void resource_get(Cpu*, PpcFunc original);
}
