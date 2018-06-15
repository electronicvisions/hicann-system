#include "types-fpga.h"

/* define the structs */
#define PACKET(pid, name, size, fields)                             \
void test_##name() {                                                \
	COMPILE_TIME_ASSERT(sizeof(struct name##_packet) <= HMF_MTU);   \
	COMPILE_TIME_ASSERT(sizeof(struct name##_packet) == size);      \
}
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
/* some helpers */
char const* getnameof_packet( packet x ) {
	#define PACKET(pid, name, size, fields) \
	if (x == name##_packet) return #name "_packet";
	#include "types-fpga.def"
	return "";
}
/**********************************************************************/
