/* Tests of jtag.c
 * as described on
 * http://www.fpga4fun.com/JTAG2.html
 *
 * gcc -o benchtest -g -Wall benchtest.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef PPDEV
#  include <stdarg.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <linux/ioctl.h>
#  include <linux/ppdev.h>
#endif

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

#ifdef  PPDEV
static char *ppname = NULL;
static int ppfd = -1;

static
void die(const char *m, ...)
{
    va_list arg;
    va_start(arg,m);
    vprintf(m, arg);
    va_end(arg);
    exit(1);
}

// data
#define TMS 1
#define TCK 2
#define TDI 3
// status
#define TDO 7

static
void jtag_sim(struct ppsdev *dev)
{
    unsigned char data = (dev->tms<<TMS) | (dev->tdi<<TDI) | (dev->tck<<TCK);
    if(ppfd==-1) {
        ppfd = open(ppname, O_RDWR);
        if(ppfd==-1) die("Failed to open %s\n", ppname);
        if(ioctl(ppfd, PPCLAIM)==-1) die("Failed to claim port\n");
    }
    if(ioctl(ppfd, PPWDATA, &data)==-1) die("Failed to write data\n");
    if(ioctl(ppfd, PPRSTATUS, &data)==-1) die("Failed to read status\n");
    dev->tdo = !(data&(1<<TDO)); // status is hardware inverted
//    printf("<< %d%d%d\n>> %d\n", dev->tck, dev->tms, dev->tdi, dev->tdo);
}
#else /* PPDEV */
#  include "jtag.c"
#endif /* PPDEV */

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
//        pp->tck = 0;
//        jtag_sim(pp);
    }
}

#define MAXIR 100

int main(int argc, char *argv[])
{
    int n, irlen, ndev;

#ifdef PPDEV
    if(argc<2) die("Usage: %s /dev/parport#\n", argv[0]);
    ppname = argv[1];
#endif

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
        printf("\nIR length %d\n", irlen);
    } else {
        printf("\nError (%d)\n", n);
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

    printf("Find device IDs\n");

    jtag_clock(1, 0, 5); // reset again to reload IDCODE (assumed default IR)

    // reset -> shift DR
    jtag_clock(0, 0, 1);
    jtag_clock(1, 0, 1);
    jtag_clock(0, 0, 2);

    for(n=0; n<ndev; n++) {
        int b=32;
        u32 id = 0;
        while(b--) {
            jtag_clock(0,1,1);
            id >>= 1;
            if(pp->tdo) id |= 1<<31;
        }
        printf(" %d %08x\n", n, id);
    }

    return 0;
}
