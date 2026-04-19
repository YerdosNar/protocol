# CUSTOM PROTOCOL (OpenP2P):

- ## Packet structure:
    ```c
    typedef struct __attribute__((packed)) {
            uint16_t        length;
            uint32_t        conn_id;
            uint8_t         version;
            uint8_t         type;
            uint64_t        seq_id;
            uint32_t        timestamp;
    } header_t;
    ```
    - #### legnth:
        Max length variable can be up to 65355.\
        Real maximum length is 1450, to prevent fragmentation.
        ```c
        #define MAX_PCT_SZ      1450
        ```

    - #### type-byte:
        ```c
        typedef enum {
                HELLO           = 0x00, // Initial handshake
                BYE             = 0x01, // Graceful sessio termination
                KEEPALIVE       = 0x02, // NAT hole-punching maintenance
                ACK             = 0x03, // Confirms receipt; includes SACK bitmask and Timestamp echo

                OBSERVE_REQ     = 0x10, // STUN-like NAT/IP discovery via server.
                OBSERVE_RES     = 0x11, // Response of observation
                ROOM_CREATE     = 0x12, // Rendezvous server room coordination
                ROOM_RESULT     = 0x13, // Room creation result

                PEER_INFO       = 0x15, // Direct peer IP, Port, Public Key

                MSG             = 0x20, // Standard text message
                FILE_OFFER      = 0x31, // Name, Size, Total Chunks, SHA-256 hash
                FILE_CHUNK      = 0x32, // Binary file slice
                FILE_EOF        = 0x33, // End-of-file signal

                ERROR           = 0xFF  // System error code payload
        } type_t;
        ```
