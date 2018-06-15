#pragma once

#include "logger.h"

#ifdef NCSIM
#define LOG4CXX_FATAL(logger, text)                                                                \
	do {                                                                                           \
		std::cout << "FATAL" << text << Logger::flush;                                             \
		std::stringstream msg;                                                                     \
		msg << text;                                                                               \
		throw std::runtime_error(msg.str());                                                       \
	} while (0)
#define LOG4CXX_ERROR(logger, text) std::cout << "ERROR" << text << Logger::flush;
#define LOG4CXX_WARN(logger, text) std::cout << "WARN" << text << Logger::flush;
#define LOG4CXX_WARN_BACKTRACE(logger, text) std::cout << "WARN" << text << Logger::flush;
#define LOG4CXX_INFO(logger, text) std::cout << "INFO" << text << Logger::flush;
#define LOG4CXX_DEBUG(logger, text) std::cout << "DEBUG" << text << Logger::flush;
#define LOG4CXX_TRACE(logger, text) std::cout << "TRACE" << text << Logger::flush;
#endif
