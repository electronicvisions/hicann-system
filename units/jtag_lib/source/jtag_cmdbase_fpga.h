
#ifndef __JTAG_CMDBASE_FPGA__
#define __JTAG_CMDBASE_FPGA__

#include "logger.h"
#include "jtag_lib.h"
#include <vector>


	
#ifndef JTAG_CLK_PERIOD
#define JTAG_CLK_PERIOD	11111     // jtag clock speed
#endif

// JTAG related defines
#define FPGA_IR_WIDTH 10   // instruction register width (bit)

// FPGA specific stuff
#define FPGA_JA 					 1   // device number in JTAG chain

// virtex-5 JTAG instructions
#define FPGA_CMD_BYPASS    0x3FF
#define FPGA_CMD_READ_ID   0x3C9
#define FPGA_CMD_USER1     0x3C2
#define FPGA_CMD_USER2     0x3C3
#define FPGA_CMD_USER3     0x3E2
#define FPGA_CMD_USER4     0x3E3

// ************ HICANN JTAG instructions ******************* //	
#define HICANN_JA							0   // device number in JTAG chain
#define IR_WIDTH           		6
#define CMD_BYPASS  			  	0x3f
#define CMD_READID          	0x00
#define CMD_LVDS_PADS_EN    	0x02
#define CMD_LINK_CTRL 				0x03
#define CMD_LAYER1_MODE				0x04
#define CMD_SYSTEM_ENABLE			0x05
#define CMD_BIAS_CTRL			  	0x06
#define CMD_SET_IBIAS 				0x07
#define CMD_START_LINK   			0x08
#define CMD_STOP_LINK   			0x09
#define CMD_STOP_TIME_COUNT		0x0a
#define CMD_READ_SYSTIME 	 		0x0b
#define CMD_SET_2XPLS  				0x0d
#define CMD_PLL2G_CTRL	 			0x10
#define CMD_SET_TX_DATA				0x11
#define CMD_GET_RX_DATA				0x12
#define CMD_SET_TEST_CTRL   	0x17
#define CMD_START_CFG_PKG   	0x18
#define CMD_START_PULSE_PKG 	0x19
#define CMD_READ_STATUS     	0x1a
#define CMD_SET_RESET	  			0x1b
#define CMD_REL_RESET	  			0x1c
#define CMD_SET_DELAY_RX_DATA	0x21
#define CMD_SET_DELAY_RX_CLK	0x22
#define CMD_READ_CRC_COUNT  	0x27
#define CMD_RESET_CRC_COUNT 	0x28

// UHEI instructions
#define CMD_PLL_FAR_CTRL			0x29
#define ARQ_CTRL          		0x30
#define ARQ_CTRL_RESET     		0x01
#define ARQ_CTRL_LOOPBACK  		0x02
#define ARQ_TXPCKNUM      		0x31
#define ARQ_RXPCKNUM      		0x32
#define ARQ_RXDROPNUM     		0x33
#define ARQ_TXTOVAL       		0x34
#define ARQ_RXTOVAL       		0x35
#define ARQ_TXTONUM       		0x36
#define ARQ_RXTONUM       		0x37

// 
#define I2C_CMD_NOP   0x0
#define I2C_CMD_START 0x1
#define I2C_CMD_STOP  0x2
#define I2C_CMD_WRITE 0x4
#define I2C_CMD_READ  0x8

// ocp address meanings in i2c master
#define ADDR_PRESC_L 0x0
#define ADDR_PRESC_M 0x1
#define ADDR_CTRL    0x2
#define ADDR_RXREG   0x3
#define ADDR_STATUS  0x4
#define ADDR_TXREG   0x5
#define ADDR_CMDREG  0x6

// ************ JTAG to OCP instructions *************** //
#define JTAG_OCP_IR_WIDTH 2      // instruction reister width for ocp-chain in FPGA
#define JTAG_OCP_DR_WIDTH 14     // data reister width for ocp-chain in FPGA
#define SET_OCP_DATA_EXEC 0x1    // set ocp address, data and rw bit and execute transfer
#define POLL_OCP          0x0    // poll if slave finished
#define GET_OCP_DATA      0x2    // read back ocp data


//////////////////////////////////////////////////////////////////////////////////
/// .
template<typename JTAG>
class jtag_cmdbase_fpga : public JTAG
{
protected:
	// "new" logger class
	Logger& log;

public:

	bool active;
	
	jtag_cmdbase_fpga():log(Logger::instance("hicann-system.jtag_cmdbase_fpga", Logger::INFO,"")),active(false){};

	//////////////////////////////////////////////////////////////////////////////
	/// Reset of Jtag with ten clock TMS=1
	bool reset_jtag(void)
	{
		this->jtag_write(0,1,10);   // Go to TEST-LOGIC-RESET with TMS all 1
		this->jtag_write(0,0);      // Go to RUN-TEST/IDLE
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
	void set_jtag_instr (T command)
	{
		if(active)
			log(Logger::ERROR) << "##### JTAG collision!!! set_jtag_instr #####" << std::endl;
		else active = true;
		
		unsigned char i;

		this->jtag_write(0,0);   // Go to RUN-TEST/IDLE
		this->jtag_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_write(0,1);   // Go to SELECT-IR-SCAN
		this->jtag_write(0,0);   // Go to CAPTURE-IR
		this->jtag_write(0,0);   // Go to SHIFT-IR

		for(i=0; i<FPGA_IR_WIDTH; i++)
		{
			this->jtag_write( command&1, i ==(FPGA_IR_WIDTH-1) );
			command >>=1;
		}

		this->jtag_write(0,1);   // Go to UPDATE-IR load IR with shift reg. value
		this->jtag_write(0,0);   // Go to RUN-TEST/IDLE

		active = false;
	}

	template<typename T>
	void set_jtag_instr_bulk (T command)
	{
		unsigned char i;

		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
		this->jtag_bulk_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_bulk_write(0,1);   // Go to SELECT-IR-SCAN
		this->jtag_bulk_write(0,0);   // Go to CAPTURE-IR
		this->jtag_bulk_write(0,0);   // Go to SHIFT-IR

		for(i=0; i<FPGA_IR_WIDTH; i++)
		{
			this->jtag_bulk_write( command&1, i ==(FPGA_IR_WIDTH-1) );
			command >>=1;
		}

		this->jtag_bulk_write(0,1);   // Go to UPDATE-IR load IR with shift reg. value
		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
	}

	void set_jtag_instr_multi_bulk (std::vector<uint>& cmds, std::vector<uint>& widths)
	{
		if(cmds.size() != widths.size())
			log(Logger::ERROR) << "set_jtag_instr_multi_bulk: input std::vector sizes don't match!";
		
		uint command,width;
		
		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
		this->jtag_bulk_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_bulk_write(0,1);   // Go to SELECT-IR-SCAN
		this->jtag_bulk_write(0,0);   // Go to CAPTURE-IR
		this->jtag_bulk_write(0,0);   // Go to SHIFT-IR

		// shift in instructions
		for(uint c=0;c<cmds.size();c++){
			command = cmds[c];
			width = widths[c];
			for(uint i=0; i<width; i++){
				this->jtag_bulk_write( command&1, (i==(width-1) && c==cmds.size()-1) );
				command >>=1;
			}
		}

		this->jtag_bulk_write(0,1);   // Go to UPDATE-IR load IR with shift reg. value
		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
	}

 	//////////////////////////////////////////////////////////////////////////////
	/// Additional two clock cycles for INSTRUCTION only operation (sync of update sig)
	void execute_instr(void)
	{
		if(active)
			std::cout << "##### JTAG collision!!! execute_instr #####" << std::endl;
		else active = true;

		this->jtag_write(0,0);   // Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_write(0,0);   // Stay in RUN-TEST/IDLE for operations without USER REG

		active = false;
	}

	void execute_instr_bulk(void)
	{
		this->jtag_bulk_write(0,0);   // Stay in RUN-TEST/IDLE for operations without USER REG
		this->jtag_bulk_write(0,0);   // Stay in RUN-TEST/IDLE for operations without USER REG
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
	void set_jtag_data(T& wdata, unsigned char width)
	{
		if(active)
			log(Logger::ERROR) << "##### JTAG collision!!! set_jtag_data #####" << std::endl;
		else active = true;
		
		unsigned char i;
		
		this->jtag_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_write(0,0);   // Go to CAPTURE-DR
		this->jtag_write(0,0);   // Go to SHIFT-DR

		for(i=0; i < width; i++)
		{   
			this->jtag_write( (unsigned int)(wdata&1), i == width-1);
			wdata >>=1;
		}

		this->jtag_write(0,1);   // Go to UPDATE-DR - no effect
		this->jtag_write(0,0);   // Go to RUN-TEST/IDLE

		active = false;
	}

	template<typename T>
	void set_jtag_data_bulk(T& wdata, unsigned char width)
	{
		unsigned char i;
		
		this->jtag_bulk_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_bulk_write(0,0);   // Go to CAPTURE-DR
		this->jtag_bulk_write(0,0);   // Go to SHIFT-DR

		for(i=0; i < width; i++)
		{   
			this->jtag_bulk_write( (unsigned int)(wdata&1), i == width-1);
			wdata >>=1;
		}

		this->jtag_bulk_write(0,1);   // Go to UPDATE-DR - no effect
		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
	void set_jtag_data_multi_bulk (std::vector<T>& data, std::vector<uint>& widths)
	{
		if(data.size() != widths.size())
			log(Logger::ERROR) << "set_jtag_data_multi_bulk: input std::vector sizes don't match!";
		
		uint64 dr;
		uint   width;
		
		this->jtag_bulk_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_bulk_write(0,0);   // Go to CAPTURE-DR
		this->jtag_bulk_write(0,0);   // Go to SHIFT-DR

		// shift in instructions
		for(uint d=0;d<data.size();d++){
			dr = data[d];
			width = widths[d];
			for(uint i=0; i<width; i++){
				this->jtag_bulk_write( dr&1, (i==(width-1) && d==data.size()-1));
				dr >>=1;
			}
		}

		this->jtag_bulk_write(0,1);   // Go to UPDATE-IR load IR with shift reg. value
		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
	void get_jtag_data(T& rdata, unsigned char width)
	{
		if(active)
			log(Logger::ERROR) << "##### JTAG collision!!! get_jtag_data #####" << std::endl;
		else active = true;
		
		unsigned char i;
		
		T bit;
		
		this->jtag_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_write(0,0);   // Go to CAPTURE-DR
		this->jtag_write(0,0);   // Go to SHIFT-DR

		for(rdata=0, i=0; i < width; i++)
		{
			bit = this->jtag_read();
			rdata |= bit << i;
			this->jtag_write( 0, i==width-1);   // Go to EXIT1-DR after width shift steps
		}

		this->jtag_write(0,1);   // Go to UPDATE-DR - no effect
		this->jtag_write(0,0);   // Go to RUN-TEST/IDLE
		
		active = false;
	}

	void get_jtag_data_bulk(unsigned char width)
	{
		unsigned char i;
		
		this->jtag_bulk_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_bulk_write(0,0);   // Go to CAPTURE-DR
		this->jtag_bulk_write(0,0);   // Go to SHIFT-DR

		for(i=0; i < width; i++)
		{
			// now read
			this->jtag_bulk_write( 0, i==width-1, jtag_lib::TDO_READ);   // Go to EXIT1-DR after width shift steps
		}

		this->jtag_bulk_write(0,1);   // Go to UPDATE-DR - no effect
		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
	}

	void get_jtag_data_multi_bulk(std::vector<uint>& widths)
	{
		uint width;
		
		this->jtag_bulk_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_bulk_write(0,0);   // Go to CAPTURE-DR
		this->jtag_bulk_write(0,0);   // Go to SHIFT-DR

		for(uint w=0;w<widths.size();w++){
			width = widths[w];
			for(uint i=0;i<width;i++){
				// now read
				// Go to EXIT1-DR after width shift steps
				this->jtag_bulk_write( 0, (i==width-1 && w==widths.size()-1), jtag_lib::TDO_READ);
			}
		}

		this->jtag_bulk_write(0,1);   // Go to UPDATE-DR - no effect
		this->jtag_bulk_write(0,0);   // Go to RUN-TEST/IDLE
	}

	// fetch JTAG bulk read data
	template<typename T>
	void jtag_bulk_read_multi(std::vector<T>& data, std::vector<uint>& widths)
	{
		if(data.size() != widths.size())
			log(Logger::ERROR) << "jtag_bulk_read_multi: input std::vector sizes don't match!";
		

		T curdata;
		uint cread;
		
		for(uint d=0;d<data.size();d++){
			uint numbytes = widths[d]/32;
			uint rem = widths[d]%32;
			uint numreads = rem ? numbytes+1 : numbytes;
cout << "reading 0x" << std::hex << widths[d] << " bits, 0x" << numbytes << " bytes, rem " << rem << " in " << numreads << " read ops" << std::endl;
			curdata = 0;
			for(uint i=0;i<numreads;i++){
				cread=0;
				uint bits = i==numbytes ? rem : 32;
				this->jtag_bulk_read(cread, bits);
				curdata |= cread << (i*32);
				std::cout << "jtag_bulk_read_multi: read byte 0x" << std::hex << cread << std::endl;
			}
			data[d] = curdata;
			
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////
	/// .
	template<typename T>
	void set_get_jtag_data(T wdata, T& rdata, unsigned int count)
	{
		if(active)
			log(Logger::ERROR) << "##### JTAG collision!!! set_get_jtag_data #####" << std::endl;
		else active = true;
		
		unsigned int	i;
		
		T bit;

		this->jtag_write(0,1);   // Go to SELECT-DR-SCAN
		this->jtag_write(0,0);   // Go to CAPTURE-DR
		this->jtag_write(0,0);   // Go to SHIFT-DR

		for(rdata=0,i=0; i < count; i++)
		{
			bit = this->jtag_read();
			rdata |= bit << i;
			this->jtag_write( (unsigned int)(wdata&1), i==count-1);
			wdata >>=1;
		}
		
		this->jtag_write(0,1);   // Go to UPDATE-DR - no effect
		this->jtag_write(0,0);   // Go to RUN-TEST/IDLE
		
		active = false;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// Init function for first access.
	/// Includes reset call.
	bool InitJTAG(void)
	{
		log(Logger::INFO) << "FPGA_IR_WIDTH set to " << FPGA_IR_WIDTH;
		
		reset_jtag();
		
		return true;
	}
	

	//////////////////////////////////////////////////////////////////////////////
	/// Bypass of data through chip, used for JTAG chain
	template<typename T>
	bool bypass(T wdata, unsigned int count, T& rdata)
	{
		set_jtag_instr (FPGA_CMD_BYPASS);

		set_get_jtag_data(wdata,rdata,count);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Reads JTAG identification resgister
	template<typename T>
	bool read_jtag_id(T& rdata)
	{
		set_jtag_instr (FPGA_CMD_READ_ID);
		get_jtag_data(rdata,32);
		return true;
	}


	//#####################################################################
	// I2C specific stuff
	// in case other devices are present in chain, set themm all to BYPASS
	// before executing any of the following
	// all commands rely on the fact the FPGA is first device in chain
	
	// ***********************************************
	bool i2c_setctrl(unsigned char ctrladdr, unsigned char ctrlreg, uint chainlen=1) {
		std::vector<uint64> data;   data.resize(chainlen,0);
		std::vector<uint> widths; widths.resize(chainlen,0);
		
		// set chain command
		for(uint i=0;i<chainlen;i++)
			if(i<chainlen-1) widths[i] = (1); // bypass reg
			else             widths[i] = (JTAG_OCP_IR_WIDTH);
		for(uint i=0;i<chainlen;i++)
			if(i<chainlen-1) data[i] = (0); // bypass reg
			else             data[i] = (SET_OCP_DATA_EXEC);
		set_jtag_data_multi_bulk(data, widths);
		
		// set chain data for immediate execute
		widths[chainlen-1] = JTAG_OCP_DR_WIDTH;
		data[chainlen-1]   = (uint64)1<<13 | (uint64)ctrladdr<<8 | ctrlreg; /*!!!fix to ocp addr/data type*/
		set_jtag_data_multi_bulk(data, widths);
		
		// execute bulk commands
		this->jtag_bulk_execute();
		return true;
	}
	
	// ***********************************************
	bool i2c_getctrl(unsigned char ctrladdr, unsigned char & ctrlreg, uint chainlen=1) {
		std::vector<uint64> data;   data.resize(chainlen,0);
		std::vector<uint> widths; widths.resize(chainlen,0);

		// set chain command
		for(uint i=0;i<chainlen;i++)
			if(i<chainlen-1) widths[i] = (1); // bypass reg
			else             widths[i] = (JTAG_OCP_IR_WIDTH);
		for(uint i=0;i<chainlen;i++)
			if(i<chainlen-1) data[i] = (0); // bypass reg
			else             data[i] = (SET_OCP_DATA_EXEC);
		set_jtag_data_multi_bulk(data, widths);
		
		// set chain data for immediate execute
		widths[chainlen-1] = JTAG_OCP_DR_WIDTH;
		data[chainlen-1]   = (uint64)0<<13 | (uint64)ctrladdr<<8; /*!!!fix to ocp addr/data type*/
		set_jtag_data_multi_bulk(data, widths);
		
		// set chain command
		for(uint i=0;i<chainlen;i++)
			if(i<chainlen-1) widths[i] = (1); // bypass reg
			else             widths[i] = (JTAG_OCP_IR_WIDTH);
		for(uint i=0;i<chainlen;i++)
			if(i<chainlen-1) data[i] = (0); // bypass reg
			else             data[i] = (GET_OCP_DATA);
		set_jtag_data_multi_bulk(data, widths);
		
		// read back data
		widths[chainlen-1] = 8; // 8 is width of control reg
		get_jtag_data_multi_bulk(widths);

		// execute bulk commands
		this->jtag_bulk_execute();

		jtag_bulk_read_multi(data, widths);
		ctrlreg = data[chainlen-1];
		
		return true;
	}
	
	// ***********************************************
	bool i2c_byte_write(unsigned char data, bool start, bool stop, bool nack, uint chainlen=1){
		i2c_setctrl(ADDR_TXREG, data, chainlen);
		
	  uint cmd = 0 | start<<7 | stop<<6 | 1<<4 | nack<<3;
		i2c_setctrl(ADDR_CMDREG, cmd, chainlen);
		
		return true;
	}

	// ***********************************************
	bool i2c_byte_read(bool start, bool stop, bool nack, unsigned char& data, uint chainlen=1){
	  uint cmd = 0 | start<<7 | stop<<6 | 2<<4 | nack<<3;
		i2c_setctrl(ADDR_CMDREG, cmd, chainlen);
		
		i2c_getctrl(ADDR_RXREG, data, chainlen);
		return true;
	}

	// end I2C specific stuff
	//#####################################################################


	//#####################################################################
	// FPGA's HICANN control reg specific stuff
	
	bool set_hcctrl (unsigned char ctrlreg)
	{
		uint tmp_cmd = SET_OCP_DATA_EXEC;
		set_jtag_data(tmp_cmd, JTAG_OCP_IR_WIDTH);

	  uint64 tmp = (uint64)1<<13 | (uint64)0x10<<8 | ctrlreg; /*!!!fix to ocp addr/data type*/
		set_jtag_data(tmp, JTAG_OCP_DR_WIDTH); /*!!!fix to consts derived from hw*/
		
		return true;
	}

	bool get_hcctrl (uint& ctrlreg)
	{
		uint tmp_cmd = SET_OCP_DATA_EXEC;
		set_jtag_data(tmp_cmd, JTAG_OCP_IR_WIDTH);

	  uint64 tmp = (uint64)0<<13 | (uint64)0x10<<8 | 0; /*!!!fix to ocp addr/data type*/
		set_jtag_data(tmp, JTAG_OCP_DR_WIDTH); /*!!!fix to consts derived from hw*/
		
		tmp_cmd = GET_OCP_DATA;
		set_jtag_data(tmp_cmd, JTAG_OCP_IR_WIDTH);

		get_jtag_data(ctrlreg, 8); /*!!!fix to ocp addr/data type*/
		return true;
	}

	// end HICANN control reg specific stuff
	//#####################################################################

};

#endif//__JTAG_EMULATOR__
