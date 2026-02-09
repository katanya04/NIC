#ifndef INCLUDED_ETHERNET_H
#define INCLUDED_ETHERNET_H

#include <stdint.h>

#include "hal.h"

#define NIC_DEFAULT_MTU  1500
#define ETH_MAC_LEN      6
#define ETH_HEADER_LEN   ETH_MAC_LEN * 2 + sizeof(uint16_t)

typedef enum ethertype {
    ethtype_IPv4 = 0x0800,
    ethtype_ARP = 0x0806,
    ethtype_IPv6 = 0x86DD,
    ethtype_ECTP = 0x9000,
} ethertype;

typedef struct ethernet_frame {
    uint8_t dest_mac[ETH_MAC_LEN];
    uint8_t src_mac[ETH_MAC_LEN];
    uint16_t ethertype;
    uint8_t payload[NIC_DEFAULT_MTU]; //Hardcoded mtu for testing!!!
} __attribute__((packed)) ethernet_frame;

unsigned int eth_build_frame(ethernet_frame *frame, const uint8_t *src_mac,
                             const uint8_t *dst_mac, const uint16_t type,
                             const void *data, const uint16_t payload_len);

void ethernet_receive(const void *data, unsigned int length, 
                      device_handle *dev, void *driver);

#endif