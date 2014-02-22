
enum jtags {
  jtag_reset,
  jtag_idle,     // 1
  jtag_dr_scan,  // 2
  jtag_dr_cap,
  jtag_dr_shift,
  jtag_dr_exit1,
  jtag_dr_pause,
  jtag_dr_exit2,
  jtag_dr_update, // 8
  jtag_ir_scan,   // 9
  jtag_ir_cap,
  jtag_ir_shift,
  jtag_ir_exit1,
  jtag_ir_pause,
  jtag_ir_exit2,
  jtag_ir_update,// f
};

static
void jtag_sim(struct ppsdev *port)
{
    int trace = 0;
    static enum jtags state = jtag_reset;
    enum jtags prev_state = state;
    static int prev_tck = 0;

    static u64 hist_tms = 0, hist_tdo = 0, hist_tdi = 0;

    static const u32 myidcode = 0x89BEEF01;

    static u16 IR = 0xffff;
    static u8  DR_len = 1;
    static u32 DR_IDCODE;
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
        if(port->tdi) *DR |= (1<<(DR_len-1));
        if(port->tms==1) state=jtag_dr_exit1;
        break;

    case jtag_dr_exit1: if(port->tms==0) state=jtag_dr_pause; else state=jtag_dr_update; break;

    case jtag_dr_pause: if(port->tms==1) state=jtag_dr_exit2; break;

    case jtag_dr_exit2: if(port->tms==0) state=jtag_dr_shift; else state=jtag_dr_update; break;

    case jtag_dr_update:
        if(port->tms==0) state=jtag_idle; else state=jtag_dr_scan;
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
        if(port->tms==0) state=jtag_idle; else state=jtag_dr_scan;
        break;
    }

    trace = prev_state != state;

    hist_tms<<=1; hist_tdo<<=1; hist_tdi<<=1;
    if(port->tms) hist_tms |= 1;
    if(port->tdi) hist_tdi |= 1;
    if(port->tdo) hist_tdo |= 1;
    if(trace)
        printk(KERN_INFO "%2x -> %2x IR %04x DR %08x TMS %016llx TDI %016llx TDO %016llx\n",
               (int)prev_state, (int)state, IR, *DR,
               (unsigned long long) hist_tms,
               (unsigned long long) hist_tdi,
               (unsigned long long) hist_tdo);
}
