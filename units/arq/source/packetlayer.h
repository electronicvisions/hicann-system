/*
	Stefan Philipp, Feb 2009

	PacketLayer.h

	implements generic packet and network layer classes
*/
#ifndef _PACKETLAYER_H_
#define _PACKETLAYER_H_

#include <iostream>

extern "C" {
	#include "stdint.h"
}
typedef uint32_t uint;

#define unused(x) static_cast<void>((x))

//#include "debug.h"
#include "nullstream.h"


// -----------------------------------------------------------
// basic Packet Class
// -----------------------------------------------------------

template <class payload_t> class Packet
{
	template <class p_t>
	friend std::ostream & operator << (std::ostream &o, const Packet<p_t> &p);

	public: // protected:
		payload_t data;
	public:
		Packet():data(payload_t()){};
		Packet(payload_t p): data(p) {};
		virtual ~Packet() { /* if(data) delete data; */ };
			
		payload_t Payload() { return data; };
		virtual void Display() { std::cout << this << std::endl; }
		virtual void Set(payload_t* p) { data = *p; }; // required for ncsim
		virtual int  Size() {return 0; };
		
		// get packet object from read-in raw data
		Packet(unsigned char* p, int size) {};
		// get raw data/bytes from packet for network send operation
		virtual int  GetRaw(unsigned char*, int) { return 0; };
};

template <class payload_t> std::ostream & operator << (std::ostream &o, const Packet<payload_t> &p) {
	return o << "(" << p.data << ")";
}


// -----------------------------------------------------------
// basic Network Layer Class
// -----------------------------------------------------------

template <
	class payload_t = Packet<int>, 
	class packet_t = Packet<int> > 
class PacketLayer //: virtual public Debug
{
	protected:
		virtual std::ostream & dbg() { return std::cout << ClassName() << ": "; }
		virtual std::ostream & dbg(int level) { if (debug_level>=level) return dbg(); else return NULLOS; }
		int debug_level; // debug level

//	virtual std::ostream & dbg(int level) { return this->dbg(level); }
		PacketLayer<packet_t,packet_t> *lower;
		PacketLayer<payload_t,payload_t> *upper;

	public:
		virtual std::string ClassName() { return "PacketLayer"; };
		PacketLayer() :  //Debug("PacketLayer"),
			lower(NULL),upper(NULL) {};
		virtual ~PacketLayer(){};

		virtual void Bind(void *low, void *up); // Why void pointers? (PacketLayer<packet_t,packet_t>*, dito) should work?
		virtual void Bind(PacketLayer<packet_t,packet_t>* low);
		virtual void SetDebugLevel(int l) { debug_level = l; };

		
		// NOTE:
		// the can/has functions denote wether the send/read functions will block 
		// if send/read are called although, the blocking will not result in an error,
		// but a timeout/failure may be implemented by derived classes

		// called from upper layer
		virtual bool CanSend();
		virtual int	 Send(payload_t *p, uint c);
		virtual void SendFlush();
		 
		virtual bool HasReadData();
		virtual int  Read(payload_t **p, bool block = false);
		virtual int  Read(payload_t &p, bool block = false);
		
		virtual bool HasReadData(uint c, uint t);
		virtual int  Read(payload_t **p, uint c, uint t, bool block = false);
		virtual int  Read(payload_t &p, uint c, uint t, bool block = false);
		
		// called from lower layer
		virtual bool HasSendData();
		virtual int  GetSendData(packet_t **p);
		virtual bool CanReceive();
		virtual int  Receive(packet_t *p); 
};

template <class payload_t, class packet_t>
void PacketLayer<payload_t, packet_t>::Bind(void *low, void *up)
{
	lower = (PacketLayer<packet_t, packet_t>*)low;
	upper = (PacketLayer<payload_t, payload_t>*)up;
}

template <class payload_t, class packet_t>
void PacketLayer<payload_t, packet_t>::Bind(PacketLayer<packet_t,packet_t>* low)
{
	lower = low;
	upper = NULL;
}

// -------------------------------
// called from upper layer
// -------------------------------

template <class payload_t, class packet_t>
bool PacketLayer<payload_t, packet_t>::CanSend()
{
	if (lower) return lower->CanSend(); else return false;
}

template <class payload_t, class packet_t>
int PacketLayer<payload_t, packet_t>::Send(payload_t* /*p*/, uint /*c*/)
{
	//if (lower) return lower->Send(p); else return 0;
	return 0;
}

template <class payload_t, class packet_t>
void PacketLayer<payload_t, packet_t>::SendFlush()
{
	if (lower) lower->SendFlush();
}

template <class payload_t, class packet_t>
bool PacketLayer<payload_t, packet_t>::HasReadData(uint c, uint t)
{
	if (lower) return lower->HasReadData(c,t); else return false;
}

template <class payload_t, class packet_t>
int PacketLayer<payload_t, packet_t>::Read(payload_t**, uint, uint, bool)
{
	return 0;
}

template <class payload_t, class packet_t>
int PacketLayer<payload_t, packet_t>::Read(payload_t& p, uint, uint, bool)
{
	unused(p); // ECM: gccxml needs the parameter name
	return 0;
}

template <class payload_t, class packet_t>
bool PacketLayer<payload_t, packet_t>::HasReadData()
{
	if (lower) return lower->HasReadData(); else return false;
}

template <class payload_t, class packet_t>
int PacketLayer<payload_t, packet_t>::Read(payload_t**, bool)
{
	//if (lower) return lower->Read(p, block); else return 0;
	return 0;
}

template <class payload_t, class packet_t>
int PacketLayer<payload_t, packet_t>::Read(payload_t& p, bool)
{
	unused(p); // ECM: gccxml needs the parameter name
	//if (lower) return lower->Read(p, block); else return 0;
	return 0;
}

// -------------------------------
// called from lower layer
// -------------------------------

template <class payload_t, class packet_t>
bool PacketLayer<payload_t, packet_t>::HasSendData()
{
	if (upper) return upper->HasSendData(); else return false;
}

template <class payload_t, class packet_t>
int PacketLayer<payload_t, packet_t>::GetSendData(packet_t**)
{
	//if (upper) return upper->GetSendData(p); else return 0;
	return 0;
}

template <class payload_t, class packet_t>
bool PacketLayer<payload_t, packet_t>::CanReceive()
{
	if (upper) return upper->CanReceive(); else return false;
}

template <class payload_t, class packet_t> 
int PacketLayer<payload_t, packet_t>::Receive(packet_t*)
{
	//if (upper) return upper->Receive(p); else return 0;
	return 0;
}


#endif

