#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define PGSIZE 4096
#define NSLOTS 4
#define LEAK_PAGES 80
#define LEAK_ITERS 200


int
main(int argc, char *argv[])
{
    // sbrk(4096);
    snap_take(atoi(argv[1]));

    // sbrk(4096);
    // printf("aa\n");
    int a = snap_count_readonly();
    printf("%d\n", a);
    sbrk(4096);
    int b = snap_count_readonly();
    printf("%d\n", b);

    exit();
}
