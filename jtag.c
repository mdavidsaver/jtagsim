
enum jtags {
  jtag_reset,
  jtag_idle,
  jtag_dr_scan,
  jtag_dr_cap,
  jtag_dr_shift,
  jtag_dr_exit1,
  jtag_dr_pause,
  jtag_dr_exit2,
  jtag_dr_update,
  jtag_ir_scan,
  jtag_ir_cap,
  jtag_ir_shift,
  jtag_ir_exit1,
  jtag_ir_pause,
  jtag_ir_exit2,
  jtag_ir_update,
};

static
void jtag_sim(struct ppsdev *port)
{
    static enum jtags state = jtag_reset;
    static int prev_tck = 0;

    static const u32 myidcode = 0x89BEEF01;

    static u16 IR = 0xffff;
    static u8  DR_len = 1;
    static u32 DR_IDCODE = myidcode;
    static u32 DR_BYPASS = 0; // only 1 bit used
    static u32 DR_BSR = 0; // EXTEST and SAMPLE
    static u32 DR_UNKNOWN = 0;
    static u32 *DR = &DR_BYPASS;

    if(port->tck==0 || prev_tck==port->tck) {
        // not a rising edge
        prev_tck = port->tck;
        return;
    }
    prev_tck = port->tck;

    switch(state) {
    case jtag_reset:
        if(port->tms==0) {
            state=jtag_idle;
            IR = 4; // pre-load IDCODE
            DR = &DR_IDCODE;
            *DR = myidcode;
            DR_len = 32;
        }
        break;

    case jtag_idle: if(port->tms==1) state=jtag_dr_scan; break;

    case jtag_dr_scan: if(port->tms==0) state=jtag_dr_cap; else state=jtag_ir_scan; break;

    case jtag_dr_cap:
        // capture current DR
        if(port->tms==0) state=jtag_dr_shift; else state=jtag_dr_exit1;
        break;

    case jtag_dr_shift:
        port->tdo = *DR&1;
        *DR >>= 1;
        if(port->tdi) *DR |= (1<<DR_len);
        if(port->tms==1) state=jtag_dr_exit1;
        break;

    case jtag_dr_exit1: if(port->tms==0) state=jtag_dr_pause; else state=jtag_dr_update; break;

    case jtag_dr_pause: if(port->tms==1) state=jtag_dr_exit2; break;

    case jtag_dr_exit2: if(port->tms==0) state=jtag_dr_shift; else state=jtag_dr_update; break;

    case jtag_dr_update:
        if(port->tms==0) state=jtag_idle; else state=jtag_dr_scan;
        printk(KERN_INFO "DR  %04x\n", *DR);
        break;


    case jtag_ir_scan: if(port->tms==0) state=jtag_ir_cap; else state=jtag_reset; break;

    case jtag_ir_cap:
        // capture current IR
        if(port->tms==0) state=jtag_ir_shift; else state=jtag_ir_exit1;
        break;

    case jtag_ir_shift:
        port->tdo = IR&1;
        IR >>= 1;
        if(port->tdi) IR |= 0x8000;
        if(port->tms==1) state=jtag_ir_exit1;
        break;

    case jtag_ir_exit1: if(port->tms==0) state=jtag_ir_pause; else state=jtag_ir_update; break;

    case jtag_ir_pause: if(port->tms==1) state=jtag_ir_exit2; break;

    case jtag_ir_exit2: if(port->tms==0) state=jtag_ir_shift; else state=jtag_ir_update; break;

    case jtag_ir_update:
        switch(IR) {
        case 0xffff: DR=&DR_BYPASS; *DR = 0; DR_len = 1; break;

        case 4: DR=&DR_IDCODE; *DR = myidcode; DR_len = 32; break;

        case 0: // EXTEST
        case 5: // SAMPLE
            DR=&DR_BSR; *DR=0xf0f0f0f0; DR_len = 32; break;

        default: DR=&DR_UNKNOWN; *DR = 0xdeadbeef; DR_len = 32; break;
        }
        printk(KERN_INFO "IR %04x\n", IR);
        if(port->tms==0) state=jtag_idle; else state=jtag_dr_scan;
        break;
    }
}
