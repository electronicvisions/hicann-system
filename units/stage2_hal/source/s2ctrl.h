// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   s2ctrl.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Fri Jun 20 08:12:24 2008                
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------

#ifndef _S2CTRL_H_
#define _S2CTRL_H_

#include "s2comm.h"
#include "logger.h"

namespace facets {


class Stage2Ctrl :
	public LoggerMixin
{
private:
	uint _addr; ///< consecutive number of this control module.

protected:
	virtual std::string ClassName() { return "Stage2Ctrl"; };
	virtual std::ostream & dbg() { return std::cout << ClassName() << ": "; }
	virtual std::ostream & dbg(int level) { if (debug_level>=level) return dbg(); else return NULLOS; }
	int debug_level; // debug level

	Stage2Comm *comm; ///< communication class
	
public:
	
	/// give pointer to communication medium class, Address of this instance
	Stage2Ctrl(Stage2Comm *c, uint addr);
	virtual ~Stage2Ctrl();

	uint addr(){return _addr;};	///< return address of this control module.
	void setAddr(uint a){_addr = a;};	///< set address of this control module.

	Stage2Comm * GetCommObj() { return comm; }


	// *** communication with selected communication medium ***
	
	/// send a read or write command via the communication medium.
	int issueCommand(ci_payload *data, uint tagid, uint del){
		return comm->issueCommand(addr(), tagid, data, del);
	};

	/// Receive data from the communication medium.
	int recvData(ci_payload *data, uint tagid){ 
		return comm->recvData(addr(), tagid, data);
	};

	/// Transmit all accumulated data. For playback memory mode. Only atomic accesses in all other modes.
	int Flush(){
		return comm->Flush();
	};

	/// Clear all internal buffer memories and set playback memory pointers etc. to start/default values
	int Clear(){ 
		return comm->Clear();
	};

};

}  // end of namespace facets

#endif //_S2CTRL_H_
