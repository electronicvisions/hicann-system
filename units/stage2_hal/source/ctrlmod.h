// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   ctrlmod.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Mon Jan 12 13:09:24 2009                
// Last Change     :   Mon Jan 12 09    
// by              :   agruebl        
//------------------------------------------------------------------------

#ifndef _CTRLMOD_H_
#define _CTRLMOD_H_

#include "s2_types.h"

#ifdef DEPRECATED
#error "Uh, nice! DEPRECATED has been added to some more-generic header..."
#endif
#define DEPRECATED(func) func __attribute__ ((deprecated))

#include "s2ctrl.h"

// fwd decl
struct HostALController;

namespace facets {

/** Common base class for all (HICANN or - morge general - Stage 2) control modules.
    It provides access to the utilized link layer and holds common variables like the 
		address range used by the module
*/

class CtrlModule
{
public:
	CtrlModule(Stage2Ctrl* c, uint ta, ci_addr_t sa, ci_addr_t ma);
	virtual ~CtrlModule();

	Stage2Ctrl* s2c;    ///< Underlying communication

	/// access functions

	/// issue write command to 'my' control module
	int write_cmd(ci_addr_t a, ci_data_t d, uint del);
	/// issue read command to 'my' control module
	int read_cmd(ci_addr_t a, uint del);

	#ifndef WITHOUT_ARQ
	/** check for received data from 'my' control module select blocking or nonblocking access */
	int get_data(ci_addr_t &addr, ci_data_t &data);
	int get_data(ci_payload *pckt);
	#endif

	/// auxiliary functions

	/// !!! inclomplete definition of data check routine
	bool check(uint64 data, uint64 mask, uint64 val){
		std::cout << "write some useful debug output for data checking" << std::endl;
		return (data&mask) == val;
	}


private:
	uint       _tagid;     ///< "address"/number of tag that this ctrlmodule is associated to
	ci_addr_t  _startaddr;  ///< address offset for this module
	ci_addr_t  _maxaddr;  ///< larges allowed addressvalue for this module (counted from 0!)
	ci_payload senddata;

	void checkAddr(ci_addr_t a, std::string prefix);

	uint startaddr(void){return _startaddr;};	///< return address range of this instance.
	uint maxaddr(void){return _maxaddr;};


protected:
	// "new" logger class
	Logger& log;
	//HW delay of chip components (setting default for derived classes, value cf. #2172) the exact value
	//has to be investigated further and possibly be split up in multiple delays (depending on read/write or physical location)
	uint del;
};

}  // end of namespace facets

#endif //_CTRLMOD_H_
