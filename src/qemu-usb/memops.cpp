#include "USBinternal.h"

//Fake it til you make it
bool address_space_rw(AddressSpace *as, hwaddr addr, uint8_t *buf,
                      int len, bool is_write)
{
	//in USB.cpp
	cpu_physical_memory_rw(addr, buf, len, is_write);
	return false;

    //hwaddr l;
    //uint8_t *ptr;
    //uint64_t val;
    //hwaddr addr1;
    //bool error = false;

    ////while (len > 0) {
    //    l = len;

    //    if (is_write) {
    //        /* RAM case */
    //        ptr = (uint8_t*)as + addr;
    //        memcpy(ptr, buf, l);
    //    } else {
    //        /* RAM case */
    //        ptr = (uint8_t*)as + addr;
    //        memcpy(buf, ptr, l);
    //    }
    ////    len -= l;
    ////    buf += l;
    ////    addr += l;
    ////}

    //return error;
}

bool dma_memory_rw(AddressSpace *as, hwaddr addr, uint8_t *buf,
	int len, DMADirection dir)
{
	return address_space_rw(as, addr, buf, len, dir == DMADirection::DMA_DIRECTION_FROM_DEVICE);
}
