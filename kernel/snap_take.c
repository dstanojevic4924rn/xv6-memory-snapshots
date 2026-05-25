#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

int
sys_snap_take(void)
{
    int slot;
    struct proc *p = myproc();

    if(argint(0, &slot) < 0)
        return -4;

    if(slot < 0 || slot >= MAX_SNAPS)
        return -2;
    if(p->snaps[slot].used)
        return -3;

    p->snaps[slot].pgdir = snapshot_copyuvm(p->pgdir, p->sz,&p->snaps[slot].num_pages);

    if(p->snaps[slot].pgdir == 0)
        return -1;

    p->snaps[slot].sz = p->sz;
    p->snaps[slot].used = 1;
    return 0;
}
