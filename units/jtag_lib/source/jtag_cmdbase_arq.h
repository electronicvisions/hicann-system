///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate$
// $LastChangedRevision$
// $LastChangedBy$

#ifndef __jtag_cmdbase__
#define __jtag_cmdbase__


#include <vector>
#include "logger.h"
#include "jtag_lib.h"
//#include "jtag_basics.h"

// JTAG related defines
#define IRWA_FPGA           6
#define IRWA_DNC            6        
#define IRWA_HICANN         6     

#define CHAINA_LENGTH       1

#define POSA_FPGA           2
#define POSA_DNC            1
#define POSA_HICANN         0

// instruction register width (bit)

#define CMD_READID          	0x00
#define CMD_CHANNEL_SELECT		0x01
#define CMD_LVDS_PADS_EN     	0x02
#define CMD_INIT_CTRL 			0x03
#define CMD_CFG_PROTOCOL		0x04
#define CMD_TIMESTAMP_CTRL		0x05
#define CMD_LOOPBACK_CTRL		0x06
#define CMD_BIAS_BYPASS			0x07
#define CMD_START_LINK   		0x08
#define CMD_STOP_LINK   		0x09
#define CMD_TEST_MODE_EN 	  	0x0a
#define CMD_MEMORY_TEST_MODE	0x0b
#define CMD_TEST_MODE_DIS 		0x0c
#define CMD_START_ROUTE_DAT		0x0d
#define CMD_LVDS_TEST_CTRL		0x0e
#define CMD_TDO_SELECT			0x0f
#define CMD_PLL2G_CTRL	 		0x10
#define CMD_SET_TX_DATA			0x11
#define CMD_GET_RX_DATA			0x12
#define CMD_SET_HEAP_MODE		0x13
#define CMD_SET_CURRENT_CHAIN 	0x14
#define CMD_SET_TIME_LIMIT		0x15
#define CMD_SPEED_DETECT_CTRL	0x16
#define UNUSED28	     		0x17
#define CMD_START_CFG_PKG   	0x18
#define CMD_START_PULSE_PKG 	0x19
#define CMD_READ_STATUS     	0x1a
#define CMD_RESET_CHANNEL 		0x1b
#define UNUSED5	  				0x1c
#define UNUSED32				0x1d
#define CMD_SET_DELAY_RX_DATA	0x21
#define CMD_SET_DELAY_RX_CLK	0x22
#define UNUSED9       			0x23
#define CMD_PULSE_PROTOCOL		0x24
#define UNUSED6					0x25
#define UNUSED29				0x26
#define CMD_READ_CRC_COUNT  	0x27
#define CMD_RESET_CRC_COUNT   	0x28
#define UNUSED31				0x29
#define CMD_READ_HEAP_COUNT		0x2a
#define CMD_RESET_HEAP_COUNT   	0x2b
#define CMD_SET_LOW_SPEED	    0x2c
#define UNUSED26			    0x2d
#define CMD_DC_CODING 	    	0x2e
#define UNUSED10		       	0x2f
#define UNUSED11				0x30
#define UNUSED12				0x31
#define UNUSED13    	    	0x32
#define UNUSED14				0x33
#define UNUSED15				0x34
#define UNUSED16    	     	0x35
#define UNUSED17		       	0x36
#define UNUSED18				0x37
#define UNUSED19		       	0x38
#define UNUSED20		       	0x39
#define UNUSED21		       	0x3a
#define UNUSED22    	    	0x3b
#define UNUSED23		       	0x3c
#define UNUSED24    	    	0x3d
#define UNUSED25	  	     	0x3e
#define CMD_BYPASS           	0x3f


#define CMD2_SET_TX_DATA			0x02
#define CMD2_SET_TX_CHANNEL 		0x03
#define CMD2_SET_TX_CTRL			0x04
#define CMD2_GET_RX_DATA			0x05
#define CMD2_GET_RX_CHANNEL			0x06
#define CMD2_GET_STATUS				0x0a
#define CMD2_SET_FPGA_CTRL			0x0b


// ************ HICANN STATES ******************* //	
#define CMD3_READID          	0x00
#define CMD3_LVDS_PADS_EN     	0x02
#define CMD3_LINK_CTRL 			0x03
#define CMD3_LAYER1_MODE		0x04
#define CMD3_SYSTEM_ENABLE		0x05
#define CMD3_BIAS_CTRL			0x06
#define CMD3_SET_IBIAS 			0x07
#define CMD3_START_LINK   		0x08
#define CMD3_STOP_LINK   		0x09
#define CMD3_STOP_TIME_COUNT	0x0a
#define CMD3_READ_SYSTIME 	 	0x0b
#define CMD3_SET_2XPLS  		0x0d
#define CMD3_PLL2G_CTRL	 		0x10
#define CMD3_SET_TX_DATA		0x11
#define CMD3_GET_RX_DATA		0x12
#define CMD3_SET_TEST_CTRL    	0x17
#define CMD3_START_CFG_PKG   	0x18
#define CMD3_START_PULSE_PKG 	0x19
#define CMD3_READ_STATUS     	0x1a
#define CMD3_SET_RESET	  		0x1b
#define CMD3_REL_RESET	  		0x1c
#define CMD3_SET_DELAY_RX_DATA	0x21
#define CMD3_SET_DELAY_RX_CLK	0x22
#define CMD3_READ_CRC_COUNT    	0x27
#define CMD3_RESET_CRC_COUNT   	0x28

// UHEI states
#define CMD3_PLL_FAR_CTRL		0x29
#define CMD3_ARQ_CTRL          	0x30
#define CMD3_ARQ_CTRL_RESET     0x01
#define CMD3_ARQ_CTRL_LOOPBACK  0x02
#define CMD3_ARQ_TXPCKNUM      	0x31
#define CMD3_ARQ_RXPCKNUM      	0x32
#define CMD3_ARQ_RXDROPNUM     	0x33
#define CMD3_ARQ_TXTOVAL       	0x34
#define CMD3_ARQ_RXTOVAL       	0x35
#define CMD3_ARQ_TXTONUM       	0x36
#define CMD3_ARQ_RXTONUM       	0x37

// ocp address meanings in i2c master
#define ADDR_PRESC_L 0x0
#define ADDR_PRESC_M 0x1
#define ADDR_CTRL    0x2
#define ADDR_RXREG   0x3
#define ADDR_STATUS  0x4
#define ADDR_TXREG   0x5
#define ADDR_CMDREG  0x6

// ************ JTAG to OCP instructions (in FPGA) *************** //
#define JTAG_OCP_DR_WIDTH 14     // data register width for ocp-bus-chain in FPGA
/*** UHEI I2C stuff ***/
#define CMD_OCP_SET_DATA_EXEC 0x20   // set ocp address, data and rw bit and execute transfer
#define CMD_OCP_POLL          0x21   // poll if slave finished
#define CMD_OCP_GET_DATA      0x22   // read back ocp data


//////////////////////////////////////////////////////////////////////////////////
/// .
template<typename JTAG>
class jtag_cmdbase : public JTAG
{
public:

	// "new" logger class
	Logger& log;


	unsigned char const& get_irw(unsigned int pos)
	{
//		static const unsigned char IRWIDTH[CHAINA_LENGTH] = {IRWA_FPGA, IRWA_DNC, IRWA_HICANN};
//		static const unsigned char IRWIDTH[CHAINA_LENGTH] = {IRWA_HICANN, IRWA_FPGA};
		static const unsigned char IRWIDTH[CHAINA_LENGTH] = {IRWA_HICANN};
		return IRWIDTH[pos];
	}

//	static const unsigned char IRWA_TOTAL = IRWA_FPGA+IRWA_DNC+IRWA_HICANN; //sum of all IRW
//	static const unsigned char IRWA_TOTAL = IRWA_FPGA+IRWA_HICANN; //sum of all IRW
	static const unsigned char IRWA_TOTAL = IRWA_HICANN; //sum of all IRW
	unsigned int actual_command[CHAINA_LENGTH];

	//////////////////////////////////////////////////////////////////////////////
	/// .
	jtag_cmdbase():log(Logger::instance(Logger::INFO,"")) {
		for(int i=0;i<CHAINA_LENGTH;++i) {
			actual_command[i]=0xffffffff;
		}
	}

	~jtag_cmdbase()
		{}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool reset_jtag(void)
	{
		//this->logging("ResetJTAG\n");
		this->jtag_bulk_write(0,1,10);
		this->jtag_bulk_execute();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_instr_chain (unsigned int command, unsigned char position)
	{
//		if( this->actual_command[position] != command ) {
			unsigned char i,k=0;

			// Go to RUN-TEST/IDLE
			this->jtag_bulk_write(0,0);
      
			// Go to SELECT-DR-SCAN
			this->jtag_bulk_write(0,1);
      
			// Go to SELECT-IR-SCAN
			this->jtag_bulk_write(0,1);
      
			// Go to CAPTURE-IR
			this->jtag_bulk_write(0,0);
      
			// Go to SHIFT-IR
			this->jtag_bulk_write(0,0);
      
			// SHIFT-IR through chain
			for (int j = 0;j<CHAINA_LENGTH;++j) {
				for(i=0; i<get_irw(j); i++) {
					++k;
					if (position == j) {
						this->jtag_bulk_write( command&1, k==IRWA_TOTAL);
						command >>=1;
					} else {
						this->jtag_bulk_write( 1, k==IRWA_TOTAL );
					}
				}
			}
			  
			// Go to UPDATE-IR load IR with shift reg. value
			this->jtag_bulk_write(0,1);
      
			// Go to RUN-TEST/IDLE
			this->jtag_bulk_write(0,0);
			this->jtag_bulk_execute();

			for(k=0;k<CHAINA_LENGTH;++k) {
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
			this->jtag_bulk_write(0,0);
      
			// Go to SELECT-DR-SCAN
			this->jtag_bulk_write(0,1);
      
			// Go to SELECT-IR-SCAN
			this->jtag_bulk_write(0,1);
      
			// Go to CAPTURE-IR
			this->jtag_bulk_write(0,0);
      
			// Go to SHIFT-IR
			this->jtag_bulk_write(0,0);
      
			// SHIFT-IR through chain
			for (int j = 0;j<CHAINA_LENGTH;++j) {
				for(i=0; i<get_irw(j); i++) {
					++k;
					if (position == j) {
						this->jtag_bulk_write( command&1, k==IRWA_TOTAL);
						command >>=1;
					} else {
						this->jtag_bulk_write( 1, k==IRWA_TOTAL );
					}
				}
			}
			  
			// Go to UPDATE-IR load IR with shift reg. value
			this->jtag_bulk_write(0,1);
      
			// Go to RUN-TEST/IDLE
			this->jtag_bulk_write(0,0);

			for(k=0;k<CHAINA_LENGTH;++k) {
				this->actual_command[k] = 0xffffffff;
			}
			this->actual_command[position] = command;
//		}
	}

 	//////////////////////////////////////////////////////////////////////////////
	/// .
	void execute_instr(void)
	{
		// Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_bulk_write(0,0);
		// Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_bulk_write(0,0);
		
		this->jtag_bulk_execute();
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		void set_jtag_data_chain(T wdata, unsigned char width, unsigned char position)
		{
			unsigned char i;

			// Go to SELECT-DR-SCAN
			this->jtag_bulk_write(0,1);

			// Go to CAPTURE-DR
			this->jtag_bulk_write(0,0);

			// Go to SHIFT-DR
			this->jtag_bulk_write(0,0);

			width += (CHAINA_LENGTH-1-position);

			for(i=0; i < width; i++)
				{
					this->jtag_bulk_write( (unsigned int) (wdata&1), i==width-1 );
					wdata >>=1;
				}

			// Go to UPDATE-DR - no effect
			this->jtag_bulk_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_bulk_write(0,0);
			this->jtag_bulk_execute();
		}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		void set_jtag_data_chain_noexec(T wdata, unsigned char width, unsigned char position)
		{
			unsigned char i;

			// Go to SELECT-DR-SCAN
			this->jtag_bulk_write(0,1);

			// Go to CAPTURE-DR
			this->jtag_bulk_write(0,0);

			// Go to SHIFT-DR
			this->jtag_bulk_write(0,0);

			width += (CHAINA_LENGTH-1-position);

			for(i=0; i < width; i++)
				{
					this->jtag_bulk_write( (unsigned int) (wdata&1), i==width-1 );
					wdata >>=1;
				}

			// Go to UPDATE-DR - no effect
			this->jtag_bulk_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_bulk_write(0,0);
		}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		void get_jtag_data_chain(T& rdata, unsigned char width, unsigned char position)
		{
			unsigned char i;
			uint64 bit=0, getdata;
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

			rdata = (T)getdata;
		}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		void get_jtag_data_chainb(T& rdata, unsigned char width, unsigned char position)
		{
			unsigned char i;
			// Go to SELECT-DR-SCAN
			this->jtag_bulk_write(0,1);

			// Go to CAPTURE-DR
			this->jtag_bulk_write(0,0);

			// Go to SHIFT-DR
			this->jtag_bulk_write(0,0);

			width += position;

			for(i=0; i < width; i++)
				{
					if(i>=position && i<position+width) {
						this->jtag_bulk_write( 0, i==width-1, jtag_lib::TDO_READ );
					} else {
						this->jtag_bulk_write( 0, i==width-1 );
					}
				}

			// Go to UPDATE-DR - no effect
			this->jtag_bulk_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_bulk_write(0,0);

			width -= position;
			uint read=0;
			uint numuints = width/8;
			uint rem = width%8;
			uint numreads = rem ? numuints+1 : numuints;
			rdata = 0;
			for(uint i=0;i<numreads;i++){
				read=0;
				uint bits = (i==numuints) ? rem : 8;
				this->jtag_bulk_read(read, bits);
				T read_t = read;
				rdata |= read_t << (i*8);
				//cout << "get_jtag_data_chainb: read byte 0x" << std::hex << read << std::endl;
			}

			this->jtag_bulk_init();
		}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		void set_get_jtag_data_chain(T wdata, T& rdata, unsigned int count, unsigned char position)
		{
			unsigned int	i;
			T bit, getdata;

			// Go to SELECT-DR-SCAN
			this->jtag_write(0,1);

			// Go to CAPTURE-DR
			this->jtag_write(0,0);

			// Go to SHIFT-DR
			this->jtag_write(0,0);

			wdata <<= position;
			count += CHAINA_LENGTH-1;

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

			rdata = (T)getdata;
		}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		void set_get_jtag_data_chainb(T wdata, T& rdata, unsigned int count, unsigned char position)
		{
			unsigned int	i;

			// Go to SELECT-DR-SCAN
			this->jtag_bulk_write(0,1);

			// Go to CAPTURE-DR
			this->jtag_bulk_write(0,0);

			// Go to SHIFT-DR
			this->jtag_bulk_write(0,0);

			wdata <<= position;
			count += CHAINA_LENGTH-1;

			for(i=0; i < count; i++)
				{
					if (i >= position && i<position+count) {
			
						this->jtag_bulk_write( (unsigned int)(wdata&1), (i==(count-1)), jtag_lib::TDO_READ );
				  
					} else {
				  
						this->jtag_bulk_write( (unsigned int)(wdata&1), (i==(count-1)) );
					}
					wdata >>=1;
				}
	  
			// Go to UPDATE-DR - no effect
			this->jtag_bulk_write(0,1);

			// Go to RUN-TEST/IDLE
			this->jtag_bulk_write(0,0);

			count -= CHAINA_LENGTH-1;
			uint read=0;
			uint numuints = count/8;
			uint rem = count%8;
			uint numreads = rem ? numuints+1 : numuints;
			rdata = 0;
			for(uint by=0;by<numreads;by++){
				read=0;
				uint bits = (by==numuints) ? rem : 8;
				this->jtag_bulk_read(read, bits);
				T read_t = read;
				rdata |= read_t << (by*8);
				//cout << "set_get_jtag_data_chainb: read byte 0x" << std::hex << read << std::endl;
			}

			this->jtag_bulk_init();
		}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_get_jtag_data_chain(const std::vector<bool>& wdata, 
								 std::vector<bool>& rdata, 
								 unsigned char position)
	{
		std::vector<bool>::size_type count   = wdata.size();
		std::vector<bool>::size_type tz      = count;
		std::vector<bool>::size_type sz      = count + CHAINA_LENGTH-1;

		std::vector<bool> temp(sz,false);

		rdata.clear();
		rdata.reserve(tz);
		//printf("in JTAG(%i) = ", (unsigned int)sz);

		this->jtag_bulk_execute();

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
	bool InitJTAG(void)
	{
		this->logging("IRW set to %d\n", IRWA_TOTAL);
		
		reset_jtag();
		
		return true;
	}
	

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_link(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD_START_LINK,POSA_DNC);

		set_jtag_data_chain(wdata,9,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_stop_link(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD_STOP_LINK,POSA_DNC);

		set_jtag_data_chain(wdata,9,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_channel(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_CHANNEL_SELECT,POSA_DNC);

		set_jtag_data_chain (wdata,4,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_tx_data(uint64 wdata)
	{
		set_jtag_instr_chain (CMD_SET_TX_DATA,POSA_DNC);

		set_jtag_data_chain (wdata,64,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool DNC_get_rx_data(T& rdata)
		{
			set_jtag_instr_chain (CMD_GET_RX_DATA,POSA_DNC);

			get_jtag_data_chain(rdata,64,POSA_DNC);

			return true;
		}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_lvds_pads_en(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD_LVDS_PADS_EN,POSA_DNC);

		set_jtag_data_chain (wdata,9,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// enable loopback mode for HICANN channels.
	/// 0    :    disable
	/// 1    :    enable looopback
	bool DNC_set_loopback(unsigned char wdata)
	{
		uint64 channel = (uint64)wdata;
		set_jtag_instr_chain (CMD_LOOPBACK_CTRL,POSA_DNC);

		set_jtag_data_chain(channel,8,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// set heap mode for 8 memories (equals layer1 channel direction) in 8 HICANN channels
	/// 0    :    disable
	/// 1    :    memory is heap
	bool DNC_set_heap_mode(uint64 wdata)
	{
		uint64 channel = (uint64)wdata;
		set_jtag_instr_chain (CMD_SET_HEAP_MODE,POSA_DNC);

		set_jtag_data_chain(channel,64,POSA_DNC);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_write_routing_dat(void)
	{
		set_jtag_instr_chain (CMD_START_ROUTE_DAT,POSA_DNC);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// GHz PLL control std::vector.
	/// bit 0    :    switch PLL: 0:2GHz 1:1GHz
	/// bit 1    :    enable VCO
	/// bit 2    :    enable 1GHz
	/// bit 3    :    enable 500MHz
	bool DNC_set_GHz_pll_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_PLL2G_CTRL,POSA_DNC);

		set_jtag_data_chain(wdata,4,POSA_DNC);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_cfg_pkg(void)
	{
		set_jtag_instr_chain (CMD_START_CFG_PKG,POSA_DNC);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_pulse_pkg(void)
	{
		set_jtag_instr_chain (CMD_START_PULSE_PKG,POSA_DNC);
		execute_instr();
		
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool DNC_read_status(T &rdata)
		{
			set_jtag_instr_chain (CMD_READ_STATUS,POSA_DNC);
			get_jtag_data_chain(rdata,8,POSA_DNC);
			return true;
		}
	
	bool DNC_read_channel_sts (unsigned char channel, unsigned char &status)
	{
		DNC_set_channel(channel);
		DNC_read_status(status);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool DNC_read_crc_count(T &rdata)
		{
			set_jtag_instr_chain (CMD_READ_CRC_COUNT,POSA_DNC);
			get_jtag_data_chain (rdata,8,POSA_DNC);
			return true;
		}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_reset_crc_count(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_RESET_CRC_COUNT,POSA_DNC);
		set_jtag_data_chain(wdata,8,POSA_DNC);
		
		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool DNC_read_heap_count(T &rdata)
		{
			set_jtag_instr_chain (CMD_READ_HEAP_COUNT,POSA_DNC);
			get_jtag_data_chain(rdata,64,POSA_DNC);
			return true;
		}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_reset_heap_count(uint64 wdata)
	{
		set_jtag_instr_chain (CMD_RESET_HEAP_COUNT,POSA_DNC);
		set_jtag_data_chain(wdata,64,POSA_DNC);
		
		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_reset(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_RESET_CHANNEL,POSA_DNC);
		set_jtag_data_chain(wdata,8,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_pulse_protocol(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_PULSE_PROTOCOL,POSA_DNC);
		set_jtag_data_chain (wdata,8,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_bias_bypass(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_BIAS_BYPASS,POSA_DNC);
		set_jtag_data_chain(wdata,1,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_memory_test_mode(unsigned char wdata)
	{
	
		set_jtag_instr_chain (CMD_MEMORY_TEST_MODE,POSA_DNC);
		set_jtag_data_chain(wdata,1,POSA_DNC);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_config_protocol(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_CFG_PROTOCOL,POSA_DNC);
		set_jtag_data_chain(wdata,8,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_time_limit(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_SET_TIME_LIMIT,POSA_DNC);
		set_jtag_data_chain(wdata,10,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Offers possibility to disable use of timestamp in a pulse event.
	/// This allows direct and low latency pulse event transmission.
	/// 1 bit for each HICANN connection.
	bool DNC_set_timestamp_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_TIMESTAMP_CTRL,POSA_DNC);
		set_jtag_data_chain(wdata,8,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_lowspeed_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_SET_LOW_SPEED,POSA_DNC);
		set_jtag_data_chain(wdata,8,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_speed_detect_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_SPEED_DETECT_CTRL,POSA_DNC);
		set_jtag_data_chain(wdata,8,POSA_DNC);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_dc_coding_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD_DC_CODING,POSA_DNC);
		set_jtag_data_chain(wdata,8,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_test_mode_en(void)
	{
		set_jtag_instr_chain (CMD_TEST_MODE_EN,POSA_DNC);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_test_mode_dis(void)
	{
		set_jtag_instr_chain (CMD_TEST_MODE_DIS,POSA_DNC);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	///	bool set_clk_delay(uint64 wdata, uint64 *rdata)
	///  wdata.size() = 54
	///  9 times 6 bit
	///  53--------------------------------------------------0
	///  link0 link1 link2 link3 link4 link5 link6 link7 link8 
	bool DNC_set_clk_delay(const std::vector<bool>& wdata, std::vector<bool>& rdata)
	{
		set_jtag_instr_chain (CMD_SET_DELAY_RX_CLK,POSA_DNC);

		set_get_jtag_data_chain(wdata,rdata,POSA_DNC);
		//set_get_jtag_data(wdata,rdata,5);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	///	bool set_data_delay(uint64 wdata, uint64 *rdata)
	///  wdata.size() = 144
	///  8 times 6 bit + 16 times 6 bit
	///  143--------------------------------------------------0
	///  link0 link1 link2 link3 link4 link5 link6 link7 link8 
	///  FPGA channel 15 -----> channel 0
	bool DNC_set_data_delay(const std::vector<bool>& wdata, std::vector<bool>& rdata)
	{
		set_jtag_instr_chain (CMD_SET_DELAY_RX_DATA,POSA_DNC);

		set_get_jtag_data_chain(wdata,rdata,POSA_DNC);
		//set_get_jtag_data(wdata,rdata,5);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	///	bool set_data_delay(uint64 wdata, uint64 *rdata)
	///  wdata.size() = 135
	///  9 times 15 bit
	///  134--------------------------------------------------0
	///  link0 link1 link2 link3 link4 link5 link6 link7 link8 
	///  current set: 	14--------------0
	///               	TX_PAD RX_PAD DES
	bool DNC_set_ibias(const std::vector<bool> wdata, std::vector<bool>& rdata)
	{
		set_jtag_instr_chain (CMD_SET_CURRENT_CHAIN,POSA_DNC);

		set_get_jtag_data_chain(wdata,rdata,POSA_DNC);
		//set_get_jtag_data(wdata,rdata,135);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool bypass(const std::vector<bool>& wdata, std::vector<bool>& rdata)
	{
		set_jtag_instr_chain (CMD_BYPASS,CHAINA_LENGTH-1);

		set_get_jtag_data_chain(wdata,rdata,CHAINA_LENGTH-1);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_pulse (unsigned char channel, uint64 data)
	{
		DNC_set_channel(channel);
		DNC_set_tx_data(data);
		DNC_start_pulse_pkg();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_config (unsigned char channel, uint64 data)
	{
		DNC_set_channel(channel);
		DNC_set_tx_data(data);
		DNC_start_cfg_pkg();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_routing (unsigned char channel, uint64 data)
	{
		DNC_set_channel(channel);
		DNC_set_tx_data(data);
		DNC_write_routing_dat();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_init_ctrl(unsigned int wdata)
	{
		set_jtag_instr_chain (CMD_INIT_CTRL,POSA_DNC);

		set_jtag_data_chain(wdata,17,POSA_DNC);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_test_lvds_ctrl(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD_LVDS_TEST_CTRL,POSA_DNC);

		set_jtag_data_chain(wdata,9,POSA_DNC);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// Start of Jtag Chain with 2 elements functions.
	template<typename T>
		bool read_id(T& rdata, unsigned char chip_nr)
		{
			set_jtag_instr_chain (CMD_READID, chip_nr);
			get_jtag_data_chainb(rdata,32,chip_nr);

			return true;
		}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_lvds_pads_en(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD3_LVDS_PADS_EN,POSA_HICANN);
		set_jtag_data_chain(wdata,1,POSA_HICANN);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_start_link()
	{
		set_jtag_instr_chain (CMD3_START_LINK,POSA_HICANN);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_stop_link()
	{
		set_jtag_instr_chain (CMD3_STOP_LINK,POSA_HICANN);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_set_pls64(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD3_SET_2XPLS, POSA_HICANN);
		set_jtag_data_chain (wdata,1,POSA_HICANN);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_start_cfg_pkg()
	{
		set_jtag_instr_chain (CMD3_START_CFG_PKG,POSA_HICANN);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_start_pulse_pkg()
	{
		set_jtag_instr_chain (CMD3_START_PULSE_PKG,POSA_HICANN);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool HICANN_read_status(T& rdata)
		{
			set_jtag_instr_chain (CMD3_READ_STATUS,POSA_HICANN);
			get_jtag_data_chain(rdata,8,POSA_HICANN);
		
			return true;
		}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool HICANN_read_crc_count (T& count)
		{
			set_jtag_instr_chain (CMD3_READ_CRC_COUNT,POSA_HICANN);
			get_jtag_data_chain(count,8,POSA_HICANN);
			
			return true;
		}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_reset_crc_count(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD3_RESET_CRC_COUNT,POSA_HICANN);
		
		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_set_tx_data(uint64 wdata)
	{
		set_jtag_instr_chain_noexec (CMD3_SET_TX_DATA,POSA_HICANN);
		set_jtag_data_chain_noexec (wdata,64,POSA_HICANN);
		this->jtag_bulk_execute();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_get_rx_data(uint64& rdata)
	{
		set_jtag_instr_chain_noexec (CMD3_GET_RX_DATA, POSA_HICANN);
		get_jtag_data_chainb (rdata,64,POSA_HICANN);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Readout of systime counter.(ATTENTION: Very slow)
	template<typename T>
	bool HICANN_read_systime(T &rdata)
	{
		set_jtag_instr_chain (CMD3_READ_SYSTIME, POSA_HICANN);
		get_jtag_data_chain(rdata,15,POSA_HICANN);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Each bit selects one type of DNC packet
	/// bit 0 : pulse_enable
	/// bit 1 : config_enable
	/// bit 2 : routing_enable
	/// bit 3 : dnc_cfg_enable
	/// bit 4 : ram_heap_enable
	/// bit 5 : limit_enable
	/// bit 6 : get_dnc_status
	/// bit 7 : get_dnc_crc
	/// bit 8 : start dnc_if
	/// bit 9 : master = 1 / slave = 0
	bool FPGA_start_dnc_packet(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD2_SET_TX_CTRL, POSA_FPGA);
		set_jtag_data_chain (wdata,10,POSA_FPGA);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_set_channel(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD2_SET_TX_CHANNEL,POSA_FPGA);
		set_jtag_data_chain (wdata,4,0);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// GHz PLL control std::vector.
	/// bit 0    :    enable VCO
	/// bit 1    :    enable 1GHz
	/// bit 2    :    enable 500MHz
	bool HICANN_set_GHz_pll_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD3_PLL2G_CTRL,POSA_HICANN);
		set_jtag_data_chain (wdata,3,POSA_HICANN);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// FPGA control std::vector.
	/// bit 0    :    A_SYNC_RESET
	bool FPGA_set_fpga_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain (CMD2_SET_FPGA_CTRL,POSA_FPGA);
		set_jtag_data_chain(wdata,9,POSA_FPGA);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool FPGA_get_status(T& rdata)
		{
			set_jtag_instr_chain (CMD2_GET_STATUS, POSA_FPGA);
			get_jtag_data_chain (rdata,5,POSA_FPGA);

			return true;
		}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
		bool FPGA_get_rx_channel(T& rdata)
		{
			set_jtag_instr_chain (CMD2_GET_RX_CHANNEL, POSA_FPGA);
			get_jtag_data_chain (rdata,8,POSA_FPGA);

			return true;
		}


	bool HICANN_set_bias(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD3_BIAS_CTRL,POSA_HICANN);
		set_jtag_data_chain (wdata,6,POSA_HICANN);

		return true;
	}

	bool HICANN_set_test(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD3_SET_TEST_CTRL,POSA_HICANN);
		set_jtag_data_chain(wdata,4,POSA_HICANN);

		return true;
	}

	bool HICANN_set_reset(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD3_SYSTEM_ENABLE,POSA_HICANN);

		set_jtag_data_chain(wdata,1,POSA_HICANN);

		return true;
	}

	bool HICANN_set_link_ctrl(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD3_LINK_CTRL,POSA_HICANN);

		set_jtag_data_chain(wdata,9,POSA_HICANN);

		return true;
	}


	template<typename T>
		bool HICANN_set_clk_delay(T wdata, T& rdata)
		{
			set_jtag_instr_chain (CMD3_SET_DELAY_RX_CLK,POSA_HICANN);
			set_get_jtag_data_chain(wdata,rdata,6,POSA_HICANN);
			
			return true;
		}
	
	template<typename T>
		bool HICANN_set_data_delay(T wdata, T& rdata)
		{
			set_jtag_instr_chain (CMD3_SET_DELAY_RX_DATA,POSA_HICANN);
			set_get_jtag_data_chain(wdata,rdata,6,POSA_HICANN);
			
			return true;
		}
	




	///////////////////////////////////////////////////////////////////////////////


	//////////////////////////////////////////////////////////////////////////////
	/// Controll access to Faraday PLL
	bool HICANN_set_pll_far_ctrl(unsigned short wdata)
	{
		set_jtag_instr_chain (CMD3_PLL_FAR_CTRL,POSA_HICANN);
		set_jtag_data_chain(wdata,15,POSA_HICANN);
		
		return true;
	}

	// PLL control register defined in "jtag_hicann.sv"
	bool HICANN_set_pll_far_ctrl(uint ms, uint ns, uint pdn, uint frange, uint test)
	{
		int wdata =
			((ms     & 0x3f) << 9)
			| ((ns     & 0x3f) << 3)
			| ((pdn    & 0x01) << 2)
			| ((frange & 0x01) << 1)
			| ((test   & 0x01) << 0);
		return HICANN_set_pll_far_ctrl(wdata);
	}

	//////////////////////////////////////////////////////////////////////////////
	// ARQ control functions 
	//////////////////////////////////////////////////////////////////////////////

	// write control reg
	bool HICANN_arq_write_ctrl(unsigned short wdata0, unsigned short wdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_CTRL,POSA_HICANN);
		unsigned long wdata = ((unsigned long)wdata0 << 16) | wdata1;
		set_jtag_data_chain(wdata, 32,POSA_HICANN);
		return true;
	}
	
	// write tx timeout
	bool HICANN_arq_write_tx_timeout(unsigned short wdata0, unsigned short wdata1)
	{
		set_jtag_instr_chain (CMD3_ARQ_TXTOVAL,POSA_HICANN);
		unsigned long wdata = ((unsigned long)wdata0 << 16) | wdata1; 
		set_jtag_data_chain(wdata, 32,POSA_HICANN);
		return true;
	}

	// write rx timeout
	bool HICANN_arq_write_rx_timeout(unsigned short wdata0, unsigned short wdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXTOVAL,POSA_HICANN);
		unsigned long wdata = ((unsigned long)wdata0 << 16) | wdata1; 
		set_jtag_data_chain(wdata, 32,POSA_HICANN);
		return true;
	}

	// read tx timeout num
	bool HICANN_arq_read_tx_timeout_num(unsigned short & rdata0, unsigned short & rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_TXTONUM,POSA_HICANN);
		unsigned long rdata;
		get_jtag_data_chain(rdata,32,POSA_HICANN);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read tx timeout num
	bool HICANN_arq_read_rx_timeout_num(unsigned short & rdata0, unsigned short & rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXTONUM,POSA_HICANN);
		unsigned long rdata;
		get_jtag_data_chain(rdata,32,POSA_HICANN);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read tx packet num
	bool HICANN_arq_read_tx_packet_num(unsigned short & rdata0, unsigned short & rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_TXPCKNUM,POSA_HICANN);
		unsigned long rdata;
		get_jtag_data_chain(rdata,32,POSA_HICANN);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read rx packet num
	bool HICANN_arq_read_rx_packet_num(unsigned short & rdata0, unsigned short & rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXPCKNUM,POSA_HICANN);
		unsigned long rdata;
		get_jtag_data_chain(rdata,32,POSA_HICANN);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read rx drop num
	bool HICANN_arq_read_rx_drop_num(unsigned short & rdata0, unsigned short & rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXDROPNUM,POSA_HICANN);
		unsigned long rdata;
		get_jtag_data_chain(rdata,32,POSA_HICANN);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}


	/////////////////////////////////////////////////////////////////////////////
	/// Collections of functions for OCP packet transmission
	bool HICANN_submit_ocp_packet (uint64 wdata)
	{
		HICANN_set_test(0xc);
		HICANN_set_tx_data(wdata);
		HICANN_start_cfg_pkg();

		HICANN_set_test(0x0);
		return true;
	}
	
	/// Returns last received data packet, requires that last packet is the requested one.
	bool HICANN_receive_ocp_packet (uint64& rdata)
	{
		HICANN_get_rx_data(rdata);
		return true;
	}


	//#####################################################################
	// I2C specific stuff
	// in case other devices are present in chain, set themm all to BYPASS
	// before executing any of the following
	// all commands rely on the fact the FPGA is first device in chain
	
	// ***********************************************
	bool FPGA_i2c_setctrl(unsigned char ctrladdr, unsigned char ctrlreg) {
		set_jtag_instr_chain(CMD_OCP_SET_DATA_EXEC, POSA_FPGA);

		uint data = 1<<13 | ctrladdr<<8 | ctrlreg; /*!!!fix to ocp addr/data type*/
		set_jtag_data_chain(data, JTAG_OCP_DR_WIDTH, POSA_FPGA);
		return true;
	}
	
	// ***********************************************
	bool FPGA_i2c_getctrl(unsigned char ctrladdr, unsigned char & ctrlreg, uint chainlen=1) {
		set_jtag_instr_chain(CMD_OCP_SET_DATA_EXEC, POSA_FPGA);

		uint data = 0<<13 | ctrladdr<<8; /*!!!fix to ocp addr/data type*/
		set_jtag_data_chain(data, JTAG_OCP_DR_WIDTH, POSA_FPGA);

		set_jtag_instr_chain(CMD_OCP_GET_DATA, POSA_FPGA);

		get_jtag_data_chain(ctrlreg, 8, POSA_FPGA);
		return true;
	}
	
	// ***********************************************
	bool FPGA_i2c_byte_write(unsigned char data, bool start, bool stop, bool nack){
		FPGA_i2c_setctrl(ADDR_TXREG, data);
		
	  uint cmd = 0 | start<<7 | stop<<6 | 1<<4 | nack<<3;
		FPGA_i2c_setctrl(ADDR_CMDREG, cmd);
		
		return true;
	}

	// ***********************************************
	bool FPGA_i2c_byte_read(bool start, bool stop, bool nack, unsigned char& data){
	  uint cmd = 0 | start<<7 | stop<<6 | 2<<4 | nack<<3;
		FPGA_i2c_setctrl(ADDR_CMDREG, cmd);
		
		FPGA_i2c_getctrl(ADDR_RXREG, data);
		return true;
	}

	// end I2C specific stuff
	//#####################################################################


	//#####################################################################
	// FPGA's HICANN control reg specific stuff
	
	bool FPGA_set_hcctrl (unsigned char ctrlreg)
	{
		set_jtag_instr_chain(CMD_OCP_SET_DATA_EXEC, POSA_FPGA);

	  uint tmp = 1<<13 | 0x10<<8 | ctrlreg; /*!!!fix to ocp addr/data type*/
		set_jtag_data_chain(tmp, JTAG_OCP_DR_WIDTH, POSA_FPGA); /*!!!fix to consts derived from hw*/
		
		return true;
	}

	bool FPGA_get_hcctrl (uint& ctrlreg)
	{
		set_jtag_instr_chain(CMD_OCP_SET_DATA_EXEC, POSA_FPGA);

	  uint64 tmp = (uint64)0<<13 | (uint64)0x10<<8 | 0; /*!!!fix to ocp addr/data type*/
		set_jtag_data_chain(tmp, JTAG_OCP_DR_WIDTH, POSA_FPGA); /*!!!fix to consts derived from hw*/
		
		set_jtag_instr_chain(CMD_OCP_GET_DATA, POSA_FPGA);

		get_jtag_data_chain(ctrlreg, 8, POSA_FPGA); /*!!!fix to ocp addr/data type*/
		return true;
	}

	// end HICANN control reg specific stuff
	//#####################################################################



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
				this->logging("not yet implemented");
			}
		else
			{
				this->logging("could not open file '%s'", name);
			}
		return true;
	}


};


#endif//__jtag_cmdbase__
