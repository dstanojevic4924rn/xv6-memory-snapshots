#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

int
sys_snap_diff(void)
{
    int slot1, slot2;
    struct proc *p = myproc();

    if(argint(0, &slot1) < 0 || argint(1, &slot2) < 0)
        return -3;

    if(slot1 < 0 || slot1 >= MAX_SNAPS || slot2 < 0 || slot2 >= MAX_SNAPS)
        return -2;

    if(!p->snaps[slot1].used || !p->snaps[slot2].used)
        return -1;

    // return snapshot_diff(p->snaps[slot1].pgdir, p->snaps[slot2].pgdir);

    return snapshot_diff(p->snaps[slot1].pgdir, p->snaps[slot1].sz,
                         p->snaps[slot2].pgdir, p->snaps[slot2].sz);
}
