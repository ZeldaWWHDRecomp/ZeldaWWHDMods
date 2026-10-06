#pragma once
#include <cstdint>
namespace gc_minimap {
bool enabled();
struct Snapshot { bool visible=false; int room=0; float x=0,z=0; int16_t yaw=0; };
void capture(uint32_t player);
Snapshot snapshot();
}
