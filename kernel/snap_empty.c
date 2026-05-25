#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

int
sys_snap_empty(void)
{
    struct proc *p = myproc();
    int i;

    for(i = 0; i < MAX_SNAPS; i++){
        if(!p->snaps[i].used)
            return i;
    }
    return -1;
}
