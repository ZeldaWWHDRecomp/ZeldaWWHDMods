/* SPDX-License-Identifier: MPL-2.0 */
#ifndef EC_EDITOR_GEOMETRY_H
#define EC_EDITOR_GEOMETRY_H
typedef struct { float x,y,w,h; } EcRect;
static inline float ec_d2(float ax,float ay,float bx,float by) { float x=ax-bx,y=ay-by; return x*x+y*y; }
static inline int ec_inside(EcRect r,float x,float y) { return x>=r.x-r.w/2 && x<=r.x+r.w/2 && y>=r.y-r.h/2 && y<=r.y+r.h/2; }
#endif
