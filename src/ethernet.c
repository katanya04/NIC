#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "ethernet.h"
#include "arp.h"
#include "ipv4.h"

unsigned int eth_build_frame(ethernet_frame *frame, const uint8_t *src_mac,
                             const uint8_t *dst_mac, const uint16_t type,
                             const void *data, const uint16_t payload_len) 
{
    memcpy(frame->dest_mac, dst_mac, 6);
    memcpy(frame->src_mac, src_mac, 6);
    frame->ethertype = htons(type);
    memcpy(frame->payload, data, payload_len);
    
    return ETH_HEADER_LEN + payload_len;
}

int ethernet_send(nic_driver_t *drv, nic_device_t *nic,
                  ethernet_frame *frame, unsigned int length)
{
    return drv->send_packet(nic, frame, length);
}

static inline int eth_is_for_me(const ethernet_frame *frame, const uint8_t *my_mac) {
    const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return (memcmp(frame->dest_mac, my_mac, 6) == 0) ||
           (memcmp(frame->dest_mac, broadcast, 6) == 0);
}

void ethernet_handle(const void *data, unsigned int length,
                     device_handle *dev, nic_driver_t *drv)
{
    if (length < ETH_HEADER_LEN) {
        printf("Ethernet: frame too short (%u bytes)\n", length);
        return;
    }
    
    const ethernet_frame *frame = (const ethernet_frame *)data;
    uint16_t ethertype = ntohs(frame->ethertype);
    unsigned int payload_len = length - ETH_HEADER_LEN;
    
    if (!eth_is_for_me(frame, dev->mac)) {
        printf("Ethernet: not for me\n");
        return;
    }
    
    printf("Ethernet: src=%02x:%02x:%02x:%02x:%02x:%02x type=0x%04x\n",
           frame->src_mac[0], frame->src_mac[1], frame->src_mac[2],
           frame->src_mac[3], frame->src_mac[4], frame->src_mac[5],
           ethertype);
    
    switch (ethertype) {
        case ethtype_ARP:
            arp_handle(frame->payload, payload_len, 
                      (uint8_t *)frame->src_mac, dev, drv);
            break;
        case ethtype_IPv4:
            /* Actualizar caché ARP con IP/MAC origen */
            /* Necesitamos parsear el header IP para obtener la IP */
            /* Por simplicidad, lo hacemos en ipv4_handler */
            ipv4_handler((uint8_t *)frame->payload, payload_len, dev, drv);
            break;
        default:
            printf("Ethernet: unknown ethertype 0x%04x\n", ethertype);
    }
}