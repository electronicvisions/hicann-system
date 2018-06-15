///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate: 2008-10-28 17:16:39 +0100 (Tue, 28 Oct 2008) $
// $LastChangedRevision: 29 $
// $LastChangedBy: henker $

#ifndef __JTAG_EMULATOR__
#define __JTAG_EMULATOR__

#include "systemc.h"
#include "jtag_cmdbase.h"
#include <vector>


	
#ifndef JTAG_CLK_PERIOD
#define JTAG_CLK_PERIOD	111111     // jtag clock speed
#endif


//////////////////////////////////////////////////////////////////////////////////
/// .
class jtag_emulator : public sc_module, public jtag_cmdbase//, public jtag_base<>
{
public:
	//////////////////////////////////////////////////////////////////////////////
	/// .
	sc_in< sc_lv<1> > start_jtag_h;
	sc_in< sc_lv<1> > tdo;
	sc_out< sc_lv<1> > tck;
	sc_out< sc_lv<1> > tdi;
	sc_out< sc_lv<1> > tms;
	
	sc_clock clk;
	sc_event event_start, event_write, event_read;
	
	bool do_wait;
	bool do_write;
	sc_lv<1> tck_value;
	sc_lv<1> tdi_value;
	sc_lv<1> tms_value;
	sc_lv<1> tdo_value;
	
	bool do_read;
	
	//////////////////////////////////////////////////////////////////////////////
	/// .
	SC_HAS_PROCESS(jtag_emulator);

	jtag_emulator (sc_module_name name_, bool fpga_avail, bool dnc_avail, uint hicann_num, uint hicann_addr) :
		jtag_cmdbase(fpga_avail, dnc_avail, hicann_num, hicann_addr, false), // false -> no kintex
		sc_module(name_),
		start_jtag_h("start_jtag_h"),
		tdo("tdo"),
		tck("tck"),
		tdi("tdi"),
		tms("tms"),
		clk("clk", JTAG_CLK_PERIOD, SC_PS)
	{
		SC_METHOD(OnClock_p);
		dont_initialize();
		sensitive_pos << clk;

		SC_METHOD(OnStart);
		dont_initialize();
		sensitive << start_jtag_h;
		SC_THREAD(control_loop);
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .UNUSED BASE-CLASS FUNCTIONS:
	virtual void set_jtag_hicanns_data_chain(uint64_t wdata) {
		std::cout << "jtag_emulator::set_jtag_hicanns_data_chain not implemented!" << std::endl;
	}
	
	virtual void get_jtag_all_data_chain(std::vector<uint64_t>& rdata, unsigned char width) {
		std::cout << "jtag_emulator::get_jtag_all_data_chain not implemented!" << std::endl;
	}
	
	virtual void set_hicanns_ir_chain (unsigned char cmd) {
		std::cout << "jtag_emulator::set_hicanns_ir_chain not implemented!" << std::endl;
	}
	

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void OnStart()
	{
		if( do_wait ) {
			event_start.notify();
		}
	}
	//////////////////////////////////////////////////////////////////////////////
	/// .
	void OnClock_p()
	{
		if( do_write )
		{
			tck.write( tck_value );
			tdi.write( tdi_value );
			tms.write( tms_value );
			do_write=false;
			event_write.notify();
		}

		tdo_value = (tdo.read());
		if( do_read )
		{
			do_read = false;
			event_read.notify();
		}
	}
	//////////////////////////////////////////////////////////////////////////////
	/// .
	void control_loop()
	{
			wait( 1,SC_PS );
			do_wait = true;
			reset_jtag();
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual bool jtag_init(void)	{ return true; }
	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual bool jtag_valid(void)	{ return true; }
	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual bool jtag_close(void)	{ return true; }
	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual void jtag_write(unsigned int tdi, unsigned int tms)
	{
		tdi_value = (tdi!=0);
		tms_value = (tms!=0);
		tck_value = 0;
		do_write=true;
		wait( event_write );
		tck_value = 1;
		do_write=true;
		wait( event_write );
		
		tck_value = 0;
		do_write=true;
		wait( event_write );
	}
	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual void jtag_write(unsigned int tdi, unsigned int tms, size_t cnt)
	{
		tdi_value = (tdi!=0);
		tms_value = (tms!=0);
		tck_value = 0;
		do_write=true;
		wait( event_write );

		for(; cnt; --cnt)
		{
			tck_value = 1;
			do_write=true;
			wait( event_write );
			
			tck_value = 0;
			do_write=true;
			wait( event_write );
		}
	}
	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual unsigned int jtag_read(void)
	{
		do_read=true;
		wait( event_read );
		return tdo_value.to_uint();
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool InitJTAG(void)
	{
		std::cout << "InitJTAG called, will use: chain_length=" << chain_length << ", num_hicanns=" << num_hicanns << ", irw_total=" << irw_total << std::endl;
		std::cout << "InitJTAG: resetting JTAG..." << std::endl;
		reset_jtag();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Reset of Jtag with ten clock TMS=1
	bool reset_jtag(void)
	{
		this->jtag_write(0,1,10);
		this->jtag_write(0,0);
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

			for(k=0;k<chain_length;++k) {
				this->actual_command[k] = 0xffffffff;
			}
			this->actual_command[position] = command;
//		}
	}


 	//////////////////////////////////////////////////////////////////////////////
	/// Additional two clock cycles for INSTRUCTION only operation (sync of update sig)
	void execute_instr(void)
	{
		// Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_write(0,0);
		// Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_write(0,0);
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
	}

	void set_jtag_data_chain_noexec(uint64_t wdata, unsigned char width, unsigned char position) {
		set_jtag_data_chain(wdata,width,position);
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_instr_chain_noexec (unsigned int command, unsigned char position)
	{
		set_jtag_instr_chain(command,position);
	}



	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_data(const std::vector<bool> vec)
	{
		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		std::vector<bool>::const_iterator it = vec.begin();
		std::vector<bool>::const_iterator ed = vec.end();
		if( it!=ed )
		{
			--ed;
			for(++it; it!=ed; ++it)
			{
				this->jtag_write(*it,0);
			}
			// Go to EXIT1-DR
			this->jtag_write(*it,1);
		}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);
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
				if ((i >= position) && ( i<tz+position ))
					{
						const int val = this->jtag_read();
						rdata.push_back( val );
					}
				this->jtag_write(*it, (sz>1)?0:1 );
			  
				//			printf("(%i,%i)", *it?1:0, val); 
				//			printBoolVect(rdata); printf("\n");
			}
		//		printf("\n");
		
		/*	if (position) {
			const int val = this->jtag_read();
			rdata.push_back( val );
			}*/

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);

	}

	void set_get_jtag_data_chain(uint64_t wdata, uint64_t& rdata, unsigned int count, unsigned char position)
	{
		unsigned int	i;
		uint64_t bit, getdata;

		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		wdata <<= position;
		count += chain_length-1;

		for(getdata=0,i=0; i < count; i++)
			{
				if (i >= position)
					{
						bit = this->jtag_read();
						getdata |= bit << (i-position);
					}
			  
				this->jtag_write( (unsigned int)(wdata&1), (i==(count-1)) );
				wdata >>=1;
		
			}
	
		/*	  if (position)
			  {
			  bit = this->jtag_read();
			  getdata |= bit << (i-position);
			  }*/
  
		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);

		rdata = getdata;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_get_jtag_data_chainb(uint64_t wdata, uint64_t& rdata, unsigned int count, unsigned char position)
	{
		set_get_jtag_data_chain(wdata,rdata,count,position);
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_get_jtag_data_chain(const std::vector<bool>& wdata, 
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
				if ((i >= position) && ( i<tz+position ))
					{
						const int val = this->jtag_read();
						rdata.push_back( val );
					}
				this->jtag_write(*it, (sz>1)?0:1 );
			  
				//			printf("(%i,%i)", *it?1:0, val); 
				//			printBoolVect(rdata); printf("\n");
			}
		//		printf("\n");
		
		/*	if (position) {
			const int val = this->jtag_read();
			rdata.push_back( val );
			}*/

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);

	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void get_jtag_data_chain(uint64_t& rdata, unsigned char width, unsigned char position)
	{
		unsigned char i;
		uint64_t bit=0, getdata;
		// Go to SELECT-DR-SCAN
		this->jtag_write(0,1);

		// Go to CAPTURE-DR
		this->jtag_write(0,0);

		// Go to SHIFT-DR
		this->jtag_write(0,0);

		width += position;

		for(getdata=0, i=0; i < width; i++)
			{
				bit = this->jtag_read();
				if(i>=position)
					getdata += bit << (i-position);

				this->jtag_write( 0, i==width-1 );
			}

		// Go to UPDATE-DR - no effect
		this->jtag_write(0,1);

		// Go to RUN-TEST/IDLE
		this->jtag_write(0,0);

		rdata = getdata;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void get_jtag_data_chainb(uint64_t& rdata, unsigned char width, unsigned char position)
	{
		get_jtag_data_chain(rdata,width,position);
	}


	////////////////////////////////////////////////////////////////////////////
	/// Special JTAG function to set all JTAG_IR of HICANNs with same value
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
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_hicann_pos(uint hicann_addr)
	{
		pos_hicann = hicann_addr;
		return;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////
	/// Help function for bool std::vector print out
	void printBoolVect(std::vector<bool> v)
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

};

#endif//__JTAG_EMULATOR__
