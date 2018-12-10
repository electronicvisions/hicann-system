//	Stefan Philipp
//	Electronic Vision(s) group, UNI Heidelberg
//
// 	link layer.h
//
//	This is the implementation of the link layer, which incoorperates the 
//	general ARQ protocol and is attached to the EthernetLayer.
//
//	purpose:
//		* encapculates ARQ protocol
//		* handles parallel packet i/o between lower layer and upper layer
//
//	implementation
//		* uses 2 distinct processes
//		* uses the threads form the "thread" library for the same
//		  code in simulation and binary
//
//	User access to the link layer (i.e. the ARQ protocol) is 
//	done via the dericed PacketLayer functions.
//
//	july 2009, created, StPh
//	have fun!
// ---------------------------------------------------------

#ifndef _LINNKLAYER_H_
#define _LINNKLAYER_H_

#include "common.h"

#include "packetlayer.h"
#include "arq.h"

template <class payload_t, class packet_t>
class LinkLayer : public PacketLayer<payload_t, packet_t>
{
private:
	static const uint SEQ_SIZE = 64;
	static const uint WND_SIZE = 16;

	// time (in "ticks" for simulation) until packet resent if no ack
	// defaulting to 2*WND_SIZE for minimum amount of resends during JTAG operation
	// (x2 because we tick twice in Exec())
	static const uint SEND_TIMEOUT = WND_SIZE*2;

	// time until ack is sent on packet receive
	// defaulting to "ack immediately" DO NOT CHANGE THIS - WOULD BREAK JTAG OPERATION!
	// This software linklayer ist only used for JTAG -> should be ok like this.
	static const uint RECV_TIMEOUT = 0;

	// max number of issued commands without receive
	static const size_t max_issued_cmds = WND_SIZE;

	uint const tagid; // link layer "address"
	uint hcnr;  // associated HICANN JTAG "address"

	// number of issued commands (cf. max_issued_cmds) since last receive
	size_t issued_cmds;

	virtual std::string ClassName() { return "LinkLayer"; };
	virtual std::ostream & dbg() { return std::cout << ClassName() << ": "; }
	virtual std::ostream & dbg(int level) { if (this->debug_level>=level) return dbg(); else return NULLOS; }

	// since the LinkLayer has to handle multiple access/threading
	// around the arq protocol, it is not derived from it but encapsulates it
public:

	ARQ<payload_t, packet_t > arq;

	// pointer to other LinkLayer
	// -> only for JTAG single threaded case to determine the need for an additional ack cycle after
	//    switching to another tag
	LinkLayer<payload_t, packet_t>* conc_ll;
	void setConcLl(LinkLayer<payload_t, packet_t>* ll){
		conc_ll = ll;
	};
	
	void Exec(bool callback=false)
	{
		if (HasSendData())
		{
			// check if other LinkLayer needs another Exec()
			while(conc_ll->HasSendData() && !callback)
				conc_ll->Exec(true);
			
			packet_t p;
			packet_t* pp = &p;
			GetSendData(&pp);	
			if ((this->lower) && this->lower->CanSend()){
				this->lower->Send(pp, hcnr);
				if (p.Payload().write) {
					// number of pending write requests
					issued_cmds++;
				} else {
					// non-write (i.e. read) requests enforce response handling below;
					// set to "above threshold"
					issued_cmds = max_issued_cmds + 1;
				}
			}
		}
		arq.Tick(); // no mutex needed
#ifdef NCSIM
		wait(1,SC_US);
		//wait(40,SC_NS);
#endif

		/*if( this->lower ) {
			packet_t p;
			while( !this->lower->HasReadData(tagid) ) {
				Receive(&p);
			}

			if (this->lower->Read(p,tagid)) 
			{
				Receive(&p);
			}
		}*/

		// when the number of issued non-write commands exceeds the threshold (or we issued a read command)
		// and we already received a response => call ARQ's receive method to perform protocol handling
		if ((issued_cmds >= max_issued_cmds) && this->lower && this->lower->HasReadData(hcnr, tagid))
		{
			issued_cmds = 0;
			packet_t p;
			if (this->lower->Read(p, hcnr, tagid)) 
			{
				Receive(&p);
			}
		}
		arq.Tick(); // no mutex needed
	};

	LinkLayer(uint h, uint t): 
		tagid(t),
		hcnr(h),
		issued_cmds(0),
		arq(t, SEQ_SIZE, WND_SIZE, SEND_TIMEOUT, RECV_TIMEOUT)
	{
		assert(max_issued_cmds <= WND_SIZE);
		
		Logger& log = Logger::instance("hicann-system.LinkLayer", 99, "");
		arq.SetDebugLevel(log.getLevel());
		conc_ll = NULL;
	};

	virtual ~LinkLayer() { }; 
	
	void Start()
	{
		dbg(0) << "ERROR: Start not valid/implemented in LinkLayer!" << std::endl;
	}

	uint tag() {return tagid;}
		

	// -----------------------------------------------
	// called internally, mutexes are functios
	// -----------------------------------------------

	virtual bool HasSendData() { 
		return arq.HasSendData();
	}

	virtual int  GetSendData(packet_t **p) { 
		return arq.GetSendData(p);
	}
	
	virtual bool CanReceive() { 
		return arq.CanReceive();
	}
	
	virtual int  Receive(packet_t *p) { 
		return arq.Receive(p);
	}; 

	// -----------------------------------------------
	// called from upper layer
	// -----------------------------------------------

	virtual bool CanSend() { 
		return arq.CanSend();
	}
		
	virtual int	 Send(payload_t *p) { 
		int tmp = arq.Send(p);
		Exec(); // JTAG specific!
		return tmp;
	} 
	
	virtual bool SendDone() { 
		return arq.SendDone();
	}

	virtual void SendFlush() { 
		while(!SendDone())
			Exec(); // JTAG specific!
	} // wait for incoming ack
	
	virtual bool HasReadData() { 
		return arq.HasReadData();
	}

	virtual int  Read(payload_t **p, bool block = false) 
	{ 
		while(true)
		{
			int _res = arq.Read(p, block);
			if (!block || (_res > 0))
				return _res;
		}
	}

	
	void SetDebugLevel(int l) 
	{ 
		PacketLayer<payload_t, packet_t >::SetDebugLevel(l);
		arq.SetDebugLevel(l); 
	}

};

#endif

