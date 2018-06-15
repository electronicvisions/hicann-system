#pragma once
// Company         :   kip                      
// Author          :   Sebastian Millner		      
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   fg_control.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Wed Dec 09 09
// Last Change     :   Tue Mar 06 12 
// by              :   mehrlich    
//------------------------------------------------------------------------

#include "ctrlmod.h"
#include "logger.h"

/// TODO: Replace the ??? comments 
/// TODO: Put the parameter defaults in header
/// TODO: Move internal functions to private

namespace facets {

/// FGControl floating gate control class.
///
/// a floating gate block is arranged in lines and columns
/// starting with line 0 for a stored current value,
/// current and voltage values are stored alternatingly.
///
/// column 0 of each block is for global configuration, 
/// the remaining columns are for neuron configuration.
///
class FGControl : public CtrlModule
{

private:

	Logger& log;  //!< reference to the global logger
	
	uint maxcycle;  //!< the maximum number of cycles allowed in the programming process (8bit)
	uint currentwritetime;  //!< length of a current write pulse
	uint voltagewritetime;  //!< length of a voltage write pulse
	uint readtime;  //!< Time waited for stable value during read in write process
	uint acceleratorstep;  //!< Number of cycle after which intern write time is doubled
	uint lines;  //!< 
	uint pulselength;  //!< 
	uint groundVM;  //!< 
	uint calib;  //!< 
	uint fg_biasn;  //!< 
	uint fg_bias;  //!< 

	uint HICANNversion;  //!< HICANN version, 1 or 2
	
	 #if HICANN_V == 1
	    uint revert10bits(uint val);  //!< 
    #endif
    
	virtual std::string ClassName() { return "FGControl"; };
public:

	// for constants see "compare units/fgateCtrl/source/fgateCtrl.sv" 
	static const int TAG;

	static const int REG_BASE;  //!<  bit 8 set
	static const int REG_OP;  //!< ???
	static const int REG_ADDRINS;  //!< ???
	static const int REG_SLAVE;  //!< ???
	static const int REG_BIAS;  //!< ???

	static const int REG_OP_WIDTH;  //!<  pos 00010
	static const int REG_BIAS_WIDTH;  //!<  pos 01000
	static const int REG_SLAVE_BUSY;  //!<  bit 8

	static const int REG_TEST_NUM;  //!< ???

	static const int MEM_SIZE;  //!< size of the latch memory within one bank, i.e number of words (65)
	static const int MEM_WIDTH;  //!< width of one entry (one word) in the latch memory (20)
	static const int MEM0_BASE;  //!< ???
	static const int MEM1_BASE;  //!< bit 7 set

	fg_loc loc; //!< FG location this instance corresponds to
	
	/// give pointer to utilized LinkLayer, start and range of this module's address range
	FGControl(
	    fg_loc sl,  //!< location of the floating gate, one of {FGTL=0, FGTR=1, FGBL=2, FGBR=3}
			Stage2Ctrl* c,  //!< pointer to parent Stage2Ctrl class (i.e. HicannCtrl)
			uint ta, //!< associated OCP tagid 
	    uint sa,  //!< ???
	    uint ma  //!< ???
	    );
	
	~FGControl();

	/// fetch read data resulting from read_data.
	void get_read_data(
	    ci_addr_t& rowaddr,  //!< ???
	    ci_data_t& data  //!< ???
	    );
	
    /// ???	  
	void get_read_data_ram(
	    ci_addr_t& rowaddr,  //!< ???
	    ci_data_t& data  //!< ???
	    );
	
	/// write data to ram. 
	/// As two values are written simultaneously,
	/// two values must be given.
	/// When used for programming the FGs,
	/// the first value is written to the floating gate with lower id.
	/// When used for current stimulus,
	/// the first value is applied before the 2nd value to the target neuron.
	int write_ram(
	    uint bank,  //!< ram bank (0 or 1)
	    uint addr,  //!< address in range(MEM_SIZE)(0..64)
	    uint val1,  //!< first 10-bit value written to the ram
	    uint val2  //!< second 10-bit value written to the ram
	    ); 
	
    	/// reads data from ram block
	int read_ram(
	    uint bank,  //!< ram bank (0 or 1)
	    uint addr  //!< address in range(MEM_SIZE)(0..64)
	    );
	
	/// inits all entries of a ram bank with zero.
    	/// write zeros into all entries of a bank of the ram block.
	int initzeros(
	    uint bank  //!< ram bank (0 or 1)
	    );
	
	/// inits all entries of a ram bank with the maximum value.
    	/// write the maximum value(1023) into all entries of a bank of the ram block.
	int initmax(
	    uint bank  //!< ram bank (0 or 1)
	    );
	
	/// inits all entries of a ram bank with value val.
    	/// write the value val into all entries of a bank of the ram block.
	int initval(
	    uint bank,  //!< ram bank (0 or 1)
	    uint val  //!< value, with which all entries of the ram are initialized.
        );
	
	/// initializes value zero to val1 and others to val 2.
	int initval1(
	    uint bank,  //!< ???
	    uint val1,  //!< ???
	    uint val2  //!< ???
	    );
	
	/// initialize
	int initArray(
	    uint bank,  //!< ???
	    std::vector<int> val  //!< ???
	    );
	
	/// initialize 
	int initArray(
	    uint bank,  //!< ???
	    std::vector<int> val,  //!< ???
	    int neuron  //!< ???
	    );
	
    /// ???		
	int initSquare(
	    uint bank,  //!< ???
	    uint val  //!< ???
	    );

	/// Create a ramp stimulus
	int initRamp(
	    uint bank,  //!< ???
	    uint val  //!< ???
	    );
	
    /// ???	
	int write_biasingreg();
	
	/// send given std::vector for biasing direcly to fg.
	int write_biasingreg(
	    uint val  //!< ???
	        );
	
    /// ???		
	int write_operationreg();
	
    /// ???		
	int write_operationreg(
	    uint val  //!< ???
	    );
	
    /// ???		
	int write_instructionreg(
	    uint collumn,  //!< ???
	    uint line,  //!< ???
	    uint bank,  //!< ???
	    FG_pkg::ControlInstruction instruction  //!< ???
	    );
	
    /// ???		
	int program_cells(
	    uint collumn,  //!< ???
	    uint bank,  //!< ???
	    uint dir  //!< programming direction (0 - down from above, 1 - up from below)
	    ); 
	
    /// ???		
	int readout_cell(
	    uint collumn,  //!< ???
	    uint line  //!< ???
	    );

	/// get next false.
	/// gets column address of next cell which did not reach the programmed value during a programming process
	int get_next_false();
	
	/// stimulate a neuron once.
	/// stimulates a neuron using the values in one RAM bank as input; stops at last ram address.
	int stimulateNeurons(
	    int bank  //!< the ram bank (0 or 1) whose values are programmed
	    );
	
	/// stimulate a neuron continuously.
	/// stimulates a neuron using the values in one RAM bank as input continuously until another instruction is executed
	int stimulateNeuronsContinuous(
	    int bank  //!< the ram bank (0 or 1) whose values are programmed
	    );
	
	/// checks if programming state machine is busy.
	/// returns true, when state machine is active
	bool isBusy();
	
	/// checks whether, there was an error during programming.
	/// returns true, when a cell did not reach its value
	bool isError();
	
	/// ???	
	uint get_regslave();

	/// programs a single line. 
	/// row0 is programed to val0,
	/// all other are programed to val1
	int programLine(
	    uint row,  //!< ???
	    uint val0,  //!< ???
	    uint val1  //!< ???
	    );

	//uint get_offs(fg_loc loc); // ECM: not implemented

	//fg_loc get_loc(uint addr); // ECM: not implemented

	
	/// get the maximum number of cycles allowed in the programming process
	uint get_maxcycle();
	
    	/// set the maximum number of cycles allowed in the programming process
	void set_maxcycle(
	    uint data  //!< maxcycle (8bit)
	    );					

	
    /// ???	
	uint get_currentwritetime();
	
    /// ???		
	void set_currentwritetime(
	    uint data  //!< ???
	    );

	
    /// ???	
	uint get_voltagewritetime();
	
    /// ???		
	void set_voltagewritetime(
	    uint data  //!< ???
	    );

	
    /// ???	
	uint get_readtime();
	
    /// ???		
	void set_readtime(
	    uint data  //!< ???
	    );

	
    /// ???	
	uint get_acceleratorstep();
	
    /// ???		
	void set_acceleratorstep(
	    uint data  //!< ???
	    );

	
    /// ???
    uint get_pulselength();
	
    /// ???		
	void set_pulselength(
	    uint data  //!< ???
	    );

	
    /// ???	
	uint get_fg_biasn();
	
    /// ???		
	void set_fg_biasn(
	    uint data  //!< ???
	    );

	
    /// ???	
	uint get_fg_bias();
	
    /// ???	
	void set_fg_bias(
	    uint data  //!< ???
	    );
	    

    /// ???
	uint get_calib();
	
    /// ???	
	void set_calib(
	    uint data  //!< ???
	    );


    /// ???
	uint get_version();
	
	/// ???
	void set_version(
	    uint data  //!< ???
	    );

	/// resets all RAM and registers
	void reset();

	/// prints RAM state to stdout
	void print_config();

    /// write data to ram or registers
	int write_data(
	    uint rowaddr,  //!< ???
	    std::vector<bool> cfg  //!< ???
	    );
	    
	/// ???
	int write_data(
	    uint rowaddr,  //!< ???
	    uint cfg  //!< ???
	    );

	/// issue read command to read back data
	int read_data(
	    uint rowaddr  //!< ???
	    );

}; //end of fgctl
}  // end of namespace facets


