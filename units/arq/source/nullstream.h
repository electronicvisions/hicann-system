// -----------------------------------------------------------
//
// author: Stefan Philipp
// date  : Jan 2009
//
// nullstream.h
//
// dummy stream to depress outputs and inputs
//
// -----------------------------------------------------------

#ifndef _NULLSTREAM_
#define _NULLSTREAM_

#include <iostream>

struct nullostream: std::ostream
{
	struct nullbuf: std::streambuf { int overflow(int c) { return traits_type::not_eof(c); } } m_sbuf;
  nullostream(): std::ios(&m_sbuf), std::ostream(&m_sbuf) {}
};

extern nullostream NULLOS;

#endif
