#ifndef INCLUDED_ARP_H
#define INCLUDED_ARP_H

#include <stdint.h>

#include "hal.h"
#include "interface.h"

#define ARP_REQUEST  0x0001
#define ARP_REPLY    0x0002

typedef struct arp_packet {
    uint16_t hw_type;       
    uint16_t proto_type;    
    uint8_t  hw_len;        
    uint8_t  proto_len;     
    uint16_t opcode;        
    uint8_t  sender_mac[6];
    uint8_t  sender_ip[4];
    uint8_t  target_mac[6];
    uint8_t  target_ip[4];
} __attribute__((packed)) arp_packet;

void arp_handle(const void *data, unsigned int length,
                const uint8_t *src_mac, device_handle *dev,
                nic_driver_t *drv);

unsigned int arp_build_request(void *buffer, device_handle *dev,
                               const uint8_t *target_ip);

unsigned int arp_build_reply(void *buffer, device_handle *dev,
                             const uint8_t *target_mac,
                             const uint8_t *target_ip);

int arp_lookup(const uint8_t *ip, uint8_t *out_mac);

void arp_cache_update(const uint8_t *ip, const uint8_t *mac);

#endif