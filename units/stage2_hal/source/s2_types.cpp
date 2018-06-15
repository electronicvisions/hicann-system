//_____________________________________________
// Company      :	kip			      	
// Author       :	agruebl			
// E-Mail   	:	<email>					
//								
// Date         :   03.04.2008				
// Last Change  :   03.04.2008			
// Module Name  :   --					
// Filename     :   types.cpp
// Project Name	:   p_facets/s_systemsim				
// Description	:   global type definitions for facets system simulation
//								
//_____________________________________________

#include <climits>
#include <string>
#include <iostream>
#include <stdint.h>

#include "s2_types.h"

const double facets::SBData::adcvref = 3.3;

using namespace std;
using namespace facets;

namespace facets {

//_____________________________________________
// global_time
//_____________________________________________

global_time global_time::create_from_dnc_cycles(size_t const dnc_time) {
	return global_time(dnc_time);
}

global_time global_time::create_from_fpga_cycles(size_t const fpga_time) {
	return global_time(fpga_time * dnc_cycles_per_fpga_cycle);
}

global_time global_time::create_from_nano_sec(size_t const nano_sec) {
	return global_time(nano_sec / nano_sec_per_dnc_cycle);
}

global_time::global_time(size_t const dnc_time)
	: m_raw(dnc_time)
{}

size_t global_time::get_dnc_time() const {
	return m_raw;
}

size_t global_time::get_fpga_time() const {
	return get_dnc_time() / dnc_cycles_per_fpga_cycle;
}

size_t global_time::get_nano_sec() const {
	return get_dnc_time() * nano_sec_per_dnc_cycle;
}

global_time global_time::operator+(global_time const& inc) const {
	global_time ret = *this;
	ret += inc;
	return ret;
}

global_time& global_time::operator+=(global_time const& inc) {
	m_raw += inc.m_raw;
	return *this;
}

global_time global_time::operator-(global_time const& sub) const {
	global_time ret = *this;
	ret -= sub;
	return ret;
}

global_time& global_time::operator-=(global_time const& sub) {
	m_raw -= sub.m_raw;
	return *this;
}

bool global_time::operator<(global_time const& obj) const {
	return (m_raw < obj.m_raw);
}

bool global_time::operator<=(global_time const& obj) const {
	return (m_raw <= obj.m_raw);
}

bool global_time::operator>(global_time const& obj) const {
	return (m_raw > obj.m_raw);
}

bool global_time::operator>=(global_time const& obj) const {
	return (m_raw >= obj.m_raw);
}

bool global_time::operator==(global_time const& obj) const {
	return (m_raw == obj.m_raw);
}

bool global_time::operator!=(global_time const& obj) const {
	return (m_raw != obj.m_raw);
}

//_____________________________________________
// ci_payload
//_____________________________________________

ci_payload::~ci_payload(){}

/// stream ci_payload_t structs
std::ostream & operator<<(std::ostream &o,const ci_payload & d){
	return o << "write: " << d.write << ", addr: " << d.addr << ", data: " << d.data;
}

/** Output string without special formatting. Is used for TCP/IP transfer */
std::ostringstream & operator<<(std::ostringstream &o,const ci_payload & d){
	o << d.write << d.addr << d.data;
	return o;
}

void ci_payload::serialize(unsigned char* buffer, bool verbose){
	uint size = (CIP_WRITE_WIDTH+CIP_ADDR_WIDTH+CIP_DATA_WIDTH+7)/8;
	uint pos  = 0;
	if(verbose){
		std::cout << "Verbose serialization of ci_payload_t not yet implemented!" << std::endl;
	}else{
	uint64 temp =
		  (((uint64)this->write    & (((uint64)1 << CIP_WRITE_WIDTH) -1)) << CIP_WRITE_POS)
		| (((uint64)this->addr     & (((uint64)1 << CIP_ADDR_WIDTH) -1))  << CIP_ADDR_POS)
		| (((uint64)this->data     & (((uint64)1 << CIP_DATA_WIDTH) -1))  << CIP_DATA_POS);

		for(uint i=0 ; i<size ; i++, temp>>=8, pos++) buffer[pos] = temp & 0xff;
	}
}

void ci_payload::deserialize(const unsigned char* buffer){
	this->clear();

	size_t const size = (CIP_WRITE_WIDTH+CIP_ADDR_WIDTH+CIP_DATA_WIDTH+7)/CHAR_BIT;
	uint64 temp=0;

	for (size_t i = 0; i < size; i++) {
		temp |= static_cast<uint64_t>(buffer[i]) << (i * CHAR_BIT);
	}

	(*this).write = (temp >> CIP_WRITE_POS) & (((uint64)1 << CIP_WRITE_WIDTH)-1);
	(*this).addr  = (temp >> CIP_ADDR_POS)  & (((uint64)1 << CIP_ADDR_WIDTH)-1);
	(*this).data  = (temp >> CIP_DATA_POS)  & (((uint64)1 << CIP_DATA_WIDTH)-1);
}

// get packet object from read-in raw data
ci_payload::ci_payload(unsigned char* p, int size){
	if(size == (int)(CIP_WRITE_WIDTH+CIP_ADDR_WIDTH+CIP_DATA_WIDTH+7)/8){
		this->deserialize(p);
	}	else {
		std::cout << "ci_payload:: Wrong size for deserialization!" << endl;
	}
}

// get raw data/bytes from packet for network send operation
int ci_payload::GetRaw(unsigned char* p, int max_size){
	if(max_size == (int)(CIP_WRITE_WIDTH+CIP_ADDR_WIDTH+CIP_DATA_WIDTH+7)/8){
		this->serialize(p, false);
		return (int)(CIP_WRITE_WIDTH+CIP_ADDR_WIDTH+CIP_DATA_WIDTH+7)/8;
	}	else return 0;
}


//_____________________________________________
// ci_packet_t
//_____________________________________________


///< ci_packet_t destructor
ci_packet_t::~ci_packet_t(){}

/// stream ci_packet_t structs
std::ostream & operator<<(std::ostream &o,const ci_packet_t & d){
	return o<<"TagID: "<<d.TagID<<", SeqValid: "<<d.SeqValid<<", Seq: "<<d.Seq<<", Ack: "<<d.Ack<<", Write: "<<d.Write<<", Addr: "<<d.Addr<<", Data: "<<d.Data;
}

/** Output string without special formatting. Is used for TCP/IP transfer */
/*
std::ostringstream & operator<<(std::ostringstream &o,const ci_packet_t & d){
	o << d.TagID << d.SeqValid << d.Seq << d.Ack << d.Write << d.Addr << d.Data;
	return o;
}
*/

uint64_t ci_packet_t::to_uint64(){
	uint64 left=
		  (((uint64)this->TagID    & (((uint64)1 << CIP_TAGID_WIDTH) -1)) << CIP_TAGID_POS)
		| (((uint64)this->SeqValid & (((uint64)1 << CIP_SEQV_WIDTH) -1))  << CIP_SEQV_POS)
	  | (((uint64)this->Seq      & (((uint64)1 << CIP_SEQ_WIDTH) -1))   << CIP_SEQ_POS)
		| (((uint64)this->Ack      & (((uint64)1 << CIP_ACK_WIDTH) -1))   << CIP_ACK_POS)
		| (((uint64)this->Write    & (((uint64)1 << CIP_WRITE_WIDTH) -1)) << CIP_WRITE_POS)
		| (((uint64)this->Addr     & (((uint64)1 << CIP_ADDR_WIDTH) -1))  << CIP_ADDR_POS)
		| (((uint64)this->Data     & (((uint64)1 << CIP_DATA_WIDTH) -1))  << CIP_DATA_POS);

	return left;
}

ci_packet_t & ci_packet_t::operator = (const uint64_t & d) {
	(*this).TagID    = (d >> CIP_TAGID_POS) & (((uint64)1 << CIP_TAGID_WIDTH)-1);
	(*this).SeqValid = (d >> CIP_SEQV_POS)  & (((uint64)1 << CIP_SEQV_WIDTH)-1);
	(*this).Seq      = (d >> CIP_SEQ_POS)   & (((uint64)1 << CIP_SEQ_WIDTH)-1);
	(*this).Ack      = (d >> CIP_ACK_POS)   & (((uint64)1 << CIP_ACK_WIDTH)-1);
	(*this).Write    = (d >> CIP_WRITE_POS) & (((uint64)1 << CIP_WRITE_WIDTH)-1);
	(*this).Addr     = (d >> CIP_ADDR_POS)  & (((uint64)1 << CIP_ADDR_WIDTH)-1);
	(*this).Data     = (d >> CIP_DATA_POS)  & (((uint64)1 << CIP_DATA_WIDTH)-1);
	return *this;
}

/** Serialize ci_packet_t. This could also be done by directly shifting the data
into an uint64, which is omitted for seamless extensibility.
The total size of the character array is stored at its beginning. The size of the size 
identifier is defined by */
void ci_packet_t::serialize(unsigned char* buffer, bool verbose){
	//ci_packet_t tmp = *this;
	uint size;
	uint i,pos=0;
	uint64 tu = to_uint64();

	if(verbose){
		size = CIP_SIZE_VERB;
		std::cout << "Verbose serialization of ci_packet_t not yet implemented!" << std::endl;
		//assert(false);
	}else{
		size = CIP_SIZE_REG;
		for(i=0;   i<IDATA_PAYL_SIZE;         i++, size>>= 8, pos++) buffer[pos] = size & 0xff;
		for(i=pos; i<(CIP_PACKET_WIDTH+7)/8;  i++, pos++)            buffer[pos] = tu & 0xff;
	}
}

/** Deserialize ci_packet_t. Information is extracted in the order specified in ci_packet_t::serialize() */
void ci_packet_t::deserialize(const unsigned char* buffer){
	this->clear();
	uint i,pos=0;
	uint size=0;
	uint64 tmp=0;

	for(i=0; i<IDATA_PAYL_SIZE; i++, pos++) size += ((buffer[pos] & 0xff) << (8*i));

	if(size > CIP_SIZE_REG){  // verbose stream
		std::cout << "Verbose deserialization of ci_packet_t not yet implemented!" << std::endl;
		//assert(false);
	}else{					// standard length
		for(i=0; i<(CIP_PACKET_WIDTH+7)/8;  i++, pos++) tmp += ((buffer[pos] & 0xff) << (8*i));

		*this = tmp;
	}
}


//_____________________________________________
// ARQPacketStage2
//_____________________________________________
int ARQPacketStage2::GetRaw(unsigned char* p, int max_size){
				int size = (CIP_SEQV_WIDTH+CIP_SEQ_WIDTH+CIP_ACK_WIDTH+CIP_WRITE_WIDTH+CIP_ADDR_WIDTH+CIP_DATA_WIDTH+7)/8;
				if(size>max_size || size > (int)sizeof(uint64))
								std::cout << "ARQPacketStage2::GetRaw: ERROR in byte sizes!" << std::endl;

				uint pos=0;
				uint64 tmp = 
								(((uint64)this->tagid      & (((uint64)1 << CIP_TAGID_WIDTH) -1)) << CIP_TAGID_POS)
								| (((uint64)this->seqvalid   & (((uint64)1 << CIP_SEQV_WIDTH) -1))  << CIP_SEQV_POS)
								| (((uint64)this->seq        & (((uint64)1 << CIP_SEQ_WIDTH) -1))   << CIP_SEQ_POS)
								| (((uint64)this->ack        & (((uint64)1 << CIP_ACK_WIDTH) -1))   << CIP_ACK_POS)
								| (((uint64)this->data.write & (((uint64)1 << CIP_WRITE_WIDTH) -1)) << CIP_WRITE_POS)
								| (((uint64)this->data.addr  & (((uint64)1 << CIP_ADDR_WIDTH) -1))  << CIP_ADDR_POS)
								| (((uint64)this->data.data  & (((uint64)1 << CIP_DATA_WIDTH) -1))  << CIP_DATA_POS);
				for(int i=0 ; i<size ; i++, tmp>>=8, pos++) p[pos] = tmp & 0xff;
				return size;
}

ARQPacketStage2::ARQPacketStage2(unsigned char* p, int size){
	int act_size = (CIP_SEQV_WIDTH+CIP_SEQ_WIDTH+CIP_ACK_WIDTH+CIP_WRITE_WIDTH+CIP_ADDR_WIDTH+CIP_DATA_WIDTH+7)/8;
	if(size!=act_size || size > (int)sizeof(uint64))
		std::cout << "ARQPacketStage2::ARQPacketStage2: ERROR in byte sizes!" << std::endl;

	uint64_t temp=0;
	for(int i=0; i<size; i++) temp += (uint64)(p[i]<<(i*8));
	GetRaw(temp);
}

ARQPacketStage2::ARQPacketStage2(uint64_t p){
	GetRaw(p);
}

void ARQPacketStage2::GetRaw(uint64_t p) {

	this->tagid      = (p >> CIP_TAGID_POS) & (((uint64)1 << CIP_TAGID_WIDTH)-1);
	this->seqvalid   = (p >> CIP_SEQV_POS)  & (((uint64)1 << CIP_SEQV_WIDTH)-1);
	this->seq        = (p >> CIP_SEQ_POS)   & (((uint64)1 << CIP_SEQ_WIDTH)-1);
	this->ack        = (p >> CIP_ACK_POS)   & (((uint64)1 << CIP_ACK_WIDTH)-1);
	this->data.write = (p >> CIP_WRITE_POS) & (((uint64)1 << CIP_WRITE_WIDTH)-1);
	this->data.addr  = (p >> CIP_ADDR_POS)  & (((uint64)1 << CIP_ADDR_WIDTH)-1);
	this->data.data  = (p >> CIP_DATA_POS)  & (((uint64)1 << CIP_DATA_WIDTH)-1);

}


//_____________________________________________
// l2_packet_t
//_____________________________________________


std::string l2_packet_t::serialize()
{
	std::string outstr;
	
	// write destination ID and sub-ID (make use of sequence number)
	outstr.push_back( (unsigned char)((this->id >> 8 ) % (1<<8)) );
	outstr.push_back( (unsigned char)((this->id) % (1<<8)) );
	outstr.push_back( (unsigned char)((this->sub_id >> 8 ) % (1<<8)) );
	outstr.push_back( (unsigned char)((this->sub_id) % (1<<8)) );


	// write packet header (type)
	unsigned int entry_width = 64; // width of one data entry
	if (this->type == DNC_ROUTING)
	{
		outstr.push_back((unsigned char)0x1d);
		outstr.push_back((unsigned char)0x1a);
		entry_width = 24;
	}
	else if (this->type == FPGA_ROUTING)
	{
		outstr.push_back((unsigned char)0x0c);
		outstr.push_back((unsigned char)0xaa);
		entry_width = 47;
	}
	else if (this->type == DNC_DIRECTIONS)
	{
		outstr.push_back((unsigned char)0x0e);
		outstr.push_back((unsigned char)0x55);
		entry_width = 64;
	}
	else // should not occur
	{
		outstr.push_back('?');
		outstr.push_back('?');
		entry_width = 64;
	}

	unsigned char curr_char = 0;
	unsigned int curr_char_pos = 0;
	
	for (unsigned int ndata = 0;ndata < this->data.size(); ++ndata )
	{
		//printf("l2_packet_t::serialize: curr. entry: %llx\n",this->data[ndata]);
	
		uint64 curr_data = this->data[ndata];
		unsigned int remaining_bits = entry_width;
		while (remaining_bits >= 8-curr_char_pos) // still a whole byte to fill
		{
			unsigned int add_bits = 8-curr_char_pos;
			curr_char |= ((curr_data>>(remaining_bits-add_bits))%(1<<add_bits)) << curr_char_pos;
			//printf("Full char: %x, add_bits: %d, remaining_bits: %d\n",curr_char,add_bits,remaining_bits);
			outstr.push_back(curr_char);
			curr_char = 0;
			curr_char_pos = 0;
			remaining_bits -= add_bits;
		}
		
		if (remaining_bits > 0)
		{
			curr_char = curr_data % remaining_bits;
			curr_char_pos = remaining_bits;
		}
		
	}
	
	if (curr_char_pos > 0)
		outstr.push_back(curr_char);
		

	printf("l2_packet_t::serialize: string length: %d, data entries: %d\n",(unsigned int)outstr.size(),(unsigned int)this->data.size());

	return outstr;
}


bool l2_packet_t::deserialize(std::string instr)
{
	if (instr.length() <= 6) // minimum: 4Bytes ID, 2Bytes Header
	{
		printf("l2_packet_t::deserialize: String to deserialize is too short (%d<%d). Ignore.\n",(unsigned int)instr.length(),6);
		return false;
	}


	// read destination ID
	this->id = 0;
	this->id += (unsigned int)((unsigned char)instr[0]) << 8;
	this->id += (unsigned int)((unsigned char)instr[1]);

	this->sub_id = 0;
	this->sub_id += (unsigned int)((unsigned char)instr[2]) << 8;
	this->sub_id += (unsigned int)((unsigned char)instr[3]);
	

	// read packet header (type)
	unsigned int entry_width = 64; // width of one data entry
	if ( ((unsigned char)instr[4] == 0x1d) && ((unsigned char)instr[5] == 0x1a) )
	{
		this->type = DNC_ROUTING;
		entry_width = 24;
	}
	else if ( ((unsigned char)instr[4] == 0x0c) && ((unsigned char)instr[5] == 0xaa) )
	{
		this->type = FPGA_ROUTING;
		entry_width = 47;
	}
	else if ( ((unsigned char)instr[4] == 0x0e) && ((unsigned char)instr[5] == 0x55) )
	{
		this->type = DNC_DIRECTIONS;
		entry_width = 64;
	}
	else // should not occur
	{
		this->type = EMPTY;
		printf("l2_packet_t::deserialize: Unknown packet header (%d %d). Ignore.\n",(unsigned int)((unsigned char)instr[4]),(unsigned int)((unsigned char)instr[5]) );
		return false;
	}

	this->data.clear();
	uint64 curr_val = 0;
	unsigned int remaining_bits = entry_width;
	
	for (unsigned int npos = 6;npos < instr.length(); ++npos )
	{
		unsigned char curr_char = (unsigned char)instr[npos];
	
		if (remaining_bits >= 8)
		{
			remaining_bits -= 8;
			curr_val += (uint64)(curr_char) << remaining_bits;
			
		}
		else
		{
			unsigned int rem_charbits = 8-remaining_bits; // remaining bits in current character
			curr_val += (uint64)(curr_char) >> rem_charbits;
			this->data.push_back(curr_val);
			
			// write entries that are entirely inside current byte (for entry_width < 8)
			while (rem_charbits >= entry_width)
			{
				rem_charbits -= entry_width;
				curr_val = ((uint64)(curr_char) >> rem_charbits) % (1<<entry_width);
				this->data.push_back(curr_val);
			}
			
			// fill curr_val with overlapping part
			remaining_bits = entry_width - rem_charbits;
			curr_val = ((uint64)(curr_char) % (1<<rem_charbits)) << remaining_bits;
		}
		
		
	}
	
	if (remaining_bits == 0) // no remainder -> this is the last entry; else: it is overlap -> do not add
		this->data.push_back(curr_val);

	return true;
}



//_____________________________________________
// IData
//_____________________________________________

///< IData constructors
IData::IData():_payload(p_empty){}
IData::IData(ci_packet_t cip):_data(cip){_payload=p_ci;}
IData::IData(l2_pulse l2p):_event(l2p){_payload=p_event;}
IData::IData(l2_packet_t l2data):_l2data(l2data){_payload=p_layer2;}

///< IData destructor
IData::~IData(){}

///< IData (de)serialization methods
void IData::serialize(unsigned char* buffer, bool verbose){
	ptype temptype = this->_payload;
	uint tempaddr = this->_addr;
	
	uint i,pos=0; // loop counter
	
	// serialize IData content common for all types:
	for(i=0; i<IDATA_ID_SIZE;   i++, temptype = (ptype)((int)temptype >> 8), pos++) buffer[pos] = (uint)(int)temptype & 0xff;
	for(i=0; i<IDATA_ADDR_SIZE; i++, tempaddr >>= 8, pos++)                         buffer[pos] = tempaddr & 0xff;
	
	switch(_payload){
		case p_empty: std::cout << "IData::serialize: cannot serialize empty IData!" << std::endl;
		              //assert(false);
						      break;

		case p_ci:    _data.serialize(buffer + pos, verbose);
									break;

		case p_event: _event.serialize(buffer + pos, verbose);
		              break;

		case p_layer2:   std::cout << "IData::serialize: serialization of Layer2 data not yet implemented! Use native serialize method." << std::endl;
		              break;

		case p_dnc:   std::cout << "IData::serialize: serialization of DNC data not yet implemented!" << std::endl;
		              break;

		case p_fpga:   std::cout << "IData::serialize: serialization of FPGA data not yet implemented!" << std::endl;
		              break;

		std::cout << "IData::serialize: illegal payload specified for serialization!" << std::endl;
	}
}

/** Deserialize IData. Information is extracted in the order specified in IData::serialize() */
void IData::deserialize(const unsigned char* buffer){
	this->clear();
	uint i,pos=0;
	uint temptype=0;
	
	// deserialize IData content common for all types:
	for(i=0; i<IDATA_ID_SIZE; i++, pos++)   temptype    += ((buffer[pos] & 0xff) << (8*i));
	for(i=0; i<IDATA_ADDR_SIZE; i++, pos++) this->_addr += ((buffer[pos] & 0xff) << (8*i));
	this->_payload = (ptype)temptype;

	//std::cout << "IData::deserialize: type = " << this->_payload << ", addr = " << this->_addr << " (p_ci = " << p_ci << ")" << std::endl;

	switch(_payload){
		case p_empty: std::cout << "IData::deserialize: cannot deserialize empty IData!" << std::endl;
		              //assert(false);
						      break;

		case p_ci:    _data.deserialize(buffer+pos);
						      break;

		case p_event: _event.deserialize(buffer+pos);
		              break;

		case p_layer2:   std::cout << "IData::deserialize: deserialization of Layer2 data not yet implemented!" << std::endl;
		              break;

		case p_dnc:   std::cout << "IData::deserialize: deserialization of DNC data not yet implemented!" << std::endl;
		              break;

		case p_fpga:   std::cout << "IData::deserialize: deserialization of FPGA data not yet implemented!" << std::endl;
		              break;

		std::cout << "IData::deserialize: illegal payload for deserialization detected!" << std::endl;
	}
}
	
/// stream IData structs
std::ostream & operator<<(std::ostream &o,const IData & d){
	switch (d.payload()) {
	case p_empty: return o<<"ID:--"<<" A: " << d.addr();

	case p_ci:    return o<<"ID:p_ci"<<" -A: " << d.addr() << " -D: " << d.cmd();

	case p_event: return o<<"ID:p_event"<<" -A: " << d.addr() << endl; // " -D: " << d.event(); stack fault here ...

	case p_layer2: return o<<"ID:p_layer2"<<" -A: " << d.addr() << " -D: not implemented!";

	default: return o<<"ID:illegal ";
	} 
}


///< l2_pulse destructor
l2_pulse::~l2_pulse(){}

///< l2_pulse (de)serialization methods
void l2_pulse::deserialize(const unsigned char* buffer){
	this->clear();
	uint i,pos=0;
	uint size=0;
	
	for(i=0; i<IDATA_PAYL_SIZE; i++, pos++) size += ((buffer[pos] & 0xff) << (8*i));

	if(size > L2P_SIZE_REG){  // verbose stream
		std::cout << "Verbose deserialization of l2_pulse not yet implemented!" << std::endl;
		//assert(false);
	}else{					// standard length
		for(i=0; i<(L2_LABEL_WIDTH+7)/8; i++, pos++)                this->addr += ((buffer[pos] & 0xff) << (8*i));
		for(i=0; i<(L2_EVENT_WIDTH-L2_LABEL_WIDTH+7)/8; i++, pos++) this->time += ((buffer[pos] & 0xff) << (8*i));
	}
}
	
///< l2_pulse (de)serialization methods
void l2_pulse::serialize(unsigned char* buffer, bool verbose){
	l2_pulse tmp = *this; ///< temp object - content will be destroyed
	uint size;
	uint i,pos=0;

	
	if(verbose){
		size = L2P_SIZE_VERB;
		std::cout << "Verbose serialization of l2_pulse not yet implemented!" << std::endl;
		//assert(false);
	}else{
		size = L2P_SIZE_REG;
		for(i=0; i<IDATA_PAYL_SIZE;                     i++, size     >>= 8, pos++) buffer[pos] = size & 0xff;
		for(i=0; i<(L2_LABEL_WIDTH+7)/8;                i++, tmp.addr >>= 8, pos++) buffer[pos] = tmp.addr & 0xff;
		for(i=0; i<(L2_EVENT_WIDTH-L2_LABEL_WIDTH+7)/8; i++, tmp.time >>= 8, pos++) buffer[pos] = tmp.time & 0xff;
	}
}


/// stream SBData structs
std::ostream & operator<<(std::ostream &o,const SBData & d){
	switch (d.payload()) {
	case s_empty: return o<<"ID:--"<<" ";
	default: return o<<"ID:illegal";
	} 
}

} // namespace facets

