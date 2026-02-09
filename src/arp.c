#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "ethernet.h"
#include "arp.h"
#include "interface.h"  // Para nic_driver_t, nic_device_t, STATUS_OK

static int ip_equals(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 4) == 0;
}

void arp_receive(const void *data, unsigned int length,
                 const uint8_t *src_mac, device_handle *dev,
                 void *driver)
{
    if (length < sizeof(arp_packet)) {
        printf("ARP: packet too short (%u bytes)\n", length);
        return;
    }
    
    const arp_packet *arp = (const arp_packet *)data;
    uint16_t opcode = ntohs(arp->opcode);
    
    printf("ARP: opcode=%s sender=%d.%d.%d.%d target=%d.%d.%d.%d\n",
           opcode == ARP_REQUEST ? "REQUEST" : 
           opcode == ARP_REPLY ? "REPLY" : "UNKNOWN",
           arp->sender_ip[0], arp->sender_ip[1],
           arp->sender_ip[2], arp->sender_ip[3],
           arp->target_ip[0], arp->target_ip[1],
           arp->target_ip[2], arp->target_ip[3]);
    
    if (opcode == ARP_REQUEST && ip_equals(arp->target_ip, dev->ip)) {
        printf("ARP: REQUEST for me (%s), sending REPLY...\n", dev->name);
        
        uint8_t arp_buf[sizeof(arp_packet)];
        unsigned int arp_len = arp_build_reply(arp_buf, dev,
                                               arp->sender_mac, 
                                               arp->sender_ip);
        
        ethernet_frame eth_frame;
        unsigned int frame_len = eth_build_frame(&eth_frame, dev->mac,
                                                  src_mac, ethtype_ARP,
                                                  arp_buf, arp_len);
        
        nic_driver_t *drv = (nic_driver_t *)driver;
        if (drv->send_packet((nic_device_t *)dev, &eth_frame, frame_len) == STATUS_OK) {
            printf("ARP: REPLY sent via %s\n", dev->name);
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