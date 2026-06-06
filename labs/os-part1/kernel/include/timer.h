#ifndef __TIMER_H
#define __TIMER_H

#include "types.h"
#include "spinlock.h"

extern struct spinlock tickslock;
extern uint ticks;

void timerinit();
void set_next_timeout();
void timer_tick();

struct tms
{
    uint64 utime;  // user time
    uint64 stime;  // system time
    uint64 cutime; // user time of children
    uint64 cstime; // system time of children
};

#endif
