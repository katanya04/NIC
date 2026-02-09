#include "tcp.h"
#include "ipv4.h"
#include "http.h"
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>

typedef struct {
    uint32_t src;
    uint32_t dst;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t tcp_len;
} __attribute__((packed)) tcp_pseudo_hdr_t;

static tcp_conn_t conn = {0};
static uint16_t http_port = 80;

static uint16_t checksum(void *data, int len)
{
    uint16_t *buf = data;
    uint32_t sum = 0;
    while(len > 1) { sum += *buf++; len -= 2; }
    if(len) sum += *(uint8_t*)buf;
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

static uint16_t tcp_checksum(uint32_t src, uint32_t dst, tcp_hdr_t *tcp,
                             int tcp_len, const uint8_t *payload, int payload_len)
{
    uint8_t buf[2048];
    tcp_pseudo_hdr_t *ph = (tcp_pseudo_hdr_t*)buf;
    
    ph->src = htonl(src);
    ph->dst = htonl(dst);
    ph->zero = 0;
    ph->proto = TCP_PROTO_IP;
    ph->tcp_len = htons(tcp_len + payload_len);
    
    memcpy(buf + sizeof(tcp_pseudo_hdr_t), tcp, tcp_len);
    if(payload_len)
        memcpy(buf + sizeof(tcp_pseudo_hdr_t) + tcp_len, payload, payload_len);
    
    return checksum(buf, sizeof(tcp_pseudo_hdr_t) + tcp_len + payload_len);
}

int tcp_send(struct device_handle *dev, nic_driver_t *drv, uint32_t dst_ip, uint16_t src_port,
             uint16_t dst_port, uint32_t seq, uint32_t ack,
             uint8_t flags, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t buffer[1500];
    tcp_hdr_t *hdr = (tcp_hdr_t*)buffer;
    
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->seq = htonl(seq);
    hdr->ack = htonl(ack);
    hdr->data_offset = (TCP_HEADER_LEN / 4) << 4;
    hdr->flags = flags;
    hdr->window = htons(65535);
    hdr->checksum = 0;
    hdr->urgent = 0;
    
    if(payload_len)
        memcpy(buffer + TCP_HEADER_LEN, payload, payload_len);
    
    uint32_t src_ip = (dev->ip[0] << 24) | (dev->ip[1] << 16) | 
                      (dev->ip[2] << 8) | dev->ip[3];
    
    hdr->checksum = tcp_checksum(src_ip, dst_ip, hdr, TCP_HEADER_LEN, payload, payload_len);
    
    return ipv4_send(dev, drv, dst_ip, IPV4_PROTO_TCP, buffer, TCP_HEADER_LEN + payload_len);
}

void tcp_handler(uint8_t *packet, int len, struct device_handle *dev, nic_driver_t *drv,
                 uint32_t src_ip, uint32_t dst_ip)
{
    if(len < TCP_HEADER_LEN) return;
    
    tcp_hdr_t *hdr = (tcp_hdr_t*)packet;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint32_t seq = ntohl(hdr->seq);
    uint32_t ack = ntohl(hdr->ack);
    uint8_t flags = hdr->flags;
    uint8_t data_off = (hdr->data_offset >> 4) * 4;
    uint8_t *payload = packet + data_off;
    int payload_len = len - data_off;
    
    printf("TCP: src=%d dst=%d flags=%02x seq=%u ack=%u len=%d\n",
           src_port, dst_port, flags, seq, ack, payload_len);
    
    if(dst_port != http_port) {
        printf("TCP: not for port %d\n", http_port);
        return;
    }
    
    /* SYN -> SYN+ACK */
    if((flags & TCP_FLAG_SYN) && !(flags & TCP_FLAG_ACK)) {
        printf("TCP: SYN received, sending SYN+ACK...\n");
        
        conn.remote_ip = src_ip;
        conn.remote_port = src_port;
        conn.local_port = dst_port;
        conn.seq = 1000;
        conn.ack = seq + 1;
        conn.state = 2;
        
        tcp_send(dev, drv, src_ip, dst_port, src_port, conn.seq, conn.ack,
                 TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
        conn.seq++;
        return;
    }
    
    /* ACK final */
    if((flags & TCP_FLAG_ACK) && conn.state == 2) {
        printf("TCP: Connection established!\n");
        conn.state = 3;
    }
    
    /* Datos */
    if(conn.state == 3 && payload_len > 0) {
        printf("TCP: HTTP data received (%d bytes)\n", payload_len);
        conn.ack = seq + payload_len;
        tcp_send(dev, drv, conn.remote_ip, conn.local_port, conn.remote_port,
                 conn.seq, conn.ack, TCP_FLAG_ACK, NULL, 0);
    }
    
    /* FIN */
    if(flags & TCP_FLAG_FIN) {
        printf("TCP: FIN received, closing...\n");
        conn.ack = seq + 1;
        tcp_send(dev, drv, conn.remote_ip, conn.local_port, conn.remote_port,
                 conn.seq, conn.ack, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        conn.state = 0;
    }
}