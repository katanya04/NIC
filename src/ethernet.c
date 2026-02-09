#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "ethernet.h"
#include "arp.h"

unsigned int eth_build_frame(
    ethernet_frame *frame,
    const uint8_t *src_mac,
    const uint8_t *dst_mac,
    const uint16_t type,
    const void *data,
    const uint16_t payload_len) 
{
    memcpy(frame->dest_mac, dst_mac, 6);
    memcpy(frame->src_mac, src_mac, 6);
    frame->ethertype = htons(type);
    memcpy(frame->payload, data, payload_len);
    
    return ETH_HEADER_LEN + payload_len;
}

static inline int eth_is_for_me(const ethernet_frame *frame, const uint8_t *my_mac) {
    const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return (memcmp(frame->dest_mac, my_mac, 6) == 0) ||
           (memcmp(frame->dest_mac, broadcast, 6) == 0);
}

void ethernet_receive(const void *data, unsigned int length,
                      device_handle *dev, void *driver)
{
    if (length < ETH_HEADER_LEN) {
        printf("Ethernet: frame too short (%u bytes)\n", length);
        return;
    }
    
    const ethernet_frame *frame = (const ethernet_frame *)data;
    uint16_t ethertype = ntohs(frame->ethertype);
    unsigned int payload_len = length - ETH_HEADER_LEN;
    
    if (!eth_is_for_me(frame, dev->mac)) {
        printf("Ethernet: not for me (dst=%02x:%02x...)\n",
               frame->dest_mac[0], frame->dest_mac[1]);
        return;
    }
    
    printf("Ethernet: src=%02x:%02x:%02x:%02x:%02x:%02x type=0x%04x\n",
           frame->src_mac[0], frame->src_mac[1], frame->src_mac[2],
           frame->src_mac[3], frame->src_mac[4], frame->src_mac[5],
           ethertype);
    
    switch (ethertype) {
        case ethtype_ARP:
            arp_receive(frame->payload, payload_len, 
                       (uint8_t *)frame->src_mac, dev, driver);
            break;
        case ethtype_IPv4:
            printf("Ethernet: IPv4 not implemented yet\n");
            break;
        default:
            printf("Ethernet: unknown ethertype 0x%04x\n", ethertype);
    }
}