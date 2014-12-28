#include <stdlib.h>

#include "util.h"

#include "armemu.h"
#include "armdefs.h"

#include "gdb/gdbstub.h"
#include "gdb/gdbstubchelper.h"

u32 global_gdb_port = 0;
gdbstub_handle_t gdb_stub;

struct armcpu_memory_iface *gdb_memio;
struct armcpu_memory_iface gdb_base_memory_iface;
struct armcpu_ctrl_iface gdb_ctrl_iface;


//Nullstubs
u16 gdb_nullprefetch16(void *data, u32 adr);
u32 gdb_nullprefetch32(void *data, u32 adr);
u8 gdb_nullread8(void *data, u32 adr);
u16 gdb_nullread16(void *data, u32 adr);
u32 gdb_nullread32(void *data, u32 adr);
void gdb_nullwrite8(void *data, u32 adr, u8 val);
void gdb_nullwrite16(void *data, u32 adr, u16 val);
void gdb_nullwrite32(void *data, u32 adr, u32 val);

u16 gdb_nullprefetch16(void *data, u32 adr){ return 0; }
u32 gdb_nullprefetch32(void *data, u32 adr){ return 0; }
u8 gdb_nullread8(void *data, u32 adr){ return 0; }
u16 gdb_nullread16(void *data, u32 adr){ return 0; }
u32 gdb_nullread32(void *data, u32 adr){ return 0; }
void gdb_nullwrite8(void *data, u32 adr, u8 val){}
void gdb_nullwrite16(void *data, u32 adr, u16 val){}
void gdb_nullwrite32(void *data, u32 adr, u32 val){}


void gdbstub_Init(u32 port){
    if (port < 1 || port > 65535) {

        gdb_memio = malloc(sizeof(struct armcpu_memory_iface));
        gdb_memio->prefetch16 = gdb_prefetch16;
        gdb_memio->prefetch32 = gdb_prefetch32;
        gdb_memio->read32 = gdb_read32;
        gdb_memio->write32 = gdb_write32;
        gdb_memio->read16 = gdb_read16;
        gdb_memio->write16 = gdb_write16;
        gdb_memio->read8 = gdb_read8;
        gdb_memio->write8 = gdb_write8;
        DEBUG("Invalid GDB port.\n");
        if (port)
            exit(-1);
    }

    global_gdb_port = port;
    
    gdb_ctrl_iface.stall = stall_cpu;
    gdb_ctrl_iface.unstall = unstall_cpu;
    gdb_ctrl_iface.read_reg = read_cpu_reg;
    gdb_ctrl_iface.set_reg = set_cpu_reg;
    gdb_ctrl_iface.install_post_ex_fn = install_post_exec_fn;
    gdb_ctrl_iface.remove_post_ex_fn = remove_post_exec_fn;

    gdb_base_memory_iface.prefetch16 = gdb_prefetch16;
    gdb_base_memory_iface.prefetch32 = gdb_prefetch32;
    gdb_base_memory_iface.read32 = gdb_read32;
    gdb_base_memory_iface.write32 = gdb_write32;
    gdb_base_memory_iface.read16 = gdb_read16;
    gdb_base_memory_iface.write16 = gdb_write16;
    gdb_base_memory_iface.read8 = gdb_read8;
    gdb_base_memory_iface.write8 = gdb_write8;

    if (global_gdb_port) {
        gdb_stub = createStub_gdb(global_gdb_port,
                                  &gdb_memio,
                                  &gdb_base_memory_iface);
        if (gdb_stub == NULL) {
            DEBUG("Failed to create gdbstub on port %d\n",
                  global_gdb_port);
            exit(-1);
        }
        activateStub_gdb(gdb_stub, &gdb_ctrl_iface);
    } else {
        gdb_memio = malloc(sizeof(struct armcpu_memory_iface));
        gdb_memio->prefetch16 = gdb_prefetch16;
        gdb_memio->prefetch32 = gdb_prefetch32;
        gdb_memio->read32 = gdb_read32;
        gdb_memio->write32 = gdb_write32;
        gdb_memio->read16 = gdb_read16;
        gdb_memio->write16 = gdb_write16;
        gdb_memio->read8 = gdb_read8;
        gdb_memio->write8 = gdb_write8;
    }
}