#ifndef UPTIME_STRUCT_H
#define UPTIME_STRUCT_H

typedef enum {
	ALL_UP,
	IFACE_UP,
	IFACE_DOWN,
	CABO_UP,
	CABO_DOWN
} ACTION;

typedef struct _UPTIME {
	uint8_t action;
	char mac_address[12];
	uint64_t t_up;
	uint64_t t_iface_up;
	uint64_t t_cabo_up;
	uint64_t t_iface_down;
	uint64_t t_cabo_down;
} UPTIME;

#endif