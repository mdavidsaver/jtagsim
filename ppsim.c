/** Simulation of Urjtag minimal parallel port JTAG programmer.
 *
 */
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/parport.h>

#ifndef CONFIG_PARPORT_NOT_PC
# error CONFIG_PARPORT_NOT_PC must be defined
#endif

struct ppsdev {
    struct parport *port;
    unsigned char datareg, controlreg;
    unsigned int tms:1;
    unsigned int tck:1;
    unsigned int tdi:1;
    unsigned int tdo:1;
    spinlock_t lock;
};

/*
 * Urjtag minimal places
 *  host -> device signals in the data byte
 * (data direction output)
 */
#define TMS (1<<1)
#define TCK (1<<2)
#define TDI (1<<3)

/* device -> host signal is in the status byte
 */
#define TDO (1<<7)


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
    struct ppsdev *pp = dev->private_data;
    spin_lock(&pp->lock);
    pp->datareg = d;
    spin_unlock(&pp->lock);
    printk(KERN_INFO "ppsim write data %02x\n", (int)d);
}
static void ppsim_write_control(struct parport *dev, unsigned char d)
{
    struct ppsdev *pp = dev->private_data;
    spin_lock(&pp->lock);
    pp->controlreg = d;
    spin_unlock(&pp->lock);
    printk(KERN_INFO "ppsim write control %02x\n", (int)d);
}

static unsigned char ppsim_read_data(struct parport *dev)
{
    unsigned char ret;
    struct ppsdev *pp = dev->private_data;
    spin_lock(&pp->lock);
    ret = pp->datareg;
    spin_unlock(&pp->lock);
    return ret;
}
static unsigned char ppsim_read_status(struct parport *dev) {return 0;}
static unsigned char ppsim_read_control(struct parport *dev)
{
    unsigned char ret;
    struct ppsdev *pp = dev->private_data;
    spin_lock(&pp->lock);
    ret = pp->controlreg;
    spin_unlock(&pp->lock);
    return ret;
}

static unsigned char ppsim_frob_control(struct parport *dev, unsigned char mask,
                               unsigned char val)
{
    unsigned char ret;
    struct ppsdev *pp = dev->private_data;

    mask &= 0xf;
    val &= 0xf;

    spin_lock(&pp->lock);
    pp->controlreg = ret = (pp->controlreg & ~mask) ^ val;
    spin_unlock(&pp->lock);
    // write (control & ~mask) ^ val
    printk(KERN_INFO "ppsim frob control to %02x (%02x %02x)\n", (int)ret, (int)mask, (int)val);
    return ret;
}

static void ppsim_noop(struct parport *dev) {}

static void ppsim_initstate(struct pardevice *dev, struct parport_state *s)
{
    s->u.pc.ctr = 0;
    s->u.pc.ecr = 0;
}

static void ppsim_savestate(struct parport *dev, struct parport_state *s)
{
    struct ppsdev *pp = dev->private_data;
    spin_lock(&pp->lock);
    s->u.pc.ctr = pp->controlreg;
    s->u.pc.ecr = pp->datareg;
    spin_unlock(&pp->lock);
}
static void ppsim_restorestate(struct parport *dev, struct parport_state *s)
{
    struct ppsdev *pp = dev->private_data;
    spin_lock(&pp->lock);
    pp->controlreg = s->u.pc.ctr;
    pp->datareg = s->u.pc.ecr;
    spin_unlock(&pp->lock);
}

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

    .init_state    = ppsim_initstate,
    .save_state    = ppsim_savestate,
    .restore_state = ppsim_restorestate,

//    .compat_write_data = parport_ieee1284_write_compat,
//    .nibble_read_data = parport_ieee1284_read_nibble,
//    .byte_read_data   = parport_ieee1284_read_byte,
};

static struct ppsdev *onedev;

static int __init ppsim_init(void)
{
    struct ppsdev *dev;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if(!dev)
        return -ENOMEM;

    dev->port = parport_register_port(0, PARPORT_IRQ_NONE,
                                 PARPORT_DMA_NONE,
                                 &simops);
    if(!dev->port) {
        kzfree(dev);
        printk(KERN_ERR "Failed to register parallel port\n");
        return -EIO;
    }
    spin_lock_init(&dev->lock);
    printk(KERN_INFO "Initialize simulated parallel port\n");
    dev->port->modes = PARPORT_MODE_PCSPP;
    dev->port->private_data = dev;

    parport_announce_port(dev->port);
    onedev = dev;
    return 0;
}
module_init(ppsim_init);

static void __exit ppsim_exit(void)
{
    struct ppsdev *dev = onedev;
    onedev = NULL;
    parport_remove_port(dev->port);
    kzfree(dev);
    printk(KERN_INFO "Removed simulated parallel port\n");
}
module_exit(ppsim_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Davidsaver <mdavidsaver@gmail.com");
MODULE_DESCRIPTION("Simulated Urjtag minimal parallel port programmer");
