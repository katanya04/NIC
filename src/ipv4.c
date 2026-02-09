#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"
#include "tcp.h"
#include "dhcp.h"

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

static uint16_t checksum(void *data, int len)
{
    uint16_t *buf = data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len)
        sum += *(uint8_t *)buf;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}

int ipv4_send(device_handle *dev, nic_driver_t *drv, uint32_t dst, uint8_t proto,
              const uint8_t *payload, uint16_t payload_len)
{
    uint8_t buffer[2048];
    ethernet_frame *frame = (ethernet_frame *)buffer;
    uint8_t dst_ip[4];
    uint8_t dst_mac[6];

    /* Convertir dst a array para ARP lookup */
    dst_ip[0] = (dst >> 24) & 0xFF;
    dst_ip[1] = (dst >> 16) & 0xFF;
    dst_ip[2] = (dst >> 8)  & 0xFF;
    dst_ip[3] = dst & 0xFF;

    /* Broadcast: no ARP, MAC FF:FF:FF:FF:FF:FF (DHCP, etc.) */
    if (dst == 0xFFFFFFFF) {
        memset(dst_mac, 0xFF, 6);
    } else {
        /* Buscar MAC en caché ARP */
        if (arp_lookup(dst_ip, dst_mac) != 0) {
            printf("IPv4: ARP lookup failed for %d.%d.%d.%d, sending ARP request...\n",
                   dst_ip[0], dst_ip[1], dst_ip[2], dst_ip[3]);

            /* Enviar ARP request y devolver error (el llamador debe reintentar) */
            uint8_t arp_buf[sizeof(arp_packet)];
            unsigned int arp_len = arp_build_request(arp_buf, dev, dst_ip);

            ethernet_frame *arp_frame = (ethernet_frame *)buffer;
            memset(arp_frame->dest_mac, 0xFF, 6);
            memcpy(arp_frame->src_mac, dev->mac, 6);
            arp_frame->ethertype = htons(ethtype_ARP);
            memcpy(arp_frame->payload, arp_buf, arp_len);

            ethernet_send(drv, (nic_device_t *)dev, arp_frame, ETH_HEADER_LEN + arp_len);
            return -1;  /* Necesita reintentar después de recibir ARP reply */
        }
    }

    /* Construir IP header en payload del frame */
    ipv4_hdr_t *hdr = (ipv4_hdr_t *)frame->payload;

    uint32_t src = (dev->ip[0] << 24) | (dev->ip[1] << 16) |
                   (dev->ip[2] << 8)  | dev->ip[3];

    hdr->ver_ihl   = (IPV4_VERSION << 4) | 5;
    hdr->tos       = 0;
    hdr->total_len = htons(IPV4_HEADER_LEN + payload_len);
    hdr->id        = htons(1);
    hdr->flags_frag= 0;
    hdr->ttl       = 64;
    hdr->protocol  = proto;
    hdr->checksum  = 0;
    hdr->src       = htonl(src);
    hdr->dst       = htonl(dst);

    hdr->checksum = checksum(hdr, IPV4_HEADER_LEN);
    memcpy(frame->payload + IPV4_HEADER_LEN, payload, payload_len);

    /* Usar MAC de caché ARP o broadcast */
    memcpy(frame->dest_mac, dst_mac, 6);
    memcpy(frame->src_mac, dev->mac, 6);
    frame->ethertype = htons(ethtype_IPv4);

    unsigned int total_len = ETH_HEADER_LEN + IPV4_HEADER_LEN + payload_len;

    return ethernet_send(drv, (nic_device_t *)dev, frame, total_len);
}

void ipv4_handler(uint8_t *packet, int len, device_handle *dev, nic_driver_t *drv)
{
    (void)drv;

    if (len < IPV4_HEADER_LEN)
        return;

    ipv4_hdr_t *hdr = (ipv4_hdr_t *)packet;
    if ((hdr->ver_ihl >> 4) != IPV4_VERSION)
        return;

    uint32_t src_ip = ntohl(hdr->src);
    uint32_t dst_ip = ntohl(hdr->dst);

    /* Aquí podrías actualizar caché ARP con la IP origen si tuvieras la MAC del frame */

    uint32_t my_ip = (dev->ip[0] << 24) | (dev->ip[1] << 16) |
                     (dev->ip[2] << 8)  | dev->ip[3];

    /* Aceptar paquetes dirigidos a nosotros o a broadcast (para DHCP) */
    uint32_t broadcast_ip = 0xFFFFFFFF;
    if (dst_ip != my_ip && dst_ip != broadcast_ip)
        return;

    uint8_t *payload = packet + IPV4_HEADER_LEN;
    int payload_len  = ntohs(hdr->total_len) - IPV4_HEADER_LEN;

    switch (hdr->protocol) {
        case IPV4_PROTO_ICMP:
            icmp_handler(payload, payload_len, dev, drv, src_ip);
            break;
        case IPV4_PROTO_TCP:
            tcp_handler(payload, payload_len, dev, drv, src_ip, dst_ip);
            break;
        case IPV4_PROTO_UDP:
            dhcp_handle_udp(payload, payload_len, dev);
            break;
        default:
            break;
    }
}