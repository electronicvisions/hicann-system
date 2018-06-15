/*
	Stefan Philipp, KIP, UNI Heidelberg

	arq.h

	Software-implementation of the VHDL-ARQ protocol

	* uses independend buffers and timers for send and receive
	* usable within simulator systemc or for real user-drivers
	  during operation.
	* implements only functionality
		- timers implemented via abstract timer objects, see there
		- timing has to be added externally via a tick() function
		  within systemc or kernel timers
		- no mutexes, has to be implemented from outside using systemc
		  threads or kernel mutexes

	Created: Feb 2009, Stefan Philipp
*/

#ifndef _ARQ_H_
#define _ARQ_H_

extern "C" {
#include <unistd.h>
}
#include <iostream>

#include <string>
#include <vector>

//#include "debug.h"
#include "nullstream.h"
#include "packetlayer.h"
#include "timer.h"

// -----------------------------------------------------------
// ARQ packet type
// -----------------------------------------------------------

template <class payload_t> class ARQPacket : public Packet<payload_t>
{
	template <class p_t>
	friend std::ostream & operator << (std::ostream &o, const ARQPacket<p_t> &p);

	public: // protected:
		bool      seqvalid;
		int       seq;
		//bool			ackvalid;
		int       ack;
	public:
		ARQPacket(): seqvalid(false),seq(0),/*ackvalid(false),*/ack(-1) {};
		ARQPacket(payload_t* p): seqvalid(false),seq(0),/*ackvalid(false),*/ack(-1) { this->Set(p);};
		ARQPacket(bool sv, int s, /*bool av,*/ int a, payload_t* p):
			seqvalid(sv), seq(s), /*ackvalid(av),*/ ack(a)
		{
				if (p) {
					this->Set(p);
				}
		}	
			
		bool SeqValid() { return seqvalid; };
		//bool AckValid() { return ackvalid; };
};

template <class payload_t> std::ostream & operator << (std::ostream &o, const ARQPacket<payload_t> &p) {
	return o << "(sv=" << p.seqvalid << ", seq=" << p.seq << /*", av=" << p.ackvalid <<*/ ", ack=" << p.ack << ", data=" << p.data << ")";
}


// -----------------------------------------------------------
// ARQ protocoll 
// ---------------------------------------------

template <class payload_t, class packet_t = ARQPacket<payload_t> > 
class ARQ : public PacketLayer<payload_t, packet_t>
{
	private:
		uint tagid;  // ($AG$): this ARQ's tag number

		uint SEQ_S;  // sequence, at least 2 * WND_S
		uint WND_S;  // buffer slots
		
		// Send Packets
		std::vector<payload_t> sendbuf; // WND_S;
		int	seqpos;
		int	wndpos;
		int sendpos;
		Timer SendTimer;

		// Receive Packets
		std::vector<payload_t> recbuf; // WND_S;
		std::vector<bool> recvalids; // WND_S;
		uint recwndpos;	// start of receive window, next packt to be send to upper layer
		uint recackpos;  // number of packet to ack
		Timer RecvTimer;

	public:
		virtual std::string ClassName() { return "ARQ"; };

		virtual void SetDebugLevel(int l) 
		{ 
			PacketLayer<payload_t, packet_t>::SetDebugLevel(l);
			SendTimer.SetDebugLevel(l);
			RecvTimer.SetDebugLevel(l);
		}

		//virtual std::ostream & dbg(int level) { return std::cout;/*return this->dbg(level); */}
			
		std::ostream & dbg() { return std::cout << ClassName() << ": "; }
		std::ostream & dbg(int level) { if (this->debug_level>=level) return dbg(); else return NULLOS; }

		// statistics
		int tx_timeout_num;
		int rx_timeout_num;
		int rx_pck_num;
		int tx_pck_num;
		int rx_drop_num;

		ARQ(uint t, uint seq_s, uint wnd_s, uint send_timeout, uint recv_timeout) :
			PacketLayer<payload_t, packet_t>(),
			tagid(t),
			SEQ_S(seq_s),
			WND_S(wnd_s),
			sendbuf(WND_S),
			SendTimer("SendTimer", send_timeout),
			recbuf(WND_S),
			recvalids(WND_S),
			RecvTimer("RecvTimer", recv_timeout)
		{
			//this->ClassName = "ARQ";
			this->debug_level=0;
			Reset();
		}

		virtual ~ARQ() {}

		// internal functions
	private:
		uint send_valids() { return (SEQ_S + seqpos -wndpos) % SEQ_S; }

	// access functions
	public:
		void Reset();
		void ClearStats();
		void PrintStats();
		void SetTimeouts(int txtimeout, int rxtimeout);

		// called from upper layer
		virtual bool CanSend();
		virtual int	 Send(payload_t *p); 
		virtual bool SendDone() { return send_valids() == 0; } ; // all send packets acked, send buffer empty
		virtual bool HasReadData();
		virtual int  Read(payload_t **p, bool block = false);
		
		// called from lower layer
		virtual bool HasSendData();
		virtual int  GetSendData(packet_t **p);
		virtual bool CanReceive();
		virtual int  Receive(packet_t *p); 

		// called periodically from external timeref
		virtual void Tick() { SendTimer.Tick(); RecvTimer.Tick(); };
};

template <class payload_t, class packet_t> 
void ARQ<payload_t, packet_t>::Reset()
{
	SendTimer.Stop();
	RecvTimer.Stop();
	seqpos = 0;
	wndpos = 0;
	sendpos = 0;
	recwndpos = 0;
	recackpos = SEQ_S -1;
	for (uint i=0; i<WND_S; i++) recvalids[i]=false;
	dbg(5) << "ARQ has been reset." << std::endl;
}


template <class payload_t, class packet_t> 
void ARQ<payload_t, packet_t>::ClearStats()
{
	tx_timeout_num= rx_timeout_num= rx_pck_num= tx_pck_num= rx_drop_num= 0;
}

template <class payload_t, class packet_t> 
void ARQ<payload_t, packet_t>::PrintStats()
{
	dbg(0) << "Statistics:" << std::dec << std::endl;
	dbg(0) << "TX packets:  " << tx_pck_num << std::endl;
	dbg(0) << "RX packets:  " << rx_pck_num << std::endl;
	dbg(0) << "RX drops:    " << rx_drop_num << std::endl;
	dbg(0) << "TX timeouts: " << tx_timeout_num << std::endl;
	dbg(0) << "RX timeouts: " << rx_timeout_num << std::endl;
}


template <class payload_t, class packet_t> 
void ARQ<payload_t, packet_t>::SetTimeouts(int txtimeout, int rxtimeout)
{
	SendTimer.SetTimeoutVal(txtimeout); 
	RecvTimer.SetTimeoutVal(rxtimeout);
}


// -----------------------------------------------------------
// transmit member functions of the ARQ protocol object
// -----------------------------------------------------------


template <class payload_t, class packet_t> 
bool ARQ<payload_t, packet_t>::CanSend()
{
  return send_valids() < WND_S;
}

// write data to send buffer
template <class payload_t, class packet_t> 
int ARQ<payload_t, packet_t>::Send(payload_t *p)
{
	dbg(5) << "Send(): s=" << seqpos << ", w=" << wndpos << ", payload=" << *p << std::endl;
	if (send_valids() == WND_S)
	{
		dbg(5) << "max reached" << std::endl;

		// return here or "wait blocking" until buffer becomes free
		return 0;
	}
	sendbuf[seqpos % WND_S] = *p; // store packet in buffer
	seqpos = (seqpos+1) % SEQ_S;

	dbg(5) << "Send: Buffer Content: " << std::endl;
	for(uint i=0; i<send_valids(); i++)
			dbg(9) << "Send: # " << (wndpos+i) % WND_S << ": " << sendbuf[(wndpos+i) % WND_S] << std::endl;

	return 1;
}


template <class payload_t, class packet_t> 
bool ARQ<payload_t, packet_t>::HasSendData()
{
  return SendTimer.Timeout() || RecvTimer.Timeout() || ((SEQ_S +seqpos -sendpos) % SEQ_S > 0);
}


// get next packet to be sent to lower network layer
template <class payload_t, class packet_t> 
int ARQ<payload_t, packet_t>::GetSendData(packet_t **p)
{
	dbg(5) << "GetSendData: seqp=" << seqpos << ", sendp=" << sendpos << ", wndp=" << wndpos << ", ackpos=" << recackpos << ", recwnd=" << recwndpos << std::endl;
	// NOTE
	// since the payload has to be repeatetly send, 
	// payload data is copied here and a new object is created
	// so outside logic has to delete the object later.

	if (SendTimer.Timeout())
	{
		SendTimer.Ack();
		tx_timeout_num++;
		if (seqpos == wndpos) {
			dbg(5) << "timeout for packet already sent, ignored" << std::endl;
			return 0;
		}
		dbg(5) << "re-send packet at pos " << wndpos << std::endl;
		**p = packet_t(tagid, true, wndpos, /*true,*/ recackpos, &sendbuf[wndpos % WND_S]);
		SendTimer.Continue();
	}
	else if ((SEQ_S +seqpos -sendpos) % SEQ_S > 0)
	{
		dbg(5) << "send packet at pos " << sendpos << std::endl;
		**p = packet_t(tagid, true, sendpos, /*true,*/ recackpos, &sendbuf[sendpos % WND_S]);
		sendpos = (sendpos+1) % SEQ_S;
		SendTimer.Continue();
	}
	else if (RecvTimer.Timeout())
	{
		dbg(5) << "send single ACK for pos " << recackpos << std::endl;
		**p = packet_t(tagid, false, 0, /*true,*/ recackpos, &sendbuf[0]);
		RecvTimer.Ack();
		rx_timeout_num++;
	}
	else return 0;

	dbg(5) << "send packet " << **p << std::endl;
	RecvTimer.Stop(); // ack is always send
	tx_pck_num++;
	
	dbg(9) << "end of arq:getsenddata" << std::endl;
	return 1;
}


// -----------------------------------------------------------
// receive member functions of the ARQ protocol object
// -----------------------------------------------------------


// checks if not all buffers are full, i.e. at least a single buffer
// space is free to receive a packet

template <class payload_t, class packet_t> 
bool ARQ<payload_t, packet_t>::CanReceive()
{
	for (uint s = 0; s <WND_S; s++)
		if (!recvalids[s])
			return true;
	return false;
}


template <class payload_t, class packet_t> 
int ARQ<payload_t, packet_t>::Receive(packet_t* p)
{
	dbg(5) << "Receive: " << *p << std::endl;
	dbg(5) << "recwndpos = " << recwndpos << " (buffer #" << recwndpos % WND_S << ")" << std::endl;

	// handle data part
	rx_pck_num++;
	if (p->SeqValid()) {
		uint seqofs = (SEQ_S + p->seq - recwndpos) % SEQ_S;
		if (seqofs >= WND_S) {
			dbg(5) << "Receive: DROPPING packet out of window" << std::endl;
			rx_drop_num++;
		}
		else if (recvalids[p->seq % WND_S]) {
			dbg(5) << "Receive: DROPPING packet already received" << std::endl;
			rx_drop_num++;
		}
		else {
			dbg(5) << "Receive: ACCEPT and store packet" << std::endl;

			// store packet
			recbuf[p->seq % WND_S] = p->Payload();
			recvalids[p->seq % WND_S] = true;

			// debug buffer content
			dbg(5) << "Receive: Buffer Content: " << std::endl;
			for(uint i=0;i<WND_S;i++)
				if (recvalids[(i + recwndpos) % WND_S]) 
					dbg(9) << "Receive: # " << (i+recwndpos) % WND_S << "(Seq "<<(i+recwndpos)%SEQ_S<<"): " 
						<< recbuf[(i+recwndpos) % WND_S] << std::endl;
			dbg(5)<<"reackpos="<<recackpos<<" valid+1= "<<recvalids[(recackpos+1) % WND_S] << std::endl;
			// increment ACK position
			while (recvalids[((recackpos+1)%SEQ_S) % WND_S]
				&& ((SEQ_S +recackpos+1 -recwndpos) % SEQ_S < WND_S))
					recackpos = (recackpos+1) % SEQ_S;

			// start timer
			RecvTimer.Continue();
		}
	}

	// handle ACK part
//	if (p->AckValid())
//	{
		dbg(5) << "Received ACK for pos " << p->ack << std::endl;
		uint ackofs = (SEQ_S + p->ack -wndpos) % SEQ_S; // buf ofs of received ack
		// bool ackdup = (ackofs == SEQ_S -1); // react on duplicate acks in non-duplex mode only
		uint ackleft = (SEQ_S + sendpos - wndpos) % SEQ_S; // number of outstanding packets to be ACKed

		dbg(5) << "Received: ackofs=" << ackofs << ", ackleft=" << ackleft << std::endl;

		if (ackofs < ackleft)
		{
			dbg(5) << "Received valid ACK, move send window SEQ by " << ackofs+1
					<< " to " << (wndpos+(ackofs+1)) % SEQ_S << std::endl;

			// update send buffer and window
			for(uint i=0; i<=ackofs; i++)
			{
				dbg(5) << "deleting stored packet: " << sendbuf[(wndpos+i) % WND_S] << std::endl;
				// delete sendbuf[(wndpos+i) % WND_S];
			}
			wndpos = (wndpos + ackofs +1) % SEQ_S;
			
			if (wndpos == sendpos)
			{
				dbg(5) << "Received last ACK, timer stopped" << std::endl;
				SendTimer.Stop();
			}
			else SendTimer.Start();
		}
//	}

	//cout << "..deleting received packet" << std::flush << std::endl;
	//delete p; // destruct ARQ packet
	dbg(5) << "End of ARQ receive" << std::endl;

	return 0;
}


template <class payload_t, class packet_t> 
bool ARQ<payload_t, packet_t>::HasReadData()
{
	return recvalids[recwndpos % WND_S];
}


template <class payload_t, class packet_t> 
int ARQ<payload_t, packet_t>::Read(payload_t **p, bool /*block*/)
{
	// block until data available
	while(!HasReadData()) {
		dbg(5) << "Read: wait for data ..." << std::endl;
		usleep(1); // interrupt thread execution
//		if (!HasReadData()) // debug
//			dbg(3) << "Read: spurious wakup without data, go again sleeping..." << std::endl;
//		return 0;
	}

	// transfer data
	**p = recbuf[recwndpos % WND_S];
	dbg(5) << "Read packet: " << **p << std::endl;
	recvalids[recwndpos % WND_S] = false;
	recwndpos = (recwndpos+1) % SEQ_S;
	dbg(5) << "Read: window pos incremented to " << recwndpos << std::endl;

	return 1;
}

#endif
