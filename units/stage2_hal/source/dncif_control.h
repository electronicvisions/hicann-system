#pragma once
// Company         :   tud
// Author          :   Stefan Scholze
// E-Mail          :   Stefan Scholze
//                    			
// Filename        :   dncif_control.h
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jun 24 08
// Last Change     :   Tue Mar 08 12
// by              :   mehrlich
//------------------------------------------------------------------------

#ifndef _DNCIF_CTRL_H_
#define _DNCIF_CTRL_H

#include "ctrlmod.h"

namespace facets {

// Access to DNC interface settings

// error value with all bits set
#define UINT_VOID ~((unsigned int)(0))

///
/// ???
///
class DncIfControl : public CtrlModule {
public:

	/// give pointer to utilized LinkLayer, start and range of this module's address range
	DncIfControl(
		Stage2Ctrl* c,  //!< pointer to parent Stage2Ctrl class (i.e. HicannCtrl)
		uint ta, //!< associated OCP tagid 
		uint sa, //!< Startaddress (OCP)
		uint ma  //!< Maximumadress (OCP)
		);

	virtual ~DncIfControl()
	{}

	/// write DNCIF configuration
	/// @param ctrl_vector -- ???
	/// [XX:YY]: ???
	/// [YY:0]: ???
	void write_link_ctrl(
		uint ctrl_vector  //!< DNCIF configuration std::vector
		);
	
	/// send request for status.
	void read_status(
		);
	
#ifndef WITHOUT_ARQ

	// return result of status request.
	unsigned int get_read_status();

#endif // WITHOUT_ARQ
	
};

}  // end of namespace facets
 
 #endif //_DNCIF_CTRL_H_
