#pragma once
#include <iosfwd>

struct Tests2 {
	void start(std::string ip, std::string port);
	void stop();
};


#include <RCF/Idl.hpp>
RCF_BEGIN(I_Tests2, "I_Tests2")
	RCF_METHOD_V2(void, start, std::string, std::string)
	RCF_METHOD_V0(void, stop)
RCF_END(I_Tests2)
