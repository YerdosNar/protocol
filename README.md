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
    - #### length:
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
        ```c
        uint64_t build_sack_bitmask(uint64_t ack_seq_id, uint64_t *out_of_order_array, int array_count) {
                uint64_t bitmask = 0;

                for (int i = 0; i < array_count; i++) {
                        uint64_t current_seq = out_of_order_array[i];

                        // Only map packets within the 64-packet window
                        if (current_seq > ack_seq_id && current_seq <= ack_seq_id + 64) {
                                uint64_t bit_offset = current_seq - ack_seq_id - 1;
                                bitmask |= (1ULL << bit_offset); // Set the bit to 1
                        }
                }

                return bitmask;
        }
        ```
        ```c
        void process_ack(uint64_t ack_seq_id, uint64_t sack_bitmask) {
                // 1. Everything <= ack_seq_id is safe to delete from memory.
                clear_buffer_up_to(ack_seq_id);

                // 2. Scan the next 64 packets in our "sent" queue
                for (int i = 0; i < 64; i++) {
                        uint64_t target_seq = ack_seq_id + 1 + i;

                        // Skip if this packet hasn't even been sent yet
                        if (!is_packet_in_flight(target_seq)) break;

                        // 3. Check the specific bit for this sequence ID
                        int is_received = (sack_bitmask & (1ULL << i)) != 0;

                        if (is_received) {
                                // Receiver got it out of order. Safe to delte.
                                mark_packet_as_received(target_seq);
                        } else {
                                // Bit is 0. Is it lost or just delayed?
                                // Fast Retransmit Rule: If we received a SACK for packets *after* this one,
                                // it's almost certainly lost.
                                trigger_fast_retransmit(target_seq);
                        }
                }
        }
        ```
    - ### 3. RTT and RTO calculation (Jacobson/Karels Algorithm):
        The Sender must calculate the Retransmission Timeout (RTO) dynamically based on network conditions. Hardcoded timeouts will cause either network congestion or stalled transfers.
        - #### Base RTT:
            When an ACK is received, calculate the raw Round-Trip Time:\
            $RTT = Time_{Current} - echo\_ts - recv\_delay$
        - #### Smoothed RTT (SRTT) and Variance (RTTVAR):
            Maintain a running average to smooth out suddent network jitter.\
            $\alpha = 0.125$ (Smoothing factor)\
            $\beta = 0.25$ (Variance factor)\
            Update the variables on every ACK:\
            $SRTT = (1 - \alpha) * SRTT + \alpha * RTT$\
            $RTTVAR = (1 - \beta) * RTTVAR + \beta * |SRTT - RTT|$
            ##### Final RTO:
            Calculate the timeout before resending a packet:\
            $RTO = SRTT + 4 * RTTVAR$
            >(Note: Clamp the RTO to a minimum of 10ms and maximum of 2000ms).
    - ### 4. Congestion Control (BBR Engine):
        If the transfer utilizes the BBR (Bottlenext Bandwidth and RTT) algorithm, the Sender must continuously calculate two primary metrics to pace the output.
        - #### 4.1. Delivery Rate:
            Calculated over a window of recent ACKs.\
            $Delivery\_Rate = \frac{\Delta Data\_ACKed}{\Delta Time}$
        - #### 4.2. RTprop (Round-Trip Propagatino Time):
            The minimum observed RTT over a moving time window (typically 10 seconds). This represents the physical limits of the network without buffer bloat.
        - #### 4.3. In-Flight Target (BDP - Bandwidth-Delay Product):
            The Sender must cap the amount of unacknowledged data currently on the wire to the Bandwidth-Delay Product.\
            $BDP = Delivery\_Rate \cdot RTprop$
    - ### 5. Fast Retransmit Logic:
        Do not wait for the RTO timer to expire if the SACK bitmask indicates a loss.
        - ##### Rule:
            If an ACK arrives and the `sack_bitmask` shows a 0 for speicific `Seq_ID`, and the `ack_seq_id` advances past it by at least 3 packets, assume the packet is lost.
        - ##### Action:
            Immediately re-qeueue the missing `Seq_ID` for transmission and reset its specific timout timer.


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
