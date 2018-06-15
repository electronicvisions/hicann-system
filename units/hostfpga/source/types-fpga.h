/* HAL to FPGA packet types */
#ifndef TYPES_FPGA_H
#define TYPES_FPGA_H
#ifdef __cplusplus
extern "C" {
#endif
#include "types-common.h"

/* Host-to-FPGA protocol constants */
#define HMF_MTU          1500



/**********************************************************************/
/* define the structs */
#define PACKET(pid, name, size, fields)                             \
typedef struct name##_packet {                                      \
	fields                                                          \
 } __attribute__((packed)) name##_packet_t;                         \
void test_##name();
#define UNION(name, length, fields)                                 \
union {                                                             \
	fields                                                          \
} name[length];
#define INNERPACKETS(name, length, fields)                          \
struct {                                                            \
	fields                                                          \
} __attribute__((packed)) name[length];
#define INNERPACKET(name, fields)                                   \
struct {                                                            \
	fields                                                          \
} __attribute__((packed)) name;
#define PID(name)              uint16_t name;
#define UINT16(name)           uint16_t name;
#ifndef __GNUC__
#error GCC-specific feature used: bit-field type != int
#endif
#define UINT16(name)           uint16_t name;
#define UINT16S(name, length)  uint16_t name[length];
#define UINT16P(name, bits)    __extension__ uint16_t name : bits;
#define UINT32(name)           uint32_t name;
#define UINT32P(name, bits)    uint32_t name : bits;
#define UINT64(name)           uint64_t name;
#define UINT64P(name, bits)    __extension__ uint64_t name : bits;
#define TIMESTAMP(name)        uint16_t name;
#define TIMESTAMPP(name, bits) __extension__ uint16_t name : bits;
#define LABEL(name)            uint16_t name;
#define LABELP(name, bits)     __extension__ uint16_t name : bits;
#define LABELS(name, length)   uint16_t name[length];
/* packet types defined for HOST TO FPGA communication, here: */
#include "types-fpga.def"
/**********************************************************************/


/**********************************************************************/
/* define packet types via enum */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic" // due to comma warning
typedef enum {
	#define PACKET(pid, name, size, fields) name##_packet = pid,
	#include "types-fpga.def"
} packet;
#pragma GCC diagnostic pop
/**********************************************************************/

/**********************************************************************/
/* some helpers */
char const* getnameof_packet( packet x );
/**********************************************************************/


struct tracepulsetest {
	uint16_t pid;
	uint16_t N;

	union {
		struct {
			__extension__ uint16_t type : 1; /* 0: event */
			__extension__ uint16_t timestamp : 15;
			__extension__ uint16_t pad0      :  2;
			__extension__ uint16_t label     : 14;
		} event;
		struct {
			__extension__ uint16_t type : 1; /* 1: overflow counter */
			__extension__ uint32_t N : 31;
		} overflow;
	} entry[2];
};


struct test {
	union {
		__extension__ uint16_t type : 2;
		struct {
			__extension__ uint16_t type : 1;
			__extension__ uint16_t N : 15;
		} overflow;
		struct {
			__extension__ uint16_t type0 : 1;
			__extension__ uint16_t t : 15;
			__extension__ uint16_t type1 : 2;
			__extension__ uint16_t l : 14;
		} pulse;
	} d;
};

#ifdef __cplusplus
namespace SF { class Archive; }
void serialize(SF::Archive &, fpga_pulse_packet_t&);
} // extern "C"
#endif
#endif // TYPES_FPGA_H
