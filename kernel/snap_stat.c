#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"
#include "stat.h"

int
sys_snap_status(void)
{
    int slot;
    struct proc *p = myproc();
    struct snap_info *si;
    char *ptr;

    if(argint(0, &slot) < 0)
        return -2;

    if(slot < 0 || slot >= MAX_SNAPS)
        return -2;

    if(argptr(1, &ptr, sizeof(struct snap_info)) < 0)
        return -2;

    si = (struct snap_info*)ptr;

    if(!p->snaps[slot].used)
        return -1;

    si->size = p->snaps[slot].sz;
    si->num_pages = p->snaps[slot].num_pages;

    return 0;



}
