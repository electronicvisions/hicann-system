#pragma once
// Company         :   tud
// Author          :   Stefan Scholze
// E-Mail          :   Stefan.Scholze@tu-dresden.de
//                    			
// Filename        :   l1switch_control.h
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//                    			
// Create Date     :   Tue Jun 24 08
// Last Change     :   Tue Apr 11 12
// by              :   akononov
//------------------------------------------------------------------------

#include "ctrlmod.h"

namespace facets {

/// SPL1Control class.
/// Access to SPL1 configuration
//
/// Controls processing of SPL1 pulses in the DNC interface logic.
///
class SPL1Control : public CtrlModule
{
public:

	/// give pointer to utilized LinkLayer, start and range of this module's address range
	SPL1Control(
		Stage2Ctrl* c,  //!< pointer to parent Stage2Ctrl class (i.e. HicannCtrl)
		uint ta, //!< associated OCP tagid 
		uint sa, //!< Startaddress (OCP)
		uint ma  //!< Maximumadress (OCP)
		);

	virtual ~SPL1Control()
		{}

	/// write configuration for SPL1 transfers in DNC interface.
	/// @param spl1_ctrl -- switch configuration of one row
	/// [16]: use timestamps from L2 pulses;
	/// true - use timestamps
	/// false - ignore timestamps
	/// [15:8]: SPL1 direction for SPL1 channel 7:0 (0: HICANN->DNC, 1: DNC->HICANN);
	/// [7:0]: enable SPL1 channel 7:0
	/// (only effective during playback memory operation)
	void write_cfg(
		uint spl1_ctrl  //!< switch configuration
		);

	/// disables SPL1 operation in DNC interface.
	/// reset is automatically removed by subsequent write_cfg()
	/// (only effective during playback memory operation)
	void write_reset(
		);

	/// issue read statistics.
	/// (only effective during playback memory operation)
	void read_counter(
		uint num  //!< ???
		);

#ifndef WITHOUT_ARQ
	/// fetch read data resulting from read_cfg.
	void get_read_counter(
		std::vector<uint>& count_val,  //!< ???
		uint num  //!< ???
		);
#endif

};

}  // end of namespace facets
