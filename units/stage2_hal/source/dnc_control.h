// Company         :   tud
// Author          :   Stefan Scholze
// E-Mail          :   Stefan.Scholze@tu-dresden.de
//
// Filename        :   dnc_control.h
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Tue Jun 24 08
// Last Change     :   Tue Mar 08 12
// by              :   mehrlich
//------------------------------------------------------------------------

#ifndef _DNC_CTRL_H_
#define _DNC_CTRL_H_

#include "s2ctrl.h"
#include "s2comm.h"
#include "logger.h"

namespace facets {

///
/// ???
///
class DNCControl : public Stage2Ctrl
{

	public:

		DNCControl(
			Stage2Comm* comm_class,  //!< ???
			unsigned int set_dnc_id  //!< ???
			);

		/// enable/disable processing of time stamp in DNC. 
		/// HICANNs are one/hot encoded in argument
		/// [7:0] LSB == Hicann 0
		/// true - enable time stamp control in DNC
		/// false - disable time stamp control in DNC
		void setTimeStampCtrl(
			unsigned char tsc  //!< ??? 
			);
			
		/// set transmission direction for 8 l1 channels for a HICANN.
		/// 1 means downward to HICANN, 0 upward to FPGA.
		void setDirection(
			uint64 dirs //!< ???
			);

	private:

		Logger& log; //!< reference to the global logger

		unsigned int dnc_id;  //!< ???
		std::vector<bool> directions;  //!< ???
		unsigned int curr_hicann;  //!< ???
		l2_packet_t curr_packet;  //!< ???

};

}  // end of namespace facets

#endif //_DNC_CTRL_H_

