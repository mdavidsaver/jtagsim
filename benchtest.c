/* Tests of jtag.c
 * as described on
 * http://www.fpga4fun.com/JTAG2.html
 *
 * gcc -o benchtest -g -Wall benchtest.c
 */
#include <stdio.h>
#include <stdint.h>

struct ppsdev {
    unsigned int tms:1;
    unsigned int tck:1;
    unsigned int tdi:1;
    unsigned int tdo:1;
};
static struct ppsdev thedev;
static struct ppsdev * const pp = &thedev;

#define printk printf
#define KERN_INFO
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "jtag.c"


static
void jtag_clock(int tms, int tdi, int N)
{
    while(N--) {
        pp->tms = tms;
        pp->tdi = tdi;
        pp->tck = 0;
        jtag_sim(pp);
        pp->tck = 1;
        jtag_sim(pp);
        pp->tck = 0;
        jtag_sim(pp);
    }
}

#define MAXIR 100

int main(int argc, char *argv[])
{
    int n, irlen, ndev;

    printf("Find IR length\n");

    jtag_clock(1, 0, 5); // reset

    // go from reset to shift IR
    jtag_clock(0, 0, 1);
    jtag_clock(1, 0, 2);
    jtag_clock(0, 0, 2);

    // Fill with 1's
    jtag_clock(0, 1, MAXIR);
    // Shift through zeros until we see them re-appear
    for(n=0; n<MAXIR; n++) {
        jtag_clock(0, 0, 1);
        printf(" %d %d\n", n, pp->tdo);
        if(!pp->tdo)
            break;
    }
    if(!pp->tdo && n>0) {
        irlen = n;
        printf("IR length %d\n", irlen);
    } else {
        printf("Error (%d)\n", n);
        return 1;
    }

    printf("Count devices\n");

    // Fill IR with bypass
    jtag_clock(0, 1, irlen-1);
    jtag_clock(1, 1, 1); // go to exit1 IR

    // exit1 IR to shift DR
    jtag_clock(1, 0, 2);
    jtag_clock(0, 0, 2);

    // Fill DR with zeros
    jtag_clock(0, 0, 32);
    // Fill with ones until we see them come out
    // the other side
    for(n=0; n<64; n++) {
        jtag_clock(0, 1, 1);
        printf(" %d %d\n", n, pp->tdo);
        if(pp->tdo)
            break;
    }
    if(pp->tdo) {
        ndev = n;
        printf("Found %d devices\n", ndev);
    } else {
        printf("Found NO devices\n");
        return 1;
    }

    return 0;
}
