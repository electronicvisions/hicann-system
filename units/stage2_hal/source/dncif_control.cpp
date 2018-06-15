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

#include "dncif_control.h"

using namespace facets;

DncIfControl::DncIfControl(Stage2Ctrl* c, uint ta, uint sa, uint ma):CtrlModule(c,ta,sa,ma) {
}

void DncIfControl::write_link_ctrl(uint ctrl_vector)
{
	write_cmd(0, ctrl_vector%512, del);
}

void DncIfControl::read_status()
{
	read_cmd(0,del);
}
	
#ifndef WITHOUT_ARQ

// return result of status request.
unsigned int DncIfControl::get_read_status()
{
	ci_payload tmp;
	if(get_data(&tmp) == 1)
	{
		return (unsigned int)(tmp.data);
	}

	return UINT_VOID;
}

#endif // WITHOUT_ARQ
 
