//_____________________________________________
// Company      :	kip			      	
// Author       :	agruebl			
// E-Mail   	:	agruebl@kip.uni-heidelberg.de					
//								
// Date         :   03.04.2008				
// Last Change  :   13.04.2012			
// Module Name  :   --					
// Filename     :   s2_types.h
// Project Name	:   p_facets/s_hicann2				
// Description	:   global type definitions for facets system simulation
//								
//_____________________________________________


#ifndef _S2_TYPES_H_
#define _S2_TYPES_H_

#ifdef USE_SCTYPES

#ifdef NCSIM   // !!! without this macro the required DPI extensions won't be included !!!
#define NCSC_INCLUDE_TASK_CALLS
#endif  // NCSIM

#include "systemc.h"
#endif  // USE_SCTYPES

#ifndef USE_SCTYPES
extern "C" {
	#include <stdint.h>
	typedef uint64_t uint64;
}
#endif
typedef unsigned int uint;
typedef unsigned char byte_t;

#include "sim_def.h"

#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <cstdio>
// try to move the typedefs from these files to some common file at some time...
#include "packetlayer.h"
#include "arq.h"

namespace facets {

/// OCP data and address types
typedef uint64 OCPDataType;
typedef unsigned int OCPAddrType;

/// Data types for communication with control modules
typedef uint ci_write_t; // FIXME: bool baby!
typedef uint32_t ci_data_t;
typedef uint16_t ci_addr_t;

/// Global time which is held in DNC cycles, but has creation and getter interfaces to FPGA time and
/// nano seconds as well.
struct global_time {
	static global_time create_from_dnc_cycles(size_t const dnc_time);
	static global_time create_from_fpga_cycles(size_t const fpga_time);
	static global_time create_from_nano_sec(size_t const nano_sec);

	size_t get_dnc_time() const;
	size_t get_fpga_time() const;
	size_t get_nano_sec() const;

	global_time operator+(global_time const& inc) const;
	global_time& operator+=(global_time const& inc);
	global_time operator-(global_time const& sub) const;
	global_time& operator-=(global_time const& sub);
	bool operator<(global_time const& obj) const;
	bool operator<=(global_time const& obj) const;
	bool operator>(global_time const& obj) const;
	bool operator>=(global_time const& obj) const;
	bool operator==(global_time const& obj) const;
	bool operator!=(global_time const& obj) const;

private:
	global_time(size_t const dnc_time);
	size_t m_raw; //held in DNC cycles
	static const size_t dnc_cycles_per_fpga_cycle = 2;
	static const size_t nano_sec_per_dnc_cycle = 4;
};

///< type of data payload transported via communication medium between host and Stage 2 hardware.
enum ptype {p_empty=0,p_ci=1,p_event=2,p_layer2=3,p_dnc=4,p_fpga=5}; 
///< type of sideband data
enum sbtype {s_empty=0,s_dac=1,s_adc=2};
///< type of communication used in testmodes to be passed down to those modes
enum commodels {tcpip=0,ocpbus,pbtcpip,jtag,jtag_multi,jtag_full,jtag_2fpga,jtag_eth_fpga,jtag_eth_fpga_arq,jtag_wafer,jtag_wafer_fpga,jtag_eth,dryrun};

class TCommObj {
public:

	///< virtual methods for serialization and deserialization.
	///< must be implemented by derived classes
	virtual void serialize(unsigned char* buffer, bool verbose=false)=0; ///< give start pointer to char array
	virtual void deserialize(const unsigned char* buffer)=0; ///< give start pointer to char array

	virtual void clear(void)=0;
	
	virtual ~TCommObj(){};
};

/**
##########################################################################################
####           start section containing definitions corresponding to:                 ####
####          *  s_hicann1/units/hicann_top/hicann_base.svh                           ####
####          *  s_hicann1/units/hicann_top/hicann_pkg.svh                            ####
##########################################################################################
*/

//##########################################################################################
/// Content of HICANN control interface packet as delivered from/to hicannbus
// -> definition for NEW parts of software using the current ARQ protocol
class ci_payload : virtual public TCommObj , public Packet<ci_data_t> {
public:
	ci_write_t write;
	ci_addr_t addr;

	ci_payload() { clear(); };
	ci_payload(ci_write_t w, ci_addr_t a, ci_data_t d) : Packet<ci_data_t>(d), write(w), addr(a){};
	
	virtual ~ci_payload();

	// TCommObj functions
	inline virtual void clear(void){
		write    = 0;
		addr     = 0;
		data     = 0;
	}
	
	virtual void serialize(unsigned char* buffer, bool verbose=false);
	virtual void deserialize(const unsigned char* buffer);

	// Packet functions
	virtual void Display() { std::cout << this->data << std::endl; }
	virtual int  Size() {return 7; };

	// get packet object from read-in raw data
	ci_payload(unsigned char* p, int size);
	// get raw data/bytes from packet for network send operation
	virtual int GetRaw(unsigned char* p, int max_size);
};

//stream ci_payload_t structs
std::ostream & operator<<(std::ostream &o,const ci_payload & d);
std::ostringstream & operator<<(std::ostringstream &o,const ci_payload & d);


//##########################################################################################
/// Content of HICANN control interface packet as delivered from/to arq logic
// -> definition for NEW parts of software using the current ARQ protocol
class  ARQPacketStage2 : public ARQPacket<ci_payload>
{	
public:
				uint tagid;

				ARQPacketStage2() : ARQPacket<ci_payload>() {};
				ARQPacketStage2(ci_payload* p) : ARQPacket<ci_payload>(p) {};
				ARQPacketStage2(uint t, bool sv, int s, int a, ci_payload* p) : ARQPacket<ci_payload>(sv, s, a, p), tagid(t){};

				virtual void Display() { std::cout << this->data << std::endl; }
				virtual int  Size() {return 8; };

				// get packet object from read-in raw data
				ARQPacketStage2(unsigned char* p, int size);
				// get raw data/bytes from packet for network send operation
				virtual int  GetRaw(unsigned char* p, int max_size);

				ARQPacketStage2(uint64_t p);
				void GetRaw(uint64_t p);

};


//##########################################################################################
/// Content of HICANN control interface packet
class ci_packet_t : virtual public TCommObj {	
public:

#ifdef USE_SCTYPES
	sc_uint<CIP_TAGID_WIDTH> TagID;
	sc_uint<CIP_SEQV_WIDTH>  SeqValid;
	sc_uint<CIP_SEQ_WIDTH>   Seq;
	sc_uint<CIP_ACK_WIDTH>   Ack;
#else

	uint TagID;
	uint SeqValid;
	uint Seq;
	uint Ack;
#endif
	ci_write_t Write;
	ci_addr_t Addr;
	ci_data_t Data;

	ci_packet_t() { clear(); };

	ci_packet_t(
#ifdef USE_SCTYPES
		sc_uint<CIP_TAGID_WIDTH> g,
		sc_uint<CIP_SEQV_WIDTH>  v,
		sc_uint<CIP_SEQ_WIDTH>   s,
		sc_uint<CIP_ACK_WIDTH>   k,
		sc_uint<CIP_WRITE_WIDTH> w,
#else
		uint g,
		uint v,
		uint s,
		uint k,
		uint w,
#endif
		ci_addr_t a,
		ci_data_t d)
		: TagID(g), SeqValid(v), Seq(s), Ack(k), Write(w), Addr(a), Data(d){};

	explicit ci_packet_t(
#ifdef USE_SCTYPES
		sc_uint<CIP_PACKET_WIDTH> p
#else
		uint64 p
#endif
  ) {
		*this = p; // use = operator to convert
	}

	virtual ~ci_packet_t();

	inline virtual void clear(void){
		TagID    = 0;
		SeqValid = 0;
		Seq      = 0;
		Ack      = -1;
		Write    = 0;
		Addr     = 0;
		Data     = 0;
	};

	virtual void serialize(unsigned char* buffer, bool verbose=false);
	virtual void deserialize(const unsigned char* buffer);

	// convert ci_packet_t to uint64
	uint64_t to_uint64();

	// convert uint64 to ci_packet
	ci_packet_t & operator=(const uint64_t & d);

	bool operator==(const ci_packet_t & p) const {return (TagID==p.TagID && SeqValid==p.SeqValid && Seq==p.Seq && Ack==p.Ack && Write==p.Write && Addr==p.Addr && Data==p.Data);};
	bool operator!=(const ci_packet_t & p) const {return (TagID!=p.TagID || SeqValid!=p.SeqValid || Seq!=p.Seq || Ack!=p.Ack || Write!=p.Write || Addr!=p.Addr || Data!=p.Data);};
};


//stream ci_packet_t structs
std::ostream & operator<<(std::ostream &o,const ci_packet_t & d);
//std::ostringstream & operator<<(std::ostringstream &o,const ci_packet_t & d);


//##########################################################################################
// Content of DNC/FPGA configuration packets
class l2_packet_t
{
	public:
		// type of payload
		enum l2_type_t {EMPTY,DNC_ROUTING,DNC_DIRECTIONS,FPGA_ROUTING} type;
	
		// id of element (DNC or FPGA), max. 16bit (when serialized)
		unsigned int id;
		
		// sub-id of element (for DNC: HICANN channel), max. 16bit (when serialized)
		unsigned int sub_id;
	
		// std::vector of data elements
		std::vector<uint64> data;


		l2_packet_t() : type(EMPTY), id(0), sub_id(0)
		{}

		l2_packet_t(l2_type_t set_type, unsigned int set_id, unsigned int set_sub)
	    	       : type(set_type), id(set_id), sub_id(set_sub)
		{}
	
		l2_packet_t(l2_type_t set_type, unsigned int set_id, unsigned int set_sub, std::vector<uint64> set_data)
	    	       : type(set_type), id(set_id), sub_id(set_sub), data(set_data)
		{}

		l2_packet_t(l2_type_t set_type, unsigned int set_id, unsigned int set_sub, uint64 set_entry)
	    	       : type(set_type), id(set_id), sub_id(set_sub)
		{
			data.push_back(set_entry);
		}
	

	public:
		std::string serialize();
		bool deserialize(std::string instr);

	bool operator==(const l2_packet_t & p) const {
		return ( type   == p.type   &&
		         id     == p.id     &&
		         sub_id == p.sub_id &&
		         data   == p.data
				);
	}
};



/**
##########################################################################################
####             end section containing definitions corresponding to:  ...            ####
##########################################################################################
*/


//##########################################################################################
/** Layer1 pulse packet with debug information
    The minimum content of an L1 pulse packet is the destination bus id and the transferred L1 address. */
class l1_pulse {
public:
#ifdef USE_SCTYPES
	sc_uint<L1_TARGET_BUS_ID_WIDTH> bus_id;  /// target L1 bus_id before crossbar switch
	sc_uint<L1_ADDR_WIDTH> addr;             /// L1 address transferred in L1 packet.
#else
	uint bus_id;
	uint addr;
#endif
	
	uint  source_nrn;   /// logical (global no.) source neuron (origin of packet), for debugging

	l1_pulse():bus_id(0),addr(0){};
	~l1_pulse(){};
};


//##########################################################################################
/** Layer2 pulse packet with debug information.
    The minimum content of an L2 pulse packet is an address and a time stamp. If the packet is
		sent to HICANN (target event), the address denotes the  */
class l2_pulse : virtual public TCommObj {
public:
#ifdef USE_SCTYPES
	sc_uint<L2_LABEL_WIDTH> addr;  /// target/source address within HICANN. Address may be specified in more detail, later.
	sc_uint<L2_EVENT_WIDTH-L2_LABEL_WIDTH> time;  /// target/source time stamp
#else
	uint addr;
	uint time;
#endif
	
	l2_pulse() : addr(0),time(0){};
	l2_pulse(uint addr, uint time) : addr(addr),time(time){};
	virtual ~l2_pulse();
	
	virtual void serialize(unsigned char* buffer, bool verbose=false);
	virtual void deserialize(const unsigned char* buffer);

	inline virtual void clear(void) {
		addr = 0;
		time = 0;
	};
	
	// functions for sorting by time
	bool operator==(const l2_pulse &p) const {return ((this->addr==p.addr)&&(this->time==p.time)); }
	bool operator<(const l2_pulse &p) const
	{
		if (this->time == p.time)
			return (this->addr < p.addr);
			
		return (this->time < p.time);
	}
};



/**  #### encapsulation of the information exchange between host software and stage 2 hardware  ####

 two classes of data: IData:  anything that is transported to/via FPGA / DNC / HICANN
											SBData:	sideband data (data like transported parallel to the lvds bus between host and spikey)
 
*/




//##########################################################################################
///< Internal representation of the data exchanged with the Stage 2 hardware. This data structure
///< is to be transported over the communication medium (e.g. TCP/IP socket)
class IData : virtual public TCommObj {
private:
	ci_packet_t    _data;   ///< control interface data payload
	l2_pulse       _event;  ///< Layer 2 pulse event payload
	l2_packet_t    _l2data; ///< Layer 2 configuration data payload
	/* add other payload structs when implemented */
	
	uint           _addr;   /** defines target address depending on type: p_ci:    global HICANN address
	                                                                      p_event: global HICANN address
																																				p_dnc:   global DNC address
																																				p_fpga:  global FPGA address*/
	ptype _payload; ///< type of payload

public:

	/// Constructor and initializer functions
	IData();
	IData(ci_packet_t cip);
	IData(l2_pulse l2p);
	IData(l2_packet_t l2data);
	virtual ~IData();
	
	bool isEmpty() const {return payload()==p_empty;};

	///< access to private members
	uint addr(void)			const {return _addr;};
	uint & setAddr(void)			{return _addr;};
	void setAddr(const uint val) { _addr = val; }
	ptype payload(void)			const {return _payload;};
	ptype & setPayload(void)  		{return _payload;};
	void setPayload(const ptype val) { _payload = val; }

	///< access to ci command packet
	ci_packet_t cmd(void)			const {return _data;};
	ci_packet_t & setCmd(void)			  {_payload=p_ci; return _data;};
	bool isCI(void)								const {return payload()==p_ci;};

	///< access to layer2 configuration packet
	l2_packet_t l2data(void)			const {return _l2data;};
	l2_packet_t & setL2data(void)			  {_payload=p_layer2; return _l2data;};
	bool isL2data(void)								const {return payload()==p_layer2;};

	///< access to event daty payload
	l2_pulse event(void)			const {return _event;};
	l2_pulse & setEvent(void)				{_payload=p_event; return _event;};
	bool isEvent(void)				const {return payload()==p_event;};

	virtual void serialize(unsigned char* buffer, bool verbose=false);
	virtual void deserialize(const unsigned char* buffer);

	inline virtual void clear(void) {
		_payload=p_empty;
		_addr=0;
	};

	///< equal operator compares all member variables
	bool operator==(const IData & p) const {
	return (_data    == p._data   &&
	        _event   == p._event  &&
	        _l2data  == p._l2data &&
	        _addr    == p._addr   &&
	        _payload == p._payload);
	}
};


//##########################################################################################
///< sideband data -> implements external control voltages etc.
class SBData {
public:
	enum ADtype {vol, voh, vddbus, vdd25, vdd5, vdd11, vref_dac,
	             ivdd, ivdda, ivdd_dncif, ivdda_dncif, ivdd33, ivol,
	             ivoh, ivddbus, ivdd25, ivdd5, ivdd11, aout0, aout1,
	             adc0, adc1, adc2};

private:	
	sbtype _payload;
	ADtype _adtype;
	double _advalue;
	
	static const double adcvref;

public:
	SBData():_payload(s_empty){};	//default constructor
	SBData(ADtype type,double value):_payload(s_dac),_adtype(type),_advalue(value){};
	SBData(ADtype type):_payload(s_adc),_adtype(type),_advalue(0){};

	sbtype payload(void)			const {return _payload;};
	sbtype & setPayload(void)	  		{return _payload;};
	void setPayload(const sbtype val) { _payload = val; }

	//dacs
	bool isDac(void) const {return payload()==s_dac;};
	bool isAdc(void) const {return payload()==s_adc;};

	void setDac(ADtype type,double value){
		setPayload()=s_dac;
		_advalue=value;
		_adtype=type;
	};
			
	void setAdc(ADtype type){
		setPayload()=s_adc;
		_adtype=type;
	};
	
	void setADvalue(double v){_advalue=v;};
	double ADvalue() const {return _advalue;};

	ADtype getADtype() const {return _adtype;};
};



//stream idata structs
std::ostream & operator<<(std::ostream &o,const SBData & d);
//stream sbdata structs
std::ostream & operator<<(std::ostream &o,const IData & d);

}	// namespace facets

#endif // _S2_TYPES_H_

