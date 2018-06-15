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
 *  coordinate system conversion: power coordinates / wafer x,y coordinates etc,
 *  ressources management and so on...
 */

namespace facets {

//forward declarations, included later
class HicannCtrl;
class FPGAControl;
class DNCControl;

class ReticleControl: public Stage2Ctrl {
public:
	enum IDtype {sequentialnumber, powernumber, cartesiancoord, fpgaip, udpport, analogoutput, picnumber, all};

	struct cartesian_t{ //(x,y) cartesian coordinates type
		uint x, y;

		cartesian_t(uint _x=0, uint _y=0): x(_x), y(_y) {}

		friend std::ostream& operator<<(std::ostream& os, const cartesian_t& coord){
			os << "(" << coord.x << "," << coord.y << ")";
			return os;
		}

		bool operator==(const cartesian_t& comp) const { return (x==comp.x && y==comp.y); }
		// temporary? return !(*this==comp);
		bool operator!=(const cartesian_t& comp) const { return !(cartesian_t(x,y)==comp); }
		// why not without branching? return (x < comp.x) || ((x==comp.x) && (y < comp.y));
		bool operator<(const cartesian_t& comp)  const { if (x==comp.x) return (y<comp.y); else return (x<comp.x); }
		// return (*this != comp) && !(*this < comp);
		bool operator>(const cartesian_t& comp)  const { if (x==comp.x) return (y>comp.y); else return (x>comp.x); }
		// return (*this == comp) || (*this < comp);
		bool operator<=(const cartesian_t& comp) const { if (x==comp.x) return (y<=comp.y); else return (x<=comp.x); }
		// return (*this == comp) || (*this > comp);
		bool operator>=(const cartesian_t& comp) const { if (x==comp.x) return (y>=comp.y); else return (x>=comp.x); }
	};
	
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

	struct reticle {        ///info container for a reticle
		uint seq_number;    //sequential number: only existing reticles are in the list (left->right, top->bottom)
		uint pow_number;    //number of the power distribution unit (system PCB)
		cartesian_t coord;  //cartesian coordinates of the reticle (x,y)=(left->right, top->bottom)
		ip_t ip;            //IP address of the FPGA board
		uint port;          //UDP port for ETH<->JTAG communication
		uint aout;          //analog output number (ADC address) - to be implemented
		uint pic;           //PIC number for power controlling

		reticle(uint _seq_number=0, uint _pow_number=0, cartesian_t _coord=cartesian_t(0,0), ip_t _ip=ip_t(0,0,0,0), uint _port=0, uint _aout=0, uint _pic=0) :
			seq_number(_seq_number), pow_number(_pow_number), coord(_coord), ip(_ip), port(_port), aout(_aout), pic(_pic) {}

		friend std::ostream& operator<<(std::ostream& os, const reticle& r) { //output operator
			 os << "Sequential number: " << r.seq_number << ", \tPower number: " << r.pow_number <<
				", \tCartesian coordinates: " << r.coord << ", \tAout number: " << r.aout <<
				", \tFPGA IP: " << r.ip << ":" << r.port << ", \tPIC number: " << r.pic << std::endl;
			return os;
		}

		operator bool(){ //enables reticle to be evaluated as a bool type. True if any coordinate is !=0
			return (seq_number || pow_number || coord.x || coord.y || aout || ip.a || ip.b || ip.c || ip.d || port || pic);
		}
	};

private:
	/// common initialization stuff (called from all ctors)
	void init(bool on_wafer);
	virtual std::string ClassName() { return "ReticleControl"; };

	static std::multiset<uint> & instantiated();    //stores info if an instance is already created for a reticle

	//class for return-type overloading using cast operator: returns a coordinate in desired format
	class reticleIDclass{
	public:
		reticleIDclass(ReticleControl::reticle ret, ReticleControl::IDtype _id){
			id=_id;
			this_reticle=ret;

			switch (_id){
				case ReticleControl::sequentialnumber: {returnvalue_uint = this_reticle.seq_number;} break;
				case ReticleControl::powernumber:      {returnvalue_uint = this_reticle.pow_number;} break;
				case ReticleControl::udpport:          {returnvalue_uint = this_reticle.port;} break;
				case ReticleControl::analogoutput:     {returnvalue_uint = this_reticle.aout;} break;
				case ReticleControl::picnumber:        {returnvalue_uint = this_reticle.pic;} break;
				case ReticleControl::cartesiancoord:   {returnvalue_cart = this_reticle.coord;} break;
				case ReticleControl::fpgaip:           {returnvalue_ip   = this_reticle.ip;} break;
				case ReticleControl::all:              break;
			}
		}

		operator uint(){
			return returnvalue_uint;
		}

		operator ReticleControl::cartesian_t(){
			return returnvalue_cart;
		}

		operator ReticleControl::ip_t(){
			return returnvalue_ip;
		}

	private:
		ReticleControl::reticle this_reticle;
		ReticleControl::IDtype id;
		uint returnvalue_uint;
		ReticleControl::cartesian_t returnvalue_cart;
		ReticleControl::ip_t returnvalue_ip;
	};


	//internal attributes of this instance
	std::bitset<8> available_hicanns; //set of flags to indicate which hicanns are available (HS numbering!)
	std::bitset<8> used_hicanns; //set of flags to indicate HICANNs in use (JTAG numbering!)
	reticle reticle_info;    //reticle information in struct form

	uint x, y, p_number, s_number, jtag_port, aoutput, pic_num;
	FPGAConnectionId::IPv4::bytes_type fpga_ip;
	FPGAConnectionId::IPv4 pmu_ip;
	commodels model;
	bool gigabit_on, arq_mode, kintex;
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

	///constructor for the vertical setup: takes IP, number of HICANNs, whether highspeed should be on and if kintex or virtex fpga
	ReticleControl(bool gigabit_on, ip_t _ip, std::bitset<8> available_hicann, bool _arq_mode=true, bool _kintex=false);

	///constructor for single fpga on wafer: takes IP, and whether highspeed and arq should be on
	ReticleControl(size_t seq_number, size_t pow_number, ip_t _ip, uint16_t port, FPGAConnectionId::IPv4 const _pmu_ip, bool gigabit_on, bool _arq_mode=true, bool _kintex=false);
	// TODO: (shortterm) merge vertical RC + wafer RC for FPGA/PORT-based creation (add bool wafer parameter)

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

	///returns IDtype of this reticle in the right type, REUQUIRES EXPLICIT CASTING to the desired type!
	///e.g. cartesian_t example=reticleID(id)
	reticleIDclass reticleID(IDtype id) const;

	///prints the sequence numbers of already instantiated reticles
	static void printInstantiated();

	///returns true if reticle seq_num is already instantiated
	static bool isInstantiated(uint seq_num);

	///HICANN Initialization, DNC numbering, false if failed. Silent=true => less detailed info on the screen
	bool hicannInit(uint hicann_nr, bool silent=false, bool return_on_error=false);

	/// Initialization of several HICANNs, DNC numbering, false if failed. Silent=true => less detailed info on the screen
	/// Used from HALBE.
	/// by setting return_on_error to true, the initialization returns directly without a reset of the hicanns.
	/// In the other case, the initialization process is repeated for several times, including hicann resets,
	/// which entails the loss of the sync of the systime counters on the FPGA and the HICANNs
	bool hicannInit(std::bitset<8> hicann, bool silent=false, bool return_on_error=false);

	///label HICANNs of this reticle as used (DNC numbering!)
	void set_used_hicann(uint hicann_number, bool state);

	///unset used flags for HICANNs in this reticle
	std::bitset<8> get_used_hicanns();

	///get number of HICANNs on a reticle (in case vertical setup is used)
	uint8_t hicann_number();

	FPGAConnectionId::IPv4::bytes_type const get_fpga_ip() const;

	/// get the id of DNC with respect to its fpga, to which this reticle connects (range: 0-3)
	/// currently this is detected from the jtag port
	uint8_t get_fpga_dnc_channel_id() const;
};
}  //end of namespace facets
