#ifndef JD_RCF_EXPERIMENT_H
#define JD_RCF_EXPERIMENT_H

#include "hicann_cfg.h"

#include <RCF/Idl.hpp>
#include <RCF/RcfServer.hpp>
#include <RCF/TcpEndpoint.hpp>
#include <RCF/FilterService.hpp>
#include <RCF/ZlibCompressionFilter.hpp>
#include <RCF/SessionObjectFactoryService.hpp>

int const server_port = 6667; // elaborate guess
size_t const msg_size = 1024*1024*100; // 100 MB

typedef std::map< l2_pulse_address_t, std::vector<double> > map_hw_spike_trains; // Note, this typedef is needed, as the MACRO RCF_METHOD_V1 doesn't like extra "commas".

/* some remote procedures */
RCF_BEGIN(I_Experiment, "I_Experiment")
	RCF_METHOD_V0(void, reset)
	RCF_METHOD_V0(void, stop)
	RCF_METHOD_R0(bool, init)
	RCF_METHOD_V1(void, config, hicann_cfg_t)
	RCF_METHOD_V1(void, config_layer2, pcs_cfg_t)
	RCF_METHOD_V1(void, set_input_spikes, map_hw_spike_trains )
	RCF_METHOD_V1(void, get_recorded_spikes, map_hw_spike_trains & )
	RCF_METHOD_R1(bool, run, double)
RCF_END(I_Experiment)


#endif // JD_RCF_EXPERIMENT_H
