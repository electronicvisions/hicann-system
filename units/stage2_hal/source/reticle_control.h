#pragma once
// Company         :   kip
// Author          :   Alexander Kononov
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                 :
// Filename        :   reticle_control.h
//                 :
// Create Date     :   Wed Apr  4 2012
// Last Change     :   Fri Jul  6 2012
// by              :   akononov
//------------------------------------------------------------------------

//the usual includes for a stage 2 control module
#include "s2_types.h"      //types for the stage2 software
#include "common.h"        //library includes
#include "s2ctrl.h"        //stage 2 control class
#include "fpga_control.h"  //fpga control class

//jtag communication classes
#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2c_jtagphys_2fpga_arq.h"

//multi index container and stuff for the "wafer LUT"
#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif
#define BOOST_ASSIGN_MAX_PARAMS 10

#include <boost/assign/list_of.hpp>
#include <boost/scoped_ptr.hpp>
#include <set>


/** Class for reticle management: power control, neighbor reticle communication,
 *  ressources management and so on...
 */

namespace facets {

//forward declarations, included later
class HicannCtrl;
class FPGAControl;
class DNCControl;

class ReticleControl: public Stage2Ctrl {
public:
	struct ip_t{ //IP type
		uint a, b, c, d;

		ip_t(uint _a=0, uint _b=0, uint _c=0, uint _d=0): a(_a), b(_b), c(_c), d(_d) {}

		friend std::ostream& operator<<(std::ostream& os, const ip_t& coord){
			os << coord.a << "." << coord.b << "." << coord.c << "." << coord.d;
			return os;
		}

		bool operator==(const ip_t& comp) const { return (a==comp.a && b==comp.b && c==comp.c && d==comp.d); }

		bool operator!=(const ip_t& comp) const { return !(ip_t(a,b,c,d)==comp); }

		bool operator<(const ip_t& comp) const {
			if (a==comp.a)
				if (b==comp.b)
					if (c==comp.c) return (d<comp.d);
					else return (c<comp.c);
				else return (b<comp.b);
			else return (a<comp.a);
		}

		bool operator>(const ip_t& comp) const {
			if (a==comp.a)
				if (b==comp.b)
					if (c==comp.c) return (d>comp.d);
					else return (c>comp.c);
				else return (b>comp.b);
			else return (a>comp.a);
		}

		bool operator<=(const ip_t& comp) const {
			if (a==comp.a)
				if (b==comp.b)
					if (c==comp.c) return (d<=comp.d);
					else return (c<=comp.c);
				else return (b<=comp.b);
			else return (a<=comp.a);
		}

		bool operator>=(const ip_t& comp) const {
			if (a==comp.a)
				if (b==comp.b)
					if (c==comp.c) return (d>=comp.d);
					else return (c>=comp.c);
				else return (b>=comp.b);
			else return (a>=comp.a);
		}
	};

private:
	/// common initialization stuff (called from all ctors)
	void init(bool on_wafer);
	virtual std::string ClassName() { return "ReticleControl"; };

	static std::multiset<uint> & instantiated();    //stores info if an instance is already created for a reticle

	//internal attributes of this instance
	std::bitset<8> physically_available_hicanns; // set of flags to indicate which hicanns are
	                                             // available (HS numbering!)
	std::bitset<8> used_hicanns; //set of flags to indicate HICANNs in use (JTAG numbering!)
	std::bitset<8> highspeed_hicanns; //set of flags to indicate which hicanns are available via high-speed (HS numbering!)

	uint s_number, jtag_port;
	FPGAConnectionId::IPv4::bytes_type fpga_ip;
	FPGAConnectionId::IPv4 pmu_ip;
	commodels model;
	bool on_wafer, arq_mode, kintex;
	boost::scoped_ptr<CommAccess> access;

public:
	//communication objects
	boost::scoped_ptr<myjtag_full>       jtag;
	boost::scoped_ptr<S2C_JtagPhys>      jtag_p;
	boost::scoped_ptr<S2C_JtagPhys2Fpga> jtag_p2f;
	Stage2Comm * comm;

	//child control modules of this reticle
	boost::scoped_ptr<FPGAControl> fc;
	boost::scoped_ptr<DNCControl>  dc;
	boost::scoped_ptr<HicannCtrl>  hicann[8]; //array of max. 8 HICANN objects, JTAG Numbering

	ReticleControl(
	    size_t seq_number,
	    ip_t _ip,
	    uint16_t port,
	    FPGAConnectionId::IPv4 const _pmu_ip,
	    std::bitset<8> physically_available_hicanns,
	    std::bitset<8> highspeed_hicanns,
	    bool on_wafer,
	    bool _arq_mode = true,
	    bool _kintex = true);

	///destructor
	~ReticleControl();

	///switching all reticle voltages on and off (uses remote procedure calls to control PICs)
	///TODO: implement RCF to control PICs should return true only if the power went on and false if the power went off.
	///on error the return value should be opposite of the desired!
	void switchVerticalPower(bool state); ///version for vertical setup

	///sets Vol and Voh on the vertical setup (needed for L1 testing)
	void setVol(float vol);
	void setVoh(float voh);

	///prints out attributes of this instance
	void printThisReticle();

	///prints the sequence numbers of already instantiated reticles
	static void printInstantiated();

	///returns true if reticle seq_num is already instantiated
	static bool isInstantiated(uint seq_num);

	///HICANN Initialization, DNC numbering, false if failed. Silent=true => less detailed info on the screen
	bool hicannInit(uint hicann_nr, bool silent=false, bool return_on_error=false);

	/// Initialization of several HICANNs, DNC numbering, false if failed. Silent=true => less detailed info on the screen
	/// Used from HALBE.
	/// @hicann denotes the set of hicanns available in the JTAG chain.
	/// @highspeed_hicann denotes the set of highspeed links to initialize (obv. less or equal than hicann).
	/// By setting return_on_error to true, the initialization returns directly without a reset of the hicanns.
	/// In the other case, the initialization process is repeated for several times, including hicann resets,
	/// which entails the loss of the sync of the systime counters on the FPGA and the HICANNs
	bool hicannInit(std::bitset<8> hicann, std::bitset<8> highspeed_hicann, bool silent=false, bool return_on_error=false);

	///label HICANNs of this reticle as used (DNC numbering!)
	void set_used_hicann(uint hicann_number, bool state);

	///unset used flags for HICANNs in this reticle
	std::bitset<8> get_used_hicanns();

	///get number of HICANNs on a reticle (in case vertical setup is used)
	uint8_t hicann_number();

	FPGAConnectionId::IPv4::bytes_type const get_fpga_ip() const;
};
}  //end of namespace facets
