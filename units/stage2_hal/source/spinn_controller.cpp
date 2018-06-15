// Company         :   tud                      
// Author          :   partzsch            
// E-Mail          :   <email>                	
//                    			
// Filename        :   spinn_controller.cpp
// Project Name    :   p_facets
// Subproject Name :   s_fpga            
// Description     :   <short description>            
//                    			
//------------------------------------------------------------------------


#include "spinn_controller.h"
#include "ethernet_software_if.h"
#include "logger.h"

#include<stdexcept>

#include<cstdlib>
#include<cstdio>
#include<cstring>

extern "C" {
#include<arpa/inet.h>
}

SpinnController::SpinnController(uint32_t set_target_ip, uint16_t set_config_port, uint16_t set_pulse_port)
	: target_ip(set_target_ip), config_port(set_config_port), pulse_port(set_pulse_port), routingbuf(1024,0)
{
	#pragma omp critical(EthIF)
	{
	config_if.reset(new EthernetSoftwareIF());
	pulse_if.reset(new EthernetSoftwareIF());
	config_if->init(config_port);
	pulse_if->init(pulse_port);
	}
}


uint32_t SpinnController::getTargetIP()
{
	return this->target_ip;
}


uint16_t SpinnController::getConfigPort()
{
	return this->config_port;
}


uint16_t SpinnController::getPulsePort()
{
	return this->pulse_port;
}


void SpinnController::clearRoutingEntries()
{
	this->routingbuf.clear();
	this->routingbuf.resize(1024,0x0);
}
		
void SpinnController::setRoutingEntry(unsigned int addr, unsigned int value)
{
	if (addr < 1024)
	{
		this->routingbuf[addr] = (addr<<14) | (value&0x3fff);
		//printf("Setting routing addr 0x%X to 0x%X\n",addr,value);
	}
	else
		printf("SpinnController::setRoutingEntry WARNING: address (%d) out of range.\n", addr);
}

bool SpinnController::writeRoutingMemory()
{
	throw std::runtime_error("FIXME: htonl()s missing");

	if (this->config_if == NULL)
	{
		printf("SpinnController::writeRoutingMemory WARNING: config pointer not set.\n");
		return false;
	}
	
	unsigned int entries_per_pck = 32;
	
	for (unsigned int nentry=0;nentry<1024;nentry += entries_per_pck)
	{
		uint32_t *p_ptr = &(this->routingbuf.front()) + nentry;
		this->config_if->sendUDP(this->target_ip, this->config_port, p_ptr, entries_per_pck*4);
		this->config_if->pauseEthernet(2.0); // wait some time to avoid buffer overflows in Ethernet
	}
	
	
	return true;
}


bool SpinnController::writePulseMultiplier(unsigned int pulse_multiplier)
{
	throw std::runtime_error("FIXME: htonl()s missing");

	uint32_t config_val = (1<<28) | (pulse_multiplier & 0x3ff);
	int rv = this->config_if->sendUDP(this->target_ip, this->config_port, &config_val, 4);
	return (sizeof(config_val)==rv);
}


bool SpinnController::writeDownSampleCount(unsigned int downsample_count)
{
	throw std::runtime_error("FIXME: htonl()s missing");

	if ( (downsample_count < 1) || (downsample_count > 1024) )
	{
		printf("Downsample count out of range: %d (should be 1..1024). Setting to 1.\n", downsample_count);
		downsample_count = 1;
	}

	uint32_t config_val = (2<<28) | ((downsample_count-1)&0x3ff);
	int rv = this->config_if->sendUDP(this->target_ip, this->config_port, &config_val, 4);
	return (sizeof(config_val)==rv);
}


bool SpinnController::writePulsePort(unsigned int pulse_port)
{
	throw std::runtime_error("FIXME: htonl()s missing");

	uint32_t config_val = (3<<28) | (pulse_port&0xffff);
	int rv= this->config_if->sendUDP(this->target_ip, this->config_port, &config_val, 4);
	return (sizeof(config_val)==rv);
}


bool SpinnController::writeOutputAddressConfiguration(bool single_hicann_en, unsigned int single_hicann_addr,
                                                      unsigned int addr_offset, bool input_addr_mode, unsigned int output_addr_mode)
{
	throw std::runtime_error("FIXME: htonl()s missing");

	uint32_t config_val = (4<<28);
	config_val |= addr_offset&0x3fff;
	config_val |= (single_hicann_addr & 0x1f)<<14;
	if (single_hicann_en)
		config_val |= (1<<19);
	
	config_val |= (output_addr_mode&0x3)<<20;
	
	if (input_addr_mode)
		config_val |= (1<<22);
	
	int rv = this->config_if->sendUDP(this->target_ip, this->config_port, &config_val, 4);
	return (sizeof(config_val)==rv);
}


bool SpinnController::writeSenderSettings(bool is_active, uint32_t target_ip, uint16_t target_port)
{
	throw std::runtime_error("FIXME: htonl()s missing");

	uint64_t config_val = target_ip | ((uint64_t)(target_port)<<32);
	if (is_active) config_val |= (uint64_t)(1)<<48;
	
	// add header - split data into chunks of 28bit, adding for each a 4bit header
	uint64_t send_val = (5<<28) | (config_val&0xfffffff);
	send_val = (send_val<<32) | (6<<28) | ((config_val>>28)&0xfffffff);
	
	int rv = this->config_if->sendUDP(this->target_ip, this->config_port, &send_val, 8);
	return (sizeof(send_val)==rv);
}


void SpinnController::addPulse(unsigned int addr)
{
	this->pulsebuf.push_back(addr);
}


bool SpinnController::sendPulses()
{
	return this->sendPulsesToPort(this->pulse_port);
}
		

bool SpinnController::sendPulsesToPort(unsigned int pport)
{
	throw std::runtime_error("FIXME: htonl()s missing");

	if (this->pulse_if == NULL)
	{
		printf("SpinnController::sendPulses WARNING: pulse pointer not set.\n");
		return false;
	}

	uint32_t *p_ptr = &(this->pulsebuf.front());
	this->pulse_if->sendUDP(this->target_ip, pport, p_ptr, this->pulsebuf.size()*4);

	this->pulsebuf.clear();

	return true;
}


int SpinnController::getReceivedPulse()
{
	if (this->pulse_if == NULL)
	{
		printf("SpinnController::getReceivedPulse WARNING: pulse pointer not set.\n");
		return false;
	}

	if ((this->recpulses.size() == 0) && (this->pulse_if->hasReceivedUDP())) // try to receive next pulse packet
    {
    	rx_entry_t rec_frame;
        this->pulse_if->receiveUDP(rec_frame);

		uint32_t const * data = rec_frame.word_access<uint32_t>();
		for (size_t i = 0; i < rec_frame.len<uint32_t>(); i++)
			recpulses.push_back(ntohl(data[i]));

		Logger& log(Logger::instance());
		log(Logger::DEBUG0) <<  "SpinnController::getReceivedPulse(): Received " << this->recpulses.size() << " Pulses" << Logger::flush;

    }

	int recpulse = -1;
    if (this->recpulses.size() > 0)
    {
		recpulse = this->recpulses.front();
        this->recpulses.pop_front();
	}

	return recpulse;
}

