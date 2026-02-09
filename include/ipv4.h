#ifndef IPV4_H
#define IPV4_H

#include <stdint.h>
#include "hal.h"
#include "interface.h"

#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP  6
#define IPV4_PROTO_UDP  17
#define IPV4_HEADER_LEN 20
#define IPV4_VERSION    4

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed)) ipv4_hdr_t;

void ipv4_handler(uint8_t *packet, int len, device_handle *dev, nic_driver_t *drv);
int  ipv4_send(device_handle *dev, nic_driver_t *drv, uint32_t dst, uint8_t proto,
               const uint8_t *payload, uint16_t payload_len);

#endif