///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate: 2011-12-15 09:37:54 +0100 (Thu, 15 Dec 2011) $
// $LastChangedRevision: 125 $
// $LastChangedBy: henker $

#ifndef __JTAG_LIB__
#define __JTAG_LIB__

#include "jtag_types.h"

///////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER)
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
#if defined(_BUILD_JTAG_DLL)
///////////////////////////////////////////////////////////
// nothing
///////////////////////////////////////////////////////////
#elif defined(_USE_JTAG_DLL)
///////////////////////////////////////////////////////////
#pragma comment(lib, "jtag_dll.lib")
///////////////////////////////////////////////////////////
#else
///////////////////////////////////////////////////////////
#pragma comment(lib, "jtag_lib.lib")
///////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////
/// contains jtag library and necessary types/enums.
/// header works standalone (e.g. with static/dynamic libraries).
namespace jtag_lib {
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// jtag lib.
/// interface allowing dynamic exchange of different jtag implementations.
struct JTAG_DLL_DECL jtag_hub
{
	typedef jtag_hub     self_type;
private:
	///////////////////////////////////////////////////////////////////////////
	void*            jtag_obj;
	jtag_interface_t jtag_jif;
	us_delay         jtag_udelay;
	size_t           jtag_p1;
	size_t           jtag_p2;
public:



	///////////////////////////////////////////////////////////////////////////
	jtag_hub(size_t p1=0, size_t p2=0);
	jtag_hub(const us_delay& delay, size_t p1=0, size_t p2=0);
	jtag_hub(jtag_interface_t jif, size_t p1=0, size_t p2=0);
	jtag_hub(jtag_interface_t jif, const us_delay& delay, size_t p1=0, size_t p2=0);

	///////////////////////////////////////////////////////////////////////////
	~jtag_hub()
	{
		this->jtag_close();
	}

	///////////////////////////////////////////////////////////////////////////
	jtag_hub(jtag_hub const& other)
		: jtag_obj(0), jtag_jif(other.jtag_jif), jtag_udelay(other.jtag_udelay), jtag_p1(other.jtag_p1), jtag_p2(other.jtag_p2)
	{
		this->open(other.jtag_jif, other.jtag_udelay, other.jtag_p1, other.jtag_p2);
	}
	jtag_hub const& operator=(jtag_hub const& other)
	{
		if( this != &other )
			this->open(other.jtag_jif, other.jtag_udelay, other.jtag_p1, other.jtag_p2);
		return *this;
	}

	///////////////////////////////////////////////////////////////////////////
	bool open();
	bool open(size_t p1, size_t p2=0);
	bool open(const us_delay& delay);
	bool open(const us_delay& delay, size_t p1, size_t p2=0);
	bool open(jtag_interface_t jif);
	bool open(jtag_interface_t jif, size_t p1, size_t p2=0);
	bool open(jtag_interface_t jif, const us_delay& delay);
	bool open(jtag_interface_t jif, const us_delay& delay, size_t p1, size_t p2=0);
	void close();

	// added function: return IP number for Ethernet-JTAG, else return 0
	unsigned int get_ip()
    {
    	if (this->jtag_jif == JTAG_ETH)
        	return this->jtag_p1;
        
        return 0;
    }

	///////////////////////////////////////////////////////////////////////////
	// log interface
	///////////////////////////////////////////////////////////////////////////
	static FILE*    set_logfile(FILE* newlogfile=NULL);
	static log_func set_logfile(log_func new_func_ptr=NULL);
	static int      logprintf(char const* fmt, ...);
	static int      logvprintf(char const* fmt, va_list argptr);

	///////////////////////////////////////////////////////////////////////////
	// control interface
	///////////////////////////////////////////////////////////////////////////

	/// initialize the device.
	bool jtag_init(size_t p1=0, size_t p2=0);
	bool jtag_init(const us_delay& delay, size_t p1=0, size_t p2=0);

	/// closes the device.
	bool jtag_close(void);

	/// return true if device is opened/initialized.
	bool jtag_valid(void) const;

	/// set the device speed for special mode.
	void jtag_speed(unsigned int khz);

	/// set cycles for tck generation.
	void set_waitcycles(unsigned char w32border=5, unsigned char wrd=0, unsigned char wwr=0);

	/// enable/disable bulk modus
	void jtag_bulkmode(bool enable=true);

	/// return current status
	void jtag_status(jtag_status_t& status) const;
	size_t jtag_rdbits() const;


	///////////////////////////////////////////////////////////////////////////
	// default interface
	///////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////
	void jtag_set(unsigned int tdi, unsigned int tms, unsigned int tck);
	void jtag_write(unsigned int tdi, unsigned int tms, jtag_transfer_type readtdo=TDO_DEFAULT);
	void jtag_write(unsigned int tdi, unsigned int tms, jtag_transfer_type readtdo, unsigned int cnt);
	void jtag_write(unsigned int tdi, unsigned int tms, unsigned int cnt, jtag_transfer_type readtdo=TDO_DEFAULT);
	bool jtag_get_tdo(void);
	bool jtag_store_tdo(void);

	///////////////////////////////////////////////////////////////////////////
	void jtag_execute();

	///////////////////////////////////////////////////////////////////////////
	size_t jtag_read(char &val, unsigned int bits);
	size_t jtag_read(unsigned char &val, unsigned int bits);
	size_t jtag_read(short &val, unsigned int bits);
	size_t jtag_read(unsigned short &val, unsigned int bits);
	size_t jtag_read(int &val, unsigned int bits);
	size_t jtag_read(unsigned int &val, unsigned int bits);
	size_t jtag_read(int64 &val, unsigned int bits);
	size_t jtag_read(uint64 &val, unsigned int bits);
	size_t jtag_read(std::vector<bool>&val, unsigned int bits);

	///////////////////////////////////////////////////////////////////////////
	void jtag_write_seq(unsigned char* val, size_t sz);
	void jtag_write_seq(unsigned char val1);
	void jtag_write_seq(unsigned char val1, unsigned char val2);
	void jtag_write_seq(unsigned char val1, unsigned char val2, unsigned char val3);
	void jtag_write_seq(unsigned char val1, unsigned char val2, unsigned char val3, unsigned char val4);
	void jtag_write_seq(unsigned char val1, unsigned char val2, unsigned char val3, unsigned char val4, unsigned char val5);


	///////////////////////////////////////////////////////////////////////////
	void set_delay(unsigned long udelay);
	void set_delay(us_delay const& udelay)  { this->set_delay(udelay.value); }
	unsigned long get_delay() const;
	void delay();
	void waitfor(unsigned long udelay);
	void waitfor(us_delay const& udelay)    { this->waitfor(udelay.value); }
};


///////////////////////////////////////////////////////////////////////////
} // end namespace jtag_lib
///////////////////////////////////////////////////////////////////////////

#endif//__JTAG_LIB__
