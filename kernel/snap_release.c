#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

int
sys_snap_release(void)
{



    int slot;
    struct proc *p = myproc();

    if(argint(0, &slot) < 0)
        return -3;

    if(slot < 0 || slot >= MAX_SNAPS)
        return -2;
    if(!p->snaps[slot].used)
        return -1;

    freevm(p->snaps[slot].pgdir);
      p->snaps[slot].used = 0;
      p->snaps[slot].pgdir = 0;
      p->snaps[slot].sz = 0;
      p->snaps[slot].num_pages = 0;

      return 0;
}
