#include "packet.h"
#include <string.h>
#include <sodium.h>
#include <time.h>
#include <arpa/inet.h>

// Cleanest way to handle 64-bit swap in 2026
static uint64_t htonll(uint64_t val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return __builtin_bswap64(val);
#else
        return val;
#endif
}

// BBR needs better than 1-second precision. Let's use milliseconds.
static uint32_t get_timestamp_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int packet_create_encrypt(
                const uint8_t  *key,
                uint32_t        conn_id,
                uint64_t        seq_id,
                uint8_t         type,
                uint8_t         version,
                uint8_t        *payload,
                uint16_t        p_len,
                uint8_t        *out_buf)
{
        header_t header;

        header.length    = 0; // Set later
        header.conn_id   = htonl(conn_id); // Don't forget to swap this!
        header.version   = version;
        header.type      = type;           // You missed this assignment
        header.seq_id    = htonll(seq_id);
        header.timestamp = htonl(get_timestamp_ms());

        uint16_t length = 6 + 14 + p_len + 16;
        if (length > MAX_PCT_SZ) return -1;
        header.length = htons(length);

        // NONCE: Use the Network Byte Order SeqID to ensure both sides agree
        uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES] = {0};
        memcpy(nonce, &header.seq_id, 8);

        // Prepare Plaintext (Metadata + Payload)
        // Using a stack buffer is fine for MTU-sized packets
        uint8_t plaintext[MAX_PCT_SZ];
        memcpy(plaintext, &header.version, 14); // version(1)+type(1)+seq(8)+ts(4) = 14B
        memcpy(plaintext + 14, payload, p_len);

        unsigned long long cipher_len;
        // Encrypt everything from 'version' onwards
        int res = crypto_aead_xchacha20poly1305_ietf_encrypt(
                        out_buf + 6, &cipher_len,
                        plaintext, 14 + p_len,
                        (uint8_t*)&header, 6, // Authenticate the Length and ConnID
                        NULL, nonce, key);

        if (res != 0) return -1;

        // Finally, copy the unencrypted 6-byte header to the front
        memcpy(out_buf, &header, 6);

        return (int)length;
}
