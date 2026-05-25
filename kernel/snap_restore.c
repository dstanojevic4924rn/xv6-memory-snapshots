#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

int
sys_snap_restore(void)
{
    int slot;
    struct proc *p = myproc();
    pde_t *newpgdir;
    int dummy;

    if(argint(0, &slot) < 0)
        return -3;
    if(slot<0 || slot >= MAX_SNAPS)
        return -2;
    if(!p->snaps[slot].used)
        return -1;

    // newpgdir = snapshot_copyuvm(p->snaps[slot].pgdir, &dummy);
        newpgdir = snapshot_copyuvm(p->snaps[slot].pgdir, p->snaps[slot].sz, &dummy);

    if(newpgdir == 0)
        return -3;

    pde_t *oldpgdir = p->pgdir;

    p->pgdir = newpgdir;
    p->sz  =p->snaps[slot].sz;

    lcr3(V2P(p->pgdir));

    freevm(oldpgdir);
    return 0;
}
