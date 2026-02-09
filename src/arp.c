#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "arp.h"
#include "ethernet.h"

static uint8_t cache_ip[4] = {0};
static uint8_t cache_mac[6] = {0};
static int cache_valid = 0;

int arp_lookup(const uint8_t *ip, uint8_t *out_mac)
{
    if (cache_valid && memcmp(ip, cache_ip, 4) == 0) {
        memcpy(out_mac, cache_mac, 6);
        return 0;  /* Éxito */
    }
    return -1;  /* No encontrado */
}

void arp_cache_update(const uint8_t *ip, const uint8_t *mac)
{
    memcpy(cache_ip, ip, 4);
    memcpy(cache_mac, mac, 6);
    cache_valid = 1;
    printf("ARP: cached %d.%d.%d.%d -> %02x:%02x:%02x:%02x:%02x:%02x\n",
           ip[0], ip[1], ip[2], ip[3],
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int ip_equals(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 4) == 0;
}

void arp_handle(const void *data, unsigned int length,
                const uint8_t *src_mac, device_handle *dev,
                nic_driver_t *drv)
{
    if (length < sizeof(arp_packet)) {
        printf("ARP: packet too short (%u bytes)\n", length);
        return;
    }
    
    const arp_packet *arp = (const arp_packet *)data;
    uint16_t opcode = ntohs(arp->opcode);
    
    printf("ARP: opcode=%s sender=%d.%d.%d.%d target=%d.%d.%d.%d\n",
           opcode == ARP_REQUEST ? "REQUEST" : "REPLY",
           arp->sender_ip[0], arp->sender_ip[1],
           arp->sender_ip[2], arp->sender_ip[3],
           arp->target_ip[0], arp->target_ip[1],
           arp->target_ip[2], arp->target_ip[3]);
    
    /* Actualizar caché con quien habla */
    arp_cache_update(arp->sender_ip, (uint8_t *)src_mac);
    
    /* Si es REQUEST para nosotros, responder */
    if (opcode == ARP_REQUEST && ip_equals(arp->target_ip, dev->ip)) {
        printf("ARP: REQUEST for me, sending REPLY...\n");
        
        ethernet_frame frame = {0};
        
        unsigned int arp_len = arp_build_reply(frame.payload, dev,
                                               arp->sender_mac, arp->sender_ip);
        
        memcpy(frame.dest_mac, src_mac, 6);
        memcpy(frame.src_mac, dev->mac, 6);
        frame.ethertype = htons(ethtype_ARP);
        
        unsigned int total_len = ETH_HEADER_LEN + arp_len;
        
        if (ethernet_send(drv, (nic_device_t *)dev, &frame, total_len) == STATUS_OK) {
            printf("ARP: REPLY sent\n");
        } else {
            printf("ARP: failed to send REPLY\n");
        }
    }
}

unsigned int arp_build_request(void *buffer, device_handle *dev,
                               const uint8_t *target_ip)
{
    arp_packet *arp = (arp_packet *)buffer;
    
    arp->hw_type = htons(1);
    arp->proto_type = htons(0x0800);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_REQUEST);
    
    memcpy(arp->sender_mac, dev->mac, 6);
    memcpy(arp->sender_ip, dev->ip, 4);
    memset(arp->target_mac, 0, 6);
    memcpy(arp->target_ip, target_ip, 4);
    
    return sizeof(arp_packet);
}

unsigned int arp_build_reply(void *buffer, device_handle *dev,
                             const uint8_t *target_mac,
                             const uint8_t *target_ip)
{
    arp_packet *arp = (arp_packet *)buffer;
    
    arp->hw_type = htons(1);
    arp->proto_type = htons(0x0800);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_REPLY);
    
    memcpy(arp->sender_mac, dev->mac, 6);
    memcpy(arp->sender_ip, dev->ip, 4);
    memcpy(arp->target_mac, target_mac, 6);
    memcpy(arp->target_ip, target_ip, 4);
    
    return sizeof(arp_packet);
}