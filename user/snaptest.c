//
// Created by Luka Markovic on 20. 5. 2026.
//
// snaptest.c — sveobuhvatan test za memory snapshot mehanizam (OS2026 D3)
//
// Dopune u odnosu na originalnu verziju:
//   [3]  + asimetricni diff: jedan slot pun, drugi prazan -> -1
//   [6]  + simetrija diff(a,b) == diff(b,a)
//   [7]  pojacan restore: pattern na 3 stranice + provera vracene VELICINE (sz)
//   [7b] NOVO: snap_take rollback pri nedostatku memorije (OOM) -> -1 + slot prazan
//   [8]  + fork deep-copy: nasledjeni snimak je nezavisna kopija sadrzaja
//   [8b] NOVO: snap_status sa nevalidnim pokazivacem -> -2 (best-effort)
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define PGSIZE 4096
#define NSLOTS 4

// Leak test (sekcija 9): da bi se LEAK zaista detektovao iscrpljivanjem
// memorije, ukupan "scurio" obim mora preci fizicku RAM (xv6 default ~224MB).
// 4 slota * LEAK_PAGES * LEAK_ITERS * 4KB treba da bude > RAM.
// Ako je predugo na tvojoj masini, smanji LEAK_ITERS.
#define LEAK_PAGES 80
#define LEAK_ITERS 200

int tests = 0, passed = 0;

static void
check(char *name, int got, int want)
{
    tests++;
    if (got == want) {
        passed++;
        printf("  [PASS] %s  (=%d)\n", name, got);
    } else {
        printf("  [FAIL] %s  : dobio %d, ocekivano %d\n", name, got, want);
    }
    sleep(100);
}

static void
checktrue(char *name, int cond, int val)
{
    tests++;
    if (cond) {
        passed++;
        printf("  [PASS] %s  (val=%d)\n", name, val);
    } else {
        printf("  [FAIL] %s  (val=%d)\n", name, val);
    }
}

static void
release_all(void)
{
    for (int i = 0; i < NSLOTS; i++)
        snap_release(i); // ignorisemo gresku (slot je mozda vec prazan)
}

// -----------------------------------------------------------------------------
// snap_restore: sadrzaj (na VISE stranica) se vraca + VELICINA se vraca +
// snimak prezivljava (visestruko vracanje).
//
// Radi i pod "rewind" semantikom (restore premota IP na snap_take) i pod
// cisto "linearnom" semantikom (restore samo zameni memoriju i vrati 0):
//   - 2 tokena u pipe-u: rewind ih potrosi oba (2 restore-a, pa izlaz),
//     linearna semantika potrosi 1 (if se izvrsi jednom, pa fall-through).
//   - U OBA slucaja poslednje izvrseno stanje je "posle restore-a", pa su
//     provere ispod validne.
// -----------------------------------------------------------------------------
static void
restore_test(void)
{
    int fd[2];
    struct snap_info si;
    int N = 3 * PGSIZE;                 // pattern preko 3 stranice (ne samo 1 bajt)
    char *m = malloc(N);
    if (m == 0) { printf("  [SKIP] restore_test: malloc pao\n"); return; }

    if (pipe(fd) < 0) { printf("  [SKIP] restore_test: pipe pao\n"); free(m); return; }

    // 2 tokena => tacno 2 "restore" prolaza (rewind), pa izlaz.
    write(fd[1], "..", 2);
    close(fd[1]);          // zatvoren write-kraj => read na prazno vraca 0 (ne blokira)

    for (int i = 0; i < N; i++) m[i] = (char)(i & 0xFF);  // stanje koje cuvamo

    uint sz0 = (uint)sbrk(0);          // velicina memorije U TRENUTKU snimka

    snap_take(0);          // <<<<<< TACKA P: cilj premotavanja >>>>>>

    char tk;
    if (read(fd[0], &tk, 1) == 1) {    // kernelski brojac — prezivljava restore
        sbrk(5 * PGSIZE);              // narasti memoriju POSLE snimka
        for (int i = 0; i < N; i++) m[i] = 0;  // pokvari sadrzaj (sve 3 stranice)
        snap_restore(0);               // vrati memoriju+velicinu i premotaj na P
        // do ovde se ne stize dok ima tokena (pod rewind semantikom)
    }
    close(fd[0]);

    // Stizemo ovde kad su tokeni potroseni; sve je vraceno na snimak.
    int ok = 1;
    for (int i = 0; i < N; i++)
        if (m[i] != (char)(i & 0xFF)) { ok = 0; break; }
        checktrue("restore vratio sadrzaj na SVE 3 stranice", ok, ok);

    uint sz1 = (uint)sbrk(0);
    check("restore vratio i VELICINU (sz==sz_snimka)", (int)sz1, (int)sz0);

    check("snimak preziveo restore (status==0)", snap_status(0, &si), 0);
    check("release posle restore -> 0", snap_release(0), 0);
    free(m);
}

// -----------------------------------------------------------------------------
int
main(int argc, char *argv[])
{
    struct snap_info si;

    printf("\n========== SNAPTEST ==========\n\n");

    // ---- 1) Pocetno stanje ----
    printf("[1] Pocetno stanje\n");
    release_all();
    check("snap_empty (sve prazno) -> 0", snap_empty(), 0);
    check("status praznog slota -> -1", snap_status(0, &si), -1);


    // ---- 2) Nevalidni slotovi (-2) ----
    printf("[2] Nevalidni brojevi slotova\n");
    check("take(-1) -> -2",     snap_take(-1),      -2);
    check("take(4)  -> -2",     snap_take(4),       -2);
    check("take(99) -> -2",     snap_take(99),      -2);
    check("restore(-1) -> -2",  snap_restore(-1),   -2);
    check("restore(4)  -> -2",  snap_restore(4),    -2);
    check("diff(-1,0) -> -2",   snap_diff(-1, 0),   -2);
    check("diff(0,4)  -> -2",   snap_diff(0, 4),    -2);
    check("release(7) -> -2",   snap_release(7),    -2);
    check("status(-1) -> -2",   snap_status(-1, &si), -2);

    // ---- 3) Operacije nad praznim slotom (+ asimetricni diff) ----
    printf("[3] Prazan slot\n");
    release_all();
    check("restore(0) prazno -> -1", snap_restore(0),    -1);
    check("diff(0,1) oba prazna -> -1",  snap_diff(0, 1),    -1);
    check("release(0) prazno -> -1", snap_release(0),    -1);
    check("status(0) prazno -> -1",  snap_status(0, &si), -1);

    // Spec: "Jedan od slotova je prazan -> -1". Hvata impl. koja proverava
    // (oba prazna) umesto (bar jedan prazan).
    check("take(0) za asimetricni diff -> 0", snap_take(0), 0);
    check("diff(pun, prazan) -> -1",          snap_diff(0, 1), -1);
    check("diff(prazan, pun) -> -1",          snap_diff(1, 0), -1);
    release_all();

    // ---- 4) take / duplikat / snap_empty / status ----
    printf("[4] take / duplikat / empty\n");
    release_all();
    check("take(0) -> 0",            snap_take(0),    0);
    check("take(0) opet -> -3",      snap_take(0),    -3);
    check("empty posle take(0) -> 1", snap_empty(),   1);
    check("status(0) zauzet -> 0",   snap_status(0, &si), 0);
    checktrue("status size > 0", si.size > 0, si.size);
    check("num_pages = ceil(size/PG)", si.num_pages, (si.size + PGSIZE - 1) / PGSIZE);

    check("take(1) -> 0", snap_take(1), 0);
    check("take(2) -> 0", snap_take(2), 0);
    check("take(3) -> 0", snap_take(3), 0);
    check("empty (sve puno) -> -1", snap_empty(), -1);
    check("take u pun slot(2) -> -3", snap_take(2), -3);
    release_all();

    // ---- 5) num_pages za NEPORAVNATU velicinu (ceil, NE floor) ----
    printf("[5] num_pages za neporavnatu velicinu (ceil vs floor)\n");
    release_all();
    {
        uint cur = (uint)sbrk(0);                 // sbrk(0) vraca trenutni sz
        uint pad = (PGSIZE - (cur % PGSIZE)) % PGSIZE;
        if (pad) sbrk((int)pad);
        sbrk(100);                                // sz = poravnato + 100

        int sl = snap_empty();
        check("take(neporavnato) -> 0", snap_take(sl), 0);
        check("status -> 0",            snap_status(sl, &si), 0);
        int ceil  = (si.size + PGSIZE - 1) / PGSIZE;
        int floor = si.size / PGSIZE;
        checktrue("size je neporavnat (ceil != floor)", ceil != floor, si.size % PGSIZE);
        check("num_pages == ceil(size/PG)  <- hvata floor-bug", si.num_pages, ceil);
        snap_release(sl);
    }

    // ---- 6) snap_diff: DETERMINISTICKE osobine (+ simetrija) ----
    printf("[6] snap_diff (deterministicke osobine)\n");
    release_all();
    {
        char *m = malloc(4 * PGSIZE);
        if (m == 0) {
            printf("  [SKIP] malloc pao\n");
        } else {
            snap_take(0);
            check("diff(0,0) self -> 0", snap_diff(0, 0), 0);   // jak, deterministican

            for (int i = 0; i < PGSIZE; i++) m[i] ^= 0xFF;      // promeni >= 1 stranicu
            snap_take(1);
            int d1 = snap_diff(0, 1);
            checktrue("posle promene: diff(0,1) >= 1", d1 >= 1, d1);
            check("simetrija: diff(1,0) == diff(0,1)", snap_diff(1, 0), d1);
            check("diff(1,1) self -> 0", snap_diff(1, 1), 0);

            for (int i = PGSIZE; i < 3 * PGSIZE; i++) m[i] ^= 0xFF;  // jos 2 stranice
            snap_take(2);
            int d2 = snap_diff(0, 2);
            checktrue("monotonost: diff(0,2) >= diff(0,1)", d2 >= d1, d2);
            checktrue("vise izmena: diff(0,2) >= 3", d2 >= 3, d2);
            check("simetrija: diff(2,0) == diff(0,2)", snap_diff(2, 0), d2);

            release_all();
            free(m);
        }
    }

    // ---- 6b) snap_diff: razlika u VELICINI (visak stranica se broji) ----
    printf("[6b] snap_diff razlika u velicini\n");
    release_all();
    {
        uint cur = (uint)sbrk(0);
        uint pad = (PGSIZE - (cur % PGSIZE)) % PGSIZE;
        if (pad) sbrk((int)pad);                  // poravnaj pre snimka

        struct snap_info a, b;
        snap_take(0);
        snap_status(0, &a);
        sbrk(3 * PGSIZE + 100);                   // +4 nove stranice (3 pune + delimicna)
        snap_take(1);
        snap_status(1, &b);

        int extra = b.num_pages - a.num_pages;    // ocekivano 4 (ako je status korektan)
        int d = snap_diff(0, 1);
        printf("    (info) pages: %d -> %d | extra(ocekivano)=%d | diff(dobijeno)=%d\n",
               a.num_pages, b.num_pages, extra, d);
        checktrue("diff broji visak stranica (diff >= extra)", d >= extra, d);
        checktrue("diff >= 4 (4 nove stranice)", d >= 4, d);

        release_all();
    }

    // ---- 7) snap_restore (rewind-safe, multi-page + velicina) ----
    printf("[7] snap_restore (sadrzaj na vise stranica + vracena velicina)\n");
    release_all();
    restore_test();

    // ---- 7b) snap_take rollback pri nedostatku memorije (OOM) ----
    // Spec (4 poena za take): ako kopiranje pukne usred, kernel mora da uradi
    // ROLLBACK — delimicno alocirani resursi se oslobadjaju, slot ostaje u
    // prethodnom (praznom) stanju, vraca se -1, sistem ostaje stabilan.
    //
    // Radi se u detetu da iscrpljivanje RAM-a ne srusi glavni test.
    // Pojedemo svu slobodnu fizicku memoriju (sbrk dok ne pukne), pa take(0)
    // MORA da vrati -1 (nema stranica za kopiju) i da ostavi slot prazan.
    printf("[7b] snap_take OOM -> -1 + rollback (slot ostaje prazan)\n");
    release_all();
    {
        int pid = fork();
        if (pid < 0) {
            checktrue("fork() za OOM test uspeo", 0, pid);
        } else if (pid == 0) {
            // pojedi svu slobodnu fizicku memoriju
            while (sbrk(8 * PGSIZE) != (char*)-1)
                ;
            int r = snap_take(0);          // mora -1: nema fizickih stranica za kopiju
            struct snap_info ci;
            if (r == -1 && snap_status(0, &ci) == -1)
                printf("  [child] [PASS] take OOM -> -1 i rollback (slot prazan)\n");
            else if (r == -1)
                printf("  [child] [FAIL] take -> -1 ali slot NIJE prazan (los rollback)\n");
            else
                printf("  [child] [INFO] take -> %d (RAM ipak dovoljan? OOM grana neproverena)\n", r);
            exit();
        } else {
            wait();
            // roditelj mora ostati potpuno netaknut posle OOM-deteta
            check("[parent] posle OOM-deteta: empty -> 0", snap_empty(), 0);
            release_all();
        }
    }

    // ---- 8) fork: nasledjivanje + nezavisnost + deep-copy sadrzaja ----
    printf("[8] fork nasledjuje snimke (nezavisne kopije sadrzaja)\n");
    release_all();
    {
        // u roditelju upisemo prepoznatljiv pattern pa snimimo slot 0
        int N = 2 * PGSIZE;
        char *m = malloc(N);
        if (m == 0) { printf("  [SKIP] malloc pao\n"); goto fork_done; }
        for (int i = 0; i < N; i++) m[i] = (char)(0xA5 ^ (i & 0xFF));

        check("take(0) pattern u roditelju -> 0", snap_take(0), 0);
        check("take(1) u roditelju -> 0", snap_take(1), 0);

        int pid = fork();
        if (pid < 0) {
            checktrue("fork() uspeo", 0, pid);
        } else if (pid == 0) {
            struct snap_info ci;

            // (a) nasledjivanje slotova
            int s = snap_status(1, &ci);
            if (s == 0) printf("  [child] [PASS] nasledjen slot 1\n");
            else        printf("  [child] [FAIL] slot 1 NIJE nasledjen (%d)\n", s);

            // (b) deep-copy: nasledjeni snimak (slot 0 = pattern) mora biti
            //     NEZAVISNA kopija. Dete pregazi tekucu memoriju, snimi je u
            //     slot 2 i uporedi sa nasledjenim slotom 0. Ako je snimak prava
            //     kopija starog patterna, mora se razlikovati od nove memorije.
            for (int i = 0; i < N; i++) m[i] = 0x00;     // nova memorija != pattern
            if (snap_take(2) == 0) {
                int d = snap_diff(0, 2);
                if (d >= 1)
                    printf("  [child] [PASS] nasledjen snimak je nezavisna kopija (diff>=1, d=%d)\n", d);
                else
                    printf("  [child] [FAIL] nasledjen snimak NIJE prava kopija (diff=%d)\n", d);
            } else {
                printf("  [child] [FAIL] dete ne moze da snimi slot 2\n");
            }

            // (c) nezavisnost: release/take u detetu ne sme uticati na roditelja
            snap_release(1);
            int r = snap_take(3);
            if (r == 0) printf("  [child] [PASS] dete radi samostalno (take 3)\n");
            else        printf("  [child] [FAIL] dete take(3) -> %d\n", r);
            exit();
        } else {
            wait();
            check("[parent] slot 1 i dalje zauzet -> 0", snap_status(1, &si), 0);
            check("[parent] slot 2 i dalje prazan -> -1", snap_status(2, &si), -1);
            check("[parent] slot 3 i dalje prazan -> -1", snap_status(3, &si), -1);
            release_all();
        }
        free(m);
    }
    fork_done:;

    // ---- 8b) snap_status sa nevalidnim pokazivacem -> -2 (best-effort) ----
    // Spec: status vraca -2 za "ostale greske". Kernel mora preko copyout da
    // proveri korisnicki pokazivac i vrati -2, NE da pukne.
    // NAPOMENA: ako impl ne validira pokazivac (npr. direktan upis *si=...),
    //           dete ce biti ubijeno trap-om i necemo videti [PASS] — to je
    //           samo po sebi koristan signal da fali provera pokazivaca.
    printf("[8b] snap_status nevalidan pokazivac -> -2 (best-effort)\n");
    release_all();
    {
        int pid = fork();
        if (pid < 0) {
            checktrue("fork() za badptr test uspeo", 0, pid);
        } else if (pid == 0) {
            snap_take(0);   // slot mora biti pun da bismo dosli do copyout-a
            // pokazivac u kernelski opseg (>= KERNBASE) -> copyout mora pasti
            struct snap_info *bad = (struct snap_info *)0xFFFFFFF0;
            int r = snap_status(0, bad);
            if (r == -2) printf("  [child] [PASS] badptr -> -2 (validacija radi)\n");
            else         printf("  [child] [FAIL] badptr -> %d (ocekivano -2)\n", r);
            exit();
        } else {
            wait();
            // ako se dete srusilo, gore [PASS] linija nije ispisana -> fali validacija
            check("[parent] preziveo badptr test (slot 0 prazan u roditelju)",
                  snap_status(0, &si), -1);
        }
    }

    // ---- 9) exit oslobadja sve snimke (leak / stres test) ----
    printf("[9] exit oslobadja snimke (leak test: %d iter x %d str x %d slota)\n",
           LEAK_ITERS, LEAK_PAGES, NSLOTS);
    release_all();
    {
        int ok = 1;
        for (int it = 0; it < LEAK_ITERS && ok; it++) {
            int pid = fork();
            if (pid < 0) { ok = 0; printf("  [FAIL] fork pao na iter %d (leak?)\n", it); break; }
            if (pid == 0) {
                if (sbrk(LEAK_PAGES * PGSIZE) == (char*)-1) exit();
                for (int s = 0; s < NSLOTS; s++) {
                    if (snap_take(s) != 0) {
                        printf("  [FAIL] snap_take pao u detetu -> LEAK u exit()\n");
                        exit();
                    }
                }
                exit();   // exit MORA da oslobodi sva 4 snimka
            }
            wait();
        }
        checktrue("leak test (memorija se reciklira kroz exit)", ok, ok);
    }

    printf("\n========== REZIME: %d / %d PASS ==========\n\n", passed, tests);
    exit();
}
