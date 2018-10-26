// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   s2comm.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Fri Jun 20 08:12:24 2008                
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------

#ifndef _S2COMM_H_
#define _S2COMM_H_

#include "config.h"
#include "common.h"
#include "linklayer.h"
#include "CommAccess.h"
#include "logger.h"      // global logging class
#include "FPGAConnectionId.h" // IPv4 :)


namespace facets {

#define TXBUFLENGTH 1024
#define RXBUFLENGTH 1024

/** Stage2Comm encapsulates the transfer of information between host and the Stage 2 hardware/System Simulation
    The pure virtual functions Send and Receive must be implemented by the child modeuling a particular medium.
		
		Since communication can be initiated by the OCP masters on HICANN (or other chips in the system), some sort of
		server thread must be listening to this data. When using SystemC, this can inherently be solved using the SystemC
		scheduler (see e.g. child S2C_OcpBus using method rxpush). In case no other event scheduler is present, Pthreads
		are used. The server thread must be started from the constructor of a child (see child S2C_TcpIp using
		method rxlisten as server thread).
*/


class Stage2Comm : public PacketLayer<ARQPacketStage2 , ARQPacketStage2>
{
public:
	enum Commstate {ok,openfailed,eof,readfailed,writefailed,parametererror,readtimeout,writetimeout,notimplemented,initfailed};
	
	// store upper layer LinkLayers
	// written after object has been constructed
	std::vector<std::vector<LinkLayer<ci_payload,ARQPacketStage2> > > link_layers;
	
	// dequeues for storing read answers that are retrieved by the link layers
	// -> required to aloow transparency between software (also JTAG!) ARQ and hardware ARQ...
	std::vector<std::vector<std::queue<ci_payload> > > rxdata;
	
	// translate hicann number (hs channel) to jtag (chain) index and vice versa
	// TODO: rename DNC to HS to remove confusion
    size_t dnc2jtag(int const dnc_channel_nr);
    size_t jtag2dnc(int const jtag_chain_nr);

private:
	Commstate state;
	CommAccess const & access;
	int reset_fd;

protected:
	virtual std::ostream & dbg() { return std::cout << ClassName() << ": "; }
	virtual std::ostream & dbg(int level) { if (debug_level>=level) return dbg(); else return NULLOS; }
	int debug_level; // debug level
	bool k7fpga;

	// separate init function required because of dependency on JTAG object. Might not be valid when calling constructor
	void init_s2comm(void);

	// required for JTAG-ARQ operation
	IData readdata[8];
	ci_packet_t lastread[8];
	ci_payload ciptmp[8];
	bool  valid[8];

	// LUTs to assign jtag index to hs channel index and vice versa
	std::map<unsigned int, unsigned int> dnc2jtag_lut;
	std::map<unsigned int, unsigned int> jtag2dnc_lut;

	// "new" logger class
	Logger& log;

public:
	virtual std::string ClassName() { return "Stage2Comm"; };

	bool running_on_reticle;

	static const IData emptydata;     ///< used as default argument
	static const SBData emptysbdata;  ///< used as default argument


	static const int basedelay   =   1,  ///< Various delay values for particular commands. Default: one packet per buscycle
	                 synramdelay =  10,  ///< additional control interface delay to minimum delay
	                 l1ctrldelay =   1,
	                 cidelay     =   1;
	
	myjtag_full* jtag; // non-owning!
	
	Stage2Comm(CommAccess const & access, myjtag_full* j=NULL, bool on_reticle=false, bool use_k7fpga=false);
	virtual ~Stage2Comm();
	
	/// Issue command to JTAG HICANN no. hicann_nr
	virtual int issueCommand(uint hicann_nr, uint tagid, ci_payload *data, uint del=0);

	/// retreive data from JTAG HICANN no. hicann_nr
	virtual int recvData(uint hicann_nr, uint tagid, ci_payload *data);

	/// Send transmits data to target, buffering may be applied
	virtual Commstate Send(IData data=emptydata,uint del=0)=0;
	
	/// Receive fetches the topmost entry from the receive chain (rxdata) addressed by ctrladdr and lladdr
	virtual Commstate Receive(IData &data)=0; 	
	
	/// makes sure all data in the Send-chain is transmitted to the target (empties send buffer) 
	virtual Commstate Flush(void); 
	
	/// resets all queues, unsend and unreceived data is discarded (to make sure all data is transmitted, call rec until eof is reached
	virtual Commstate Clear(void);

	// initialize communication subsystem, set up all hardware in the comm chain
	virtual Commstate Init(std::bitset<8> hicann, bool silent=false, bool force_highspeed_init=false, bool return_on_error=false) = 0; //configures HICANN for jtag_multi communication
	// initialize communication subsystem, set up all hardware in the comm chain, only use a selection of highspeed links
	virtual Commstate Init(std::bitset<8> hicann, std::bitset<8> highspeed_hicann, bool silent=false, bool force_highspeed_init=false, bool return_on_error=false) = 0;
	virtual Commstate Init(int hicann_nr=-1, bool silent=false, bool force_highspeed_init=false, bool return_on_error=false) = 0; //can be used to add bus-dependent initializations
	
	/// set DAC
	virtual Commstate setDac(SBData);
	
	/// read ADC
	virtual Commstate readAdc(SBData&);

	Commstate getState(void){return state;};    ///< returns current communication state of the class
	void setState(Commstate s){state=s;};       ///< set communication state

	/// set/unset FPGA reset
	static void set_fpga_reset(unsigned int fpga_ip, bool enable_core, bool enable_fpgadnc, bool enable_ddr2onboard, bool enable_ddr2sodimm, bool enable_arq=true, uint reset_port=1801);

	Commstate reset_hicann_arq(std::bitset<8> hicann, bool release_reset = false);

	/// reprogram FGPA (requires correctly flashed design!)
	void reprog_fpga(unsigned int fpga_ip, uint reset_port=1801);

	bool is_k7fpga();


	// ----------------------------------
	// called from upper layer
	// ----------------------------------
	
	virtual bool CanSend(){
		return true;
	}
		
	virtual int Send(ARQPacketStage2 *p, uint c){ // PacketLayer function
// !!! try to get rid of type conversions !!!
		uint64_t data;
		p->GetRaw((unsigned char*)&data, 8);
		ci_packet_t tmp;
		tmp = data;
		IData packet(tmp);
		packet.setAddr() = c;
		if(Send(packet)==ok) return 8; // Stage2Comm function
		else return 0;
	}		
	 
	virtual bool HasReadData(uint c, uint t){
		if (valid[c]) return true;
		if (Receive(readdata[c]) != ok){
			log(Logger::ERROR) << "Stage2Comm:HasReadData: Receive failed!";
			return false; // get potential read data
		}
		if (readdata[c].cmd() != lastread[c] && readdata[c].cmd().TagID == t){
			log(Logger::DEBUG0) << "S2C_JtagPhys:HasReadData: read new packet from HICANN: " << std::hex << readdata;
			lastread[c] = readdata[c].cmd();
			valid[c] = true;
			return true;
		}
		log(Logger::DEBUG0) << "Stage2Comm:HasReadData: no new packet read from HICANN: " << std::hex << readdata;
		return false;
	}
	
	
	virtual int Read(ARQPacketStage2 &p, uint c, uint t, bool block = false)
	{
		UNUSED(block);
		if (!valid[c])
		{
			if (Receive(readdata[c]) != ok) return 0; // get potential read data
			valid[c] = (readdata[c].cmd() != lastread[c] && readdata[c].cmd().TagID == t);
			lastread[c] = readdata[c].cmd();
 			log(Logger::DEBUG0) << "Stage2Comm: Read packet " << std::hex << lastread[c] << ", valid=" << valid[c];
		}
		if (valid[c])
		{		
			ciptmp[c].write = lastread[c].Write;
			ciptmp[c].addr  = lastread[c].Addr;
			ciptmp[c].data  = lastread[c].Data;
			
			p.data     = ciptmp[c];
			p.tagid    = lastread[c].TagID;
			p.seqvalid = lastread[c].SeqValid;
			p.seq      = lastread[c].Seq;
			p.ack      = lastread[c].Ack;
			
			valid[c] = false;
 			log(Logger::DEBUG0) << "S2C_JtagPhys: Read packet " << std::hex << lastread[c];
			return sizeof(uint64_t);
		}
		return 0;
	}

	CommAccess const& getCommAccess() const {
		return access;
	}

	FPGAConnectionId const getConnId() const {
		return access.getFPGAConnectionId();
	}
};

}  // end of namespace facets

#endif //_S2COMM_H_
