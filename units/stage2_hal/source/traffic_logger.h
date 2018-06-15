#include "time.h"
#include <sys/time.h>
#include "logger.h"      // global logging class

class CtrlModTrafficLogger {

	size_t last_update;
	size_t data_sum;
	size_t const traffic_res = 10e3; //in us
	log4cxx::LoggerPtr perflogger = log4cxx::Logger::getLogger("hicann-system.CtrlModulePerf");
	size_t get_time_us(void) {
		struct timeval time;
		gettimeofday(&time, NULL);
		return time.tv_sec * 1e6 + time.tv_usec;
	}

public:

	CtrlModTrafficLogger() :
		last_update(get_time_us()),
		data_sum(0)
	{}

	~CtrlModTrafficLogger() {
		update(0, true);
	}

	void update(size_t data_size, bool force_update = false) {
		size_t now = get_time_us();

		#pragma omp critical(logger)
		{
		if (now - last_update > traffic_res || force_update) {
			LOG4CXX_TRACE(perflogger, "\t" << last_update << "\t" << data_sum);
			last_update = now;
			data_sum = data_size;
		} else {
			data_sum += data_size;
		}
		}
	}
};
