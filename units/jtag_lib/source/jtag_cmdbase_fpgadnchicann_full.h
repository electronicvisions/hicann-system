///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate$
// $LastChangedRevision$
// $LastChangedBy$

#ifndef __JTAG_CMDBASE_FPGADNCHICANN_FULL__
#define __JTAG_CMDBASE_FPGADNCHICANN_FULL__


#include <vector>
#include "logger.h"
#include "jtag_lib.h"
#include "jtag_cmdbase.h"
//#include "jtag_basics.h"
extern "C" {
#include <stdint.h>
}


//////////////////////////////////////////////////////////////////////////////////
/// .
template<typename JTAG>
class jtag_cmdbase_fpgadnchicann_full : public jtag_cmdbase, public JTAG
{
protected:
	// "new" logger class
	Logger& log;

public:
	// shortcuts
	typedef uint32_t uint;
	typedef uint64_t uint64;

	//////////////////////////////////////////////////////////////////////////////
	/// .
	// supply info wether FPGA/DNC is present, bitset of physical availabe HICANNs (HS Channel numbering),
	// the HICANN in chain to talk with and if Kintex7 is used
	jtag_cmdbase_fpgadnchicann_full(bool fpga_avail, bool dnc_avail, std::bitset<8> available_hicanns, uint hicann_addr, bool k7fpga = false):
			jtag_cmdbase(fpga_avail, dnc_avail, available_hicanns, hicann_addr, k7fpga),
			log(Logger::instance("hicann-system.jtag_cmdbase_fpgadnchicann_full", Logger::INFO,""))
	{}

	// depricated ctor with number of hicanns in jtagchain instead ob bitset
	jtag_cmdbase_fpgadnchicann_full(bool fpga_avail, bool dnc_avail, uint hicann_nr, uint hicann_addr, bool k7fpga = false):
			jtag_cmdbase(fpga_avail, dnc_avail, hicann_nr, hicann_addr, k7fpga),
			log(Logger::instance("hicann-system.jtag_cmdbase_fpgadnchicann_full", Logger::INFO,""))
	{}

	jtag_cmdbase_fpgadnchicann_full():
			jtag_cmdbase(false, false, 1, 0, false), // default: no FPGA, no DNC, 1 HICANN at addr 0, not on kintex
			log(Logger::instance("hicann-system.jtag_cmdbase_fpgadnchicann_full", Logger::INFO,""))
   {}

	~jtag_cmdbase_fpgadnchicann_full()
	{}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_hicann_pos(uint hicann_addr)
	{
		pos_hicann = hicann_addr;
		return;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	uint get_hicann_pos() const {
		return pos_hicann;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool reset_jtag(void)
	{
		//this->logging("ResetJTAG\n");
		this->jtag_write(0,1,10);
		this->jtag_execute();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool InitJTAG(void)
	{
		log << "IRW set to " << irw_total;

		reset_jtag();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_instr_chain (unsigned int command, unsigned char position)
	{
//		if( this->actual_command[position] != command ) {
			unsigned char i,k=0;

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			this->jtag_write(0,1);

			// Go to SELECT-IR-SCAN
			this->jtag_write(0,1);

			// Go to CAPTURE-IR
			this->jtag_write(0,0);

			// Go to SHIFT-IR
			this->jtag_write(0,0);

			// SHIFT-IR through chain
			for (uint j = 0;j<chain_length;++j) {
				for(i=0; i<get_irw(j); i++) {
					++k;
					if (position == j) {
						this->jtag_write( command&1, k==irw_total);
						command >>=1;
					} else {
						this->jtag_write( 1, k==irw_total );
					}
				}
			}

			// Go to UPDATE-IR load IR with shift reg. value
			this->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);
			this->jtag_execute();

			for(k=0;k<chain_length;++k) {
				this->actual_command[k] = 0xffffffff;
			}
			this->actual_command[position] = command;
//		}
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_instr_chain_noexec (unsigned int command, unsigned char position)
	{
//		if( this->actual_command[position] != command ) {
			unsigned char i,k=0;

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			this->jtag_write(0,1);

			// Go to SELECT-IR-SCAN
			this->jtag_write(0,1);

			// Go to CAPTURE-IR
			this->jtag_write(0,0);

			// Go to SHIFT-IR
			this->jtag_write(0,0);

			// SHIFT-IR through chain
			for (uint j = 0;j<chain_length;++j) {
				for(i=0; i<get_irw(j); i++) {
					++k;
					if (position == j) {
						this->jtag_write( command&1, k==irw_total);
						command >>=1;
					} else {
						this->jtag_write( 1, k==irw_total );
					}
				}
			}

			// Go to UPDATE-IR load IR with shift reg. value
			this->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);

			for(k=0;k<chain_length;++k) {
				this->actual_command[k] = 0xffffffff;
			}
			assert(position < chain_length);
			this->actual_command[position] = command;
//		}
	}

 	//////////////////////////////////////////////////////////////////////////////
	/// .
	void execute_instr(void)
	{
		// Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_write(0,0);
		// Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_write(0,0);

		this->jtag_execute();
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_data_chain(uint64_t wdata, unsigned char width, unsigned char position)
	{
		unsigned char i;

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		width += (chain_length-1-position);

		for(i=0; i < width; i++)
			{
				this->jtag_write( (unsigned int) (wdata&1), i==width-1 );
				wdata >>=1;
			}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);
		this->jtag_execute();
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_data_chain_noexec(uint64_t wdata, unsigned char width, unsigned char position)
	{
		unsigned char i;

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		width += (chain_length-1-position);

		for(i=0; i < width; i++)
			{
				this->jtag_write( (unsigned int) (wdata&1), i==width-1 );
				wdata >>=1;
			}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);
	}


	//////////////////////////////////////////////////////////////////////////////
	/// Shift through all jtag_dr registers to access all HICANNs in parallel
	void set_jtag_hicanns_data_chain(uint64_t wdata)
	{
		unsigned short i;
		unsigned short width = 0;
		uint64_t shift_data = 0;
		unsigned short num_dnc    = ((chain_length-num_hicanns)==2)?1:0;
		unsigned short num_fpga   = ((chain_length-num_hicanns)!=0)?1:0;
		unsigned short num_hicann = num_hicanns;

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		width = num_hicann*64 + num_fpga + num_dnc;
		//cout << "Shift width: " << width << " Chain: " << chain_length << " hicann_num: " << num_hicann << " fpga_avail: " << num_fpga << " dnc_avail: " << num_dnc << endl;

		for(i=0; i < width; i++)
			{
				if (i%64 == 0)
					shift_data = wdata;
				this->jtag_write( (unsigned int) (shift_data&1), i==width-1 );
				shift_data >>=1;
			}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);
		this->jtag_execute();
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void get_jtag_data_chain(uint64_t& rdata, unsigned char width, unsigned char position)
	{
		unsigned char i = 0;
		unsigned char shift_width = 0;

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		shift_width = width + position;

		for(i=0; i < shift_width; i++)
			{
				if(i>=position && i<shift_width) {
					this->jtag_write( 0, i==shift_width-1, jtag_lib::TDO_READ );
				} else {
					this->jtag_write( 0, i==shift_width-1 );
				}
			}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);

		// read bulk tdo buffer
		jtag_lib::uint64 temp;
		this->jtag_read(temp, width);
		rdata = temp;
	}


	void get_jtag_all_data_chain(std::vector<uint64_t>& rdata, unsigned char width)
	{
		unsigned int i = 0;
		unsigned int shift_width = 0;

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		shift_width = chain_length * width;

		for(i=0; i < shift_width; i++)
			{
				this->jtag_write( 0, i==shift_width-1, jtag_lib::TDO_READ );
			}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);

		// read bulk tdo buffer
        for (unsigned int ndev=0;ndev<chain_length;++ndev){
			jtag_lib::uint64 temp;
			this->jtag_read(temp, width);
            rdata.push_back(temp);
        }

	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_get_jtag_data_chain(uint64_t wdata, uint64_t& rdata, unsigned int count, unsigned char position)
	{
		unsigned int	i;
		unsigned char shift_width = 0;

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		wdata <<= position;

		shift_width = count + chain_length-1;

		for(i=0; i < shift_width; i++)
			{
				if (i >= position && i<(count+position)) {

					this->jtag_write( (unsigned int)(wdata&1), (i==(unsigned int)(shift_width-1)),jtag_lib::TDO_READ );

				} else {

					this->jtag_write( (unsigned int)(wdata&1), (i==(unsigned int)(shift_width-1)) );
				}
				wdata >>=1;
			}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);


		// read bulk tdo buffer
		jtag_lib::uint64 temp;
		this->jtag_read(temp, count);
		rdata = temp;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_get_jtag_data_chain(const std::vector<bool> wdata,
								 std::vector<bool>& rdata,
								 unsigned char position)
	{
		std::vector<bool>::size_type count   = wdata.size();
		std::vector<bool>::size_type tz      = count;
		std::vector<bool>::size_type sz      = count + chain_length-1;

		std::vector<bool> temp(sz,false);

		rdata.clear();
		rdata.reserve(tz);
		//printf("in JTAG(%i) = ", (unsigned int)sz);

		this->jtag_execute();

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		for (int j=0;j<(int)tz;++j){
			temp[j+position] = wdata[j];
		}

		std::vector<bool>::const_iterator it = temp.begin();

		for(unsigned int i=0; sz; ++it, --sz, ++i)
			{
				/*if ((i >= position) && ( i<tz+position ))
					{
						const int val = this->jtag_get_tdo();
						rdata.push_back( val );
					}*/
				this->jtag_write(*it, (sz>1)?0:1,((i >= position) && ( i<tz+position ))?jtag_lib::TDO_READ:jtag_lib::TDO_DEFAULT );

				//			printf("(%i,%i)", *it?1:0, val);
				//			printBoolVect(rdata); printf("\n");
			}
		//		printf("\n");

		/*	if (position) {
			const int val = this->jtag_get_tdo();
			rdata.push_back( val );
			}*/

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);

		// execute and get the bits from the buffer
		this->jtag_read(rdata, (unsigned int)count);
	}



	////////////////////////////////////////////////////////////////////////////
	/// Special JTAG function to set all JTAG_IR of HICANNs with same value
	void set_hicanns_ir_chain (unsigned char cmd)
	{
			unsigned char i,k=0;
            unsigned char command = cmd;

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			this->jtag_write(0,1);

			// Go to SELECT-IR-SCAN
			this->jtag_write(0,1);

			// Go to CAPTURE-IR
			this->jtag_write(0,0);

			// Go to SHIFT-IR
			this->jtag_write(0,0);

			// SHIFT-IR through chain
			for (uint j = 0;j<chain_length;++j) {
              command = cmd;
				for(i=0; i<get_irw(j); i++) {
					++k;
					if ( (pos_fpga != j) && (pos_dnc != j) ) {
						this->jtag_write( command&1, k==irw_total);
						command >>=1;
					} else {
						this->jtag_write( 1, k==irw_total );
					}
				}
			}

			// Go to UPDATE-IR load IR with shift reg. value
			this->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);
			this->jtag_execute();
	}


	void set_all_ir_chain (unsigned char cmd)
	{
			unsigned char i,k=0;
            unsigned char command = cmd;

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			this->jtag_write(0,1);

			// Go to SELECT-IR-SCAN
			this->jtag_write(0,1);

			// Go to CAPTURE-IR
			this->jtag_write(0,0);

			// Go to SHIFT-IR
			this->jtag_write(0,0);

			// SHIFT-IR through chain
			for (uint j = 0;j<chain_length;++j) {
              command = cmd;
				for(i=0; i<get_irw(j); i++) {
					++k;
					this->jtag_write( command&1, k==irw_total);
					command >>=1;
				}
			}

			// Go to UPDATE-IR load IR with shift reg. value
			this->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_write(0,0);
			this->jtag_execute();
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void printBoolVect(const std::vector<bool>& v)
	{
		unsigned int hexval=0;
		for (unsigned int nbit=v.size();nbit>0;)
			{
				--nbit;
				if (v[nbit])
					hexval += (1<<(nbit%4));

				if ((nbit%4)==0)
					{
						printf("%X",hexval);
						hexval = 0;
					}
			}


	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void printDelayVect(const std::vector<bool>& v)
	{
		unsigned int hexval=0;
		for (unsigned int nbit=v.size();nbit>0;)
			{
				--nbit;
				if (v[nbit])
					hexval += (1<<(nbit%6));

				if ((nbit%6)==0)
					{
						printf("\nChannel %i = %i",nbit/6,hexval);
						hexval = 0;
					}
			}


	}

	//////////////////////////////////////////////////////////////////////////////
	/// Automatic control sequence from file.
	bool read_file(const char* name)
	{
		FILE* file = fopen(name, "rb");
		if( file )
			{
				log << "not yet implemented";
			}
		else
			{
				log << "could not open file '" << name << "'";
			}
		return true;
	}


};


#endif//__JTAG_CMDBASE_FPGADNCHICANN_FULL__
