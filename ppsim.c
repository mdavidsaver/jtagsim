/** Simulation of Urjtag minimal parallel port JTAG programmer.
 *
 */
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/init.h>

#include <linux/parport.h>

struct {
    struct parport *port;
}ppsimdev;

/*
 * Urjtag minimal places
 *  host -> device signals in the data byte
 * (data direction output)
 */
#define TMS 1
#define TCK 2
#define TDI 3

/* device -> host signal is in the status byte
 */
#define TDO 7

/* Only the following IOCTLs are used by
 * the ppdev driver
 *
 * PPCLAIM
 * PPRELEASE
 * PPWDATA    parport_write_data()    ops->write_data()
 * PPRDATA    parport_read_data()     ops->read_data()
 * PPRSTATUS  parport_read_status()   ops->read_status()
 * PPWCONTROL parport_write_control() ops->write_control()
 */

static void ppsim_write_data(struct parport *dev, unsigned char d)
{
    printk(KERN_INFO "ppsim write data %02x\n", d);
}
static void ppsim_write_control(struct parport *dev, unsigned char d)
{
    printk(KERN_INFO "ppsim write control %02x\n", d);
}

static unsigned char ppsim_read_data(struct parport *dev) {return 0;}
static unsigned char ppsim_read_status(struct parport *dev) {return 0;}
static unsigned char ppsim_read_control(struct parport *dev) {return 0;}

static unsigned char ppsim_frob_control(struct parport *dev, unsigned char mask,
                               unsigned char val)
{
    mask &= 0xf;
    val &= 0xf;
    // write (control & ~mask) ^ val
    printk(KERN_INFO "ppsim frob control %02x\n", 0 ^ val);
    return 0 ^ val;
}

static void ppsim_noop(struct parport *dev) {}
static void ppsim_nostate(struct pardevice *dev, struct parport_state *s) {}
static void ppsim_nopstate(struct parport *dev, struct parport_state *s) {}

static struct parport_operations simops = {
    .owner = THIS_MODULE,

    .write_data    = ppsim_write_data,
    .read_data     = ppsim_read_data,
    .read_status   = ppsim_read_status,
    .write_control = ppsim_write_control,

    .read_control  = ppsim_read_control,
    .frob_control  = ppsim_frob_control,

    .enable_irq    = ppsim_noop,
    .disable_irq   = ppsim_noop,
    .data_forward  = ppsim_noop,
    .data_reverse  = ppsim_noop,

    .init_state    = ppsim_nostate,
    .save_state    = ppsim_nopstate,
    .restore_state = ppsim_nopstate,

//    .compat_write_data = parport_ieee1284_write_compat,
//    .nibble_read_data = parport_ieee1284_read_nibble,
//    .byte_read_data   = parport_ieee1284_read_byte,
};

static int __init ppsim_init(void)
{
    ppsimdev.port = parport_register_port(0, PARPORT_IRQ_NONE,
                                 PARPORT_DMA_NONE,
                                 &simops);
    if(!ppsimdev.port) {
        printk(KERN_ERR "Failed to register parallel port\n");
        return -EIO;
    }
    printk(KERN_INFO "Initialize simulated parallel port\n");
    ppsimdev.port->modes = PARPORT_MODE_PCSPP;
    parport_announce_port(ppsimdev.port);
    return 0;
}
module_init(ppsim_init);

static void __exit ppsim_exit(void)
{
    parport_remove_port(ppsimdev.port);
    printk(KERN_INFO "Removed simulated parallel port\n");
}
module_exit(ppsim_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Davidsaver <mdavidsaver@gmail.com");
MODULE_DESCRIPTION("Simulated Urjtag minimal parallel port programmer");
