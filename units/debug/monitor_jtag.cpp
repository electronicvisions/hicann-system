

//#include <stdarg.h>
//#include <unistd.h>
//#include <string>
//#include <vector>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netdb.h>

#include <stdio.h>
#include <unistd.h>

#include "jtag_lib.h"
#include "jtag_cmdbase_fpga.h"


typedef jtag_cmdbase_fpga<jtag_lib::jtag_hub> jtag_t;


int main(int argc, char** argv)
{

	const unsigned int addr_count = 4;

    jtag_t jtag;
    jtag.open(jtag_lib::JTAG_USB, 0);
      
    if ( !jtag.jtag_init() )
        return -1;

	jtag.initJtag();
	jtag.setJtagChain(0,0,0,0);
	if ( !jtag.setFpgaChain(2) )
	{
		printf("error: Invalid FPGA user chain.\n");
		return -1;
	}

	for (;;)
    {
    	unsigned int gp_debug[addr_count];
    	for (unsigned int naddr=0; naddr<addr_count; ++naddr)
        {
			gp_debug[naddr] = 0;
			jtag.SetDebugAddress(naddr);
			jtag.GetDebugOutput(gp_debug[naddr]);
			printf("%08X ", gp_debug[naddr]);
		}
        printf("\n");

        unsigned int flags = gp_debug[0]&0xf;
		printf(" -> HIC-ARQ status: 0x%04X, config rx: 0x%X tx: 0x%X, HostARQ: rnext %d rvalid %d wnext %d wvalid %d\n",
               (gp_debug[0]>>16), ((gp_debug[0]>>12)&0xf), ((gp_debug[0]>>8)&0xf), (flags>>6)&1, (flags>>5)&1, (flags>>4)&1, (flags>>3)&1 );
		printf(" -> al_read_next_count: %d, HICANN ARQ: wcounter=%d, rcounter=%d\n", gp_debug[1], gp_debug[2]&0x7fffffff, gp_debug[3]&0x7fffffff);
        usleep(500000);
	}

    return 0;
}
