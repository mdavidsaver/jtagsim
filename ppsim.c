/** Simulation of Urjtag minimal parallel port JTAG programmer.
 *
 */
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/spinlock.h>

#include <linux/parport.h>

#ifndef CONFIG_PARPORT_NOT_PC
# error CONFIG_PARPORT_NOT_PC must be defined
#endif

struct ppcable {
    const char *name;
    // output (host -> devices signals)
    //  may be in data or control bytes
    unsigned char tms_data, tms_ctrl;
    unsigned char tck_data, tck_ctrl;
    unsigned char tdi_data, tdi_ctrl;
    // input (device -> host)
    // may be in data or status bytes
    unsigned char tdo_data, tdo_sts;
};

static const struct ppcable cables[];
static char *cablename = "Minimal";
module_param_named(cable, cablename, charp, 0444);
MODULE_PARM_DESC(cable, "Name of JTAG parallel port cable to emulate");

struct ppsdev {
    struct parport *port;
    const struct ppcable *cable;
    unsigned int enable:1;
    unsigned int tms:1;
    unsigned int tck:1;
    unsigned int tdi:1;
    unsigned int tdo:1;
    spinlock_t lock;
};

// Simulation
#include "jtag.c"

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
    unsigned int tms, tck, tdi;
    spin_lock(&pp->lock);
    if(!pp->enable) {
        spin_unlock(&pp->lock);
        return;
    }
    tms = pp->tms; tck = pp->tck; tdi = pp->tdi;
    if(pp->cable->tms_data)
        tms = pp->tms = !!(d & pp->cable->tms_data);
    if(pp->cable->tck_data)
        tck = pp->tck = !!(d & pp->cable->tck_data);
    if(pp->cable->tdi_data)
        tdi = pp->tdi = !!(d & pp->cable->tdi_data);
    // Don't write TDO
    jtag_sim(pp);
    spin_unlock(&pp->lock);
//    printk(KERN_INFO "ppsim dwrite(%02x) TMS=%d TCK=%d TDI=%d\n", (unsigned)d, tms, tck, tdi);
}
static void ppsim_write_control(struct parport *dev, unsigned char d)
{
    struct ppsdev *pp = dev->private_data;
    unsigned int tms, tck, tdi;
    spin_lock(&pp->lock);
    if(!pp->enable) {
        spin_unlock(&pp->lock);
        return;
    }
    d ^= 0x0b; // bits are hardware inverted
    tms = pp->tms; tck = pp->tck; tdi = pp->tdi;
    if(pp->cable->tms_ctrl)
        tms = pp->tms = !!(d & pp->cable->tms_ctrl);
    if(pp->cable->tck_ctrl)
        tck = pp->tck = !!(d & pp->cable->tck_ctrl);
    if(pp->cable->tdi_ctrl)
        tdi = pp->tdi = !!(d & pp->cable->tdi_ctrl);
    jtag_sim(pp);
    spin_unlock(&pp->lock);
//    printk(KERN_INFO "ppsim cwrite(%02x) TMS=%d TCK=%d TDI=%d\n", (unsigned)d, tms, tck, tdi);
}

static unsigned char ppsim_read_data(struct parport *dev)
{
    unsigned char ret = 0;
    struct ppsdev *pp = dev->private_data;
    unsigned int tdo;
    spin_lock(&pp->lock);
    if(!pp->enable) {
        spin_unlock(&pp->lock);
        return 0;
    }
    ret |= pp->tms ? pp->cable->tms_data : 0;
    ret |= pp->tck ? pp->cable->tck_data : 0;
    ret |= pp->tdi ? pp->cable->tdi_data : 0;
    ret |= pp->tdo ? pp->cable->tdo_data : 0;
    tdo = pp->tdo;
    spin_unlock(&pp->lock);
 //   if(pp->cable->tdo_data)
 //       printk(KERN_INFO "ppsim dread(%02x) TDO=%d\n", (unsigned)ret, tdo);
    return ret;
}

static unsigned char ppsim_read_control(struct parport *dev)
{
    unsigned char ret = 0;
    struct ppsdev *pp = dev->private_data;
    spin_lock(&pp->lock);
    if(!pp->enable) {
        spin_unlock(&pp->lock);
        return 0;
    }
    ret |= pp->tms ? pp->cable->tms_ctrl : 0;
    ret |= pp->tck ? pp->cable->tck_ctrl : 0;
    ret |= pp->tdi ? pp->cable->tdi_ctrl : 0;
    spin_unlock(&pp->lock);
    ret ^= 0x0b; // bits are hardware inverted
    return ret;
}

static unsigned char ppsim_read_status(struct parport *dev)
{
    unsigned char ret;
    struct ppsdev *pp = dev->private_data;
    unsigned int tdo;
    spin_lock(&pp->lock);
    if(!pp->enable) {
        spin_unlock(&pp->lock);
        return 0;
    }
    ret = pp->tdo ? pp->cable->tdo_sts : 0;
    tdo = pp->tdo;
    spin_unlock(&pp->lock);
    ret ^= 0x80; // BUSY line is hardware inverted
//    if(pp->cable->tdo_sts)
//        printk(KERN_INFO "ppsim sread(%02x) TDO=%d\n", (unsigned)ret, tdo);
    return ret;
}

static unsigned char ppsim_frob_control(struct parport *dev, unsigned char mask,
                               unsigned char val)
{
    unsigned char ret;

    // TODO, not atomic
    ret = ppsim_read_control(dev);
    ret = (ret & ~mask) ^ val;
    ppsim_write_control(dev, ret);
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
    s->u.pc.ctr = ppsim_read_control(dev);
    s->u.pc.ecr = ppsim_read_data(dev);
}
static void ppsim_restorestate(struct parport *dev, struct parport_state *s)
{
    ppsim_write_control(dev, s->u.pc.ctr);
    ppsim_write_data(dev, s->u.pc.ecr);
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

static struct class *simcls;
static struct ppsdev *onedev;

static int __init ppsim_init(void)
{
    dev_t nodev = MKDEV(0,0);
    int err;
    const struct ppcable *ccable = cables;
    struct ppsdev *dev;

    for(; ccable->name; ccable++) {
        if(strcmp(ccable->name, cablename)==0)
            break;
    }

    if(!ccable) {
        printk(KERN_ERR "Cable '%s' is not defined\n", cablename);
        return -EINVAL;
    }
    printk(KERN_INFO "Emulating cable: %s\n", ccable->name);
    printk(KERN_INFO "TMS %02x %02x\n", ccable->tms_data, ccable->tms_ctrl);
    printk(KERN_INFO "TCK %02x %02x\n", ccable->tck_data, ccable->tck_ctrl);
    printk(KERN_INFO "TDI %02x %02x\n", ccable->tdi_data, ccable->tdi_ctrl);
    printk(KERN_INFO "TDO %02x %02x\n", ccable->tdo_data, ccable->tdo_sts);

    simcls = class_create(THIS_MODULE, "sim");
    if(IS_ERR(simcls))
        return PTR_ERR(simcls);

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if(!dev) {
        err = -ENOMEM;
        goto fail_alloc;
    }

    dev->port = parport_register_port(0, PARPORT_IRQ_NONE,
                                 PARPORT_DMA_NONE,
                                 &simops);
    if(!dev->port) {
        err = -EIO;
        goto fail_reg;
    }

    spin_lock_init(&dev->lock);
    printk(KERN_INFO "Initialize simulated parallel port\n");
    dev->cable = ccable;
    dev->port->modes = PARPORT_MODE_PCSPP;
    dev->port->private_data = dev;

    dev->port->dev = device_create(simcls, NULL, nodev, dev, "parsim");
    if(IS_ERR(dev->port->dev)) {
        err = PTR_ERR(dev->port->dev);
        goto fail_dev;
    }

    parport_announce_port(dev->port);
    printk(KERN_INFO "Complete\n");
    /* Protection from whatever random drivers have just tried
     * to print to us...
     */
    dev->enable = 1;
    onedev = dev;
    return 0;
fail_dev:
    parport_remove_port(dev->port);
fail_reg:
    kzfree(dev);
fail_alloc:
    class_destroy(simcls);
    return err;
}
module_init(ppsim_init);

static void __exit ppsim_exit(void)
{
    struct ppsdev *dev = onedev;
    onedev = NULL;
    parport_remove_port(dev->port);
    device_del(dev->port->dev);
    kzfree(dev);
    class_destroy(simcls);
    printk(KERN_INFO "Removed simulated parallel port\n");
}
module_exit(ppsim_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Davidsaver <mdavidsaver@gmail.com");
MODULE_DESCRIPTION("Simulated parallel port JTAG programmer");

static const struct ppcable cables[] =
{
  { // Urjtag minimal
    .name = "Minimal",
    .tms_data = 1 << 1,
    .tck_data = 1 << 2,
    .tdi_data = 1 << 3,
    .tdo_sts  = 1 << 7,
  },
  { // Altera ByteBlaster
    .name = "ByteBlaster",
    .tck_data = 1 << 0,
    .tms_data = 1 << 1,
    .tdi_data = 1 << 6,
    .tdo_sts  = 1 << 7,
  },
  {NULL}
};
