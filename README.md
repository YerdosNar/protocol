# CUSTOM PROTOCOL (OpenP2P):

- ## PACKET STRUCTURE:
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
        Max length variable can be up to 65355. Authenticated via MAC.\
        Real maximum length is 1450, to prevent fragmentation.
        ```c
        #define MAX_PCT_SZ      1450
        ```

    - #### conn_id:
        Session identifier for key lookup. Authenticated via MAC.

    - #### version:
        Protocol version.

    - #### type:
        ```c
        typedef enum {
                HELLO           = 0x00, // Initial handshake
                BYE             = 0x01, // Graceful sessio termination
                KEEPALIVE       = 0x02, // NAT hole-punching maintenance
                ACK             = 0x03, // Confirms receipt;
                                        // includes SACK bitmask and Timestamp echo

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

    - #### seq_id:
        Monotonic sequence number. Used as the Nonce/IV.

    - #### timestamp:
        Sender's local high-precision timestamp for RTT calculation.


- ## RELIABILITY and TIMING MECHANICS:
    - ### 1. ACK Transmission Rules:
        The Receiver does not acknowledge every packet. ACKs are triggered under three specific conditions:
        - #### 1. Pairing:
            Send `ACK` for every 2 data packets received.
        - #### 2. Gap detection:
            If a packet arrives out of order (e.g., received `Seq_ID` 4, but missing 3), send an `ACK` **immidiately** with a SACK bitmask.
        - #### 3. Idle timeout:
            If a packet arrives and no subsequent packet arrives within 20ms, send an `ACK` to flush the state.
    - ### 2. ACK Payload Structure:
        ```c
        typedef struct __attribute__((packed)) {
                uint64_t ack_seq_id;    // Highest Seq_ID received in continuous order
                uint64_t sack_bitmask;  // 64-bit mask for packets received AFTER ack_seq_id
                uint32_t echo_ts;       // Copied from the timestamp of the acknowledged packet
                uint32_t recv_delay;    // Microseconds between receiving the packet and sending this ACK
                uint32_t app_window;    // Bytes available in receiver's application buffer
        } ack_payload_t;
        ```


- ## INTERACTION:

    - ### Rendezvous-Peer interaction:
        >Peer and Rendezvous for their first interaction will use hardcoded `Discover_key`.
        - #### 1. Peer sends `HELLO`:
            Packet, with size 68B\
            `[2B length][4B Conn_ID]`-unencrypted,\
            `[1B version][1B type][8B Seq_ID][4B timestamp][32B payload(public key)][16B MAC]`- encrypted.
        - #### 2. Rendezvous check:
            2.1) If length is not exactly 68 -> discard.\
            2.2) Decrypt the packet, check MAC. If MAC doesn't authenticate length -> discard.\
            2.3) If type is not `HELLO` -> discard.\
            Accept only after all tests passed.
        - #### 3. Rendezvous `HELLO`:
            Packet, with size 68B\
            `[2B length][4B Conn_ID]`-unencrypted,\
            `[1B version][1B type][8B Seq_ID][4B timestamp][32B payload(public key)][16B MAC]`- encrypted.
        - #### 4. E2EE established.
            Both derive the actual shared secret key from the public key they received from each other.
        - #### 5. Rendezvous sends `ROOM_CREATE`:
            5.1) Rendezvous sends packet with type `ROOM_CREATE`, with payload `"Are you [H]ost or [J]oin? [h/j]:"`\
            5.2) Peer answers with one letter **H** or **J**.\
            5.3) Rendezvous asks `"Enter HostRoom ID: "`.\
            5.4) Peer answers.\
            5.5) Rendezvous checks if there are rooms with that ID. Only unique IDs allowed.\
            5.6) Rendezvous asks `"Enter HostRoom PW: "`.\
            5.7) Peer answers.
        - #### 6. Rendezvous sends `ROOM_RESULT`:
            6.1) Rendezvous creates a room with given ID/PW.\
            6.2) Rendezvous sends Peer the credentials of the room.\
            6.3) And sets 3 minute timer. (within 3 minutes, joiner should join, else room is deleted).
