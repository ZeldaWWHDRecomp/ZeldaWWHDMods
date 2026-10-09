/* Host test double for the services used by persistence.c. */
#pragma once
typedef unsigned int u32;
typedef int s32;
void wwhd_log(const char*);
s32 wwhd_file_read(const char*,void*,u32);
s32 wwhd_file_write(const char*,const void*,u32);
#define WWHD_GAME_ORIGINAL(addr,ret,name,params) ret name params
#define WWHD_REPLACE(addr,ret,name,params) static ret name params
