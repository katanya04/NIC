#include "icmp.h"
#include "ipv4.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

static uint16_t checksum(void *data, int len)
{
    uint16_t *buf = data;
    uint32_t sum = 0;
    while(len > 1) { sum += *buf++; len -= 2; }
    if(len) sum += *(uint8_t*)buf;
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

void icmp_handler(uint8_t *packet, int len, device_handle *dev,
                  nic_driver_t *drv, uint32_t src_ip)
{
    if(len < sizeof(icmp_hdr_t)) {
        printf("ICMP: packet too short\n");
        return;
    }
    
    icmp_hdr_t *icmp = (icmp_hdr_t *)packet;
    
    printf("ICMP: type=%d code=%d from %d.%d.%d.%d\n",
           icmp->type, icmp->code,
           (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
           (src_ip >> 8) & 0xFF, src_ip & 0xFF);
    
    /* Echo Request (ping) -> responder con Echo Reply */
    if(icmp->type == ICMP_TYPE_ECHO_REQUEST && icmp->code == 0) {
        printf("ICMP: Echo Request received, sending Reply...\n");
        
        uint8_t *echo_data = packet + sizeof(icmp_hdr_t);
        uint16_t echo_data_len = len - sizeof(icmp_hdr_t);
        
        icmp_send_echo_reply(dev, drv, src_ip, 
                            ntohs(icmp->id), ntohs(icmp->seq),
                            echo_data, echo_data_len);
    }
}

int icmp_send_echo_reply(device_handle *dev, nic_driver_t *drv,
                         uint32_t dst_ip, uint16_t id, uint16_t seq,
                         uint8_t *data, uint16_t data_len)
{
    uint8_t buffer[2048];
    icmp_hdr_t *icmp = (icmp_hdr_t *)buffer;
    
    icmp->type = ICMP_TYPE_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);
    
    memcpy(buffer + sizeof(icmp_hdr_t), data, data_len);
    
    icmp->checksum = checksum(buffer, sizeof(icmp_hdr_t) + data_len);
    
    return ipv4_send(dev, drv, dst_ip, IPV4_PROTO_ICMP,
                     buffer, sizeof(icmp_hdr_t) + data_len);
}