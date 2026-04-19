#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define MAC_SZ          16
#define MAX_PCT_SZ      1450

typedef enum {
        HELLO           = 0x00,
        BYE             = 0x01,
        KEEPALIVE       = 0x02,
        ACK             = 0x03,

        OBSERVE_REQ     = 0x10,
        OBSERVE_RES     = 0x11,
        ROOM_CREATE     = 0x12,
        ROOM_RESULT     = 0x13,

        PEER_INFO       = 0x15,

        MSG             = 0x20,
        FILE_OFFER      = 0x31,
        FILE_CHUNK      = 0x32,
        FILE_EOF        = 0x33,

        ERROR           = 0xFF
} type_t;

typedef struct __attribute__((packed)) {
        uint16_t        length;
        uint32_t        conn_id;
        uint8_t         version;
        uint8_t         type;
        uint64_t        seq_id;
        uint32_t        timestamp;
} header_t;

int packet_create_encrypt(
	const uint8_t 	*key,
        uint32_t        conn_id,
        uint64_t        seq_id,
        uint8_t         type,
        uint8_t         version,
        uint8_t         *payload,
        uint16_t        p_len,
        uint8_t         *out_buf);


#endif
