#define ERR(fmt, ...) do {                                                  \
    dprintf(STDERR_FILENO, fmt "\n" __VA_OPT__(,) __VA_ARGS__);             \
    return -1;                                                              \
} while(0)

#define PACKET_EXTEND(PACK, LEN) ((PACK)->length + sizeof(size_t) * 2 + sizeof(tagged_parameter_t) + LEN)

typedef uint8_t mac_t[6];

typedef struct __attribute__((__packed__)) RadioHeader {
  uint8_t revision;
  uint8_t pad;
  uint16_t length;
  uint64_t present_flags;
  uint8_t flags;
  uint8_t data_rate;
  uint16_t frequency;
  uint16_t channel_flags;
  int8_t signal;
  uint8_t pad_;
  uint16_t RX_flag;
  int8_t signal_;
  uint8_t antenna;
} radio_header_t;

typedef struct __attribute__((__packed__)) Beacon {
  unsigned version:2, type:2, subtype:4, flags:8;
  uint16_t duration;
  mac_t receiver;
  mac_t trasmitter;
  mac_t bss_id;
  unsigned fragment:4, sequence: 12;
  uint64_t timestamp;
  uint16_t interval;
  uint16_t capabilities;
} beacon_t;

typedef struct __attribute__((__packed__)) TaggedParameter {
  uint8_t number;
  uint8_t length;
  uint8_t data[];
} tagged_parameter_t;

typedef struct __attribute__((__packed__)) Packet {
  size_t length;
  size_t params_cur;
  radio_header_t radio_header;
  beacon_t beacon;
  uint8_t tag_params[];
} packet_t;

int random_mac(mac_t**);
int push_tagged_param(packet_t**, uint8_t, uint8_t, uint8_t*);
int send_packet(int,packet_t*);