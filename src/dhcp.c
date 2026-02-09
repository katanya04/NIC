#include "dhcp.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

dhcp_lease_t dhcp_lease = {0};

// XID fijo para simplificar el laboratorio
static uint32_t dhcp_xid = 0x12345678;

// Magic cookie DHCP
static const uint8_t dhcp_magic_cookie[4] = { 99, 130, 83, 99 };

static void dhcp_add_option(uint8_t **opt_ptr, uint8_t code, uint8_t len, const void *data)
{
    uint8_t *p = *opt_ptr;
    p[0] = code;
    p[1] = len;
    memcpy(&p[2], data, len);
    *opt_ptr = p + 2 + len;
}

static int dhcp_find_option(uint8_t *options, int max_len,
                            uint8_t code, uint8_t **value, uint8_t *len)
{
    int i = 0;
    while (i < max_len) {
        uint8_t opt = options[i];
        if (opt == DHCP_OPTION_END)
            break;
        if (i + 1 >= max_len)
            break;

        uint8_t opt_len = options[i + 1];
        if (i + 2 + opt_len > max_len)
            break;

        if (opt == code) {
            *value = &options[i + 2];
            *len = opt_len;
            return 1;
        }

        i += 2 + opt_len;
    }
    return 0;
}

static void dhcp_send(struct device_handle *dev,
                      uint8_t msg_type,
                      uint32_t req_ip,
                      uint32_t server_id)
{
    uint8_t buffer[1500];
    memset(buffer, 0, sizeof(buffer));

    // Construir cabecera UDP + DHCP dentro de buffer
    udp_hdr_t *uh = (udp_hdr_t *)buffer;
    dhcp_msg_t *dh = (dhcp_msg_t *)(buffer + sizeof(udp_hdr_t));

    // Cabecera DHCP
    dh->op    = BOOTREQUEST;
    dh->htype = 1;   // Ethernet
    dh->hlen  = 6;   // MAC 6 bytes
    dh->hops  = 0;

    dh->xid   = htonl(dhcp_xid);
    dh->secs  = 0;
    dh->flags = htons(0x8000); // Broadcast

    dh->ciaddr = 0;
    dh->yiaddr = 0;
    dh->siaddr = 0;
    dh->giaddr = 0;

    memset(dh->chaddr, 0, sizeof(dh->chaddr));
    memcpy(dh->chaddr, dev->mac, 6);

    memcpy(dh->options, dhcp_magic_cookie, 4);
    uint8_t *opt_ptr = dh->options + 4;

    // Opción: tipo de mensaje
    dhcp_add_option(&opt_ptr, DHCP_OPTION_MSG_TYPE, 1, &msg_type);

    if (msg_type == DHCPREQUEST) {
        if (req_ip != 0) {
            uint32_t ip_n = htonl(req_ip);
            dhcp_add_option(&opt_ptr, DHCP_OPTION_REQ_IP, 4, &ip_n);
        }
        if (server_id != 0) {
            uint32_t sid_n = htonl(server_id);
            dhcp_add_option(&opt_ptr, DHCP_OPTION_SERVERID, 4, &sid_n);
        }
    }

    // Fin de opciones
    *opt_ptr = DHCP_OPTION_END;

    int dhcp_len = sizeof(dhcp_msg_t); // enviamos estructura completa

    // Cabecera UDP
    uh->src_port = htons(DHCP_CLIENT_PORT);
    uh->dst_port = htons(DHCP_SERVER_PORT);
    uh->len      = htons(sizeof(udp_hdr_t) + dhcp_len);
    uh->checksum = 0; // para el laboratorio, lo dejamos a 0

    int udp_total_len = sizeof(udp_hdr_t) + dhcp_len;

    // Enviar como IPv4/UDP a broadcast 255.255.255.255
    uint32_t dst_ip = 0xFFFFFFFF; // broadcast
    ipv4_send(dev, dst_ip, 17 /* UDP */, buffer, udp_total_len);
}

void dhcp_start(struct device_handle *dev)
{
    printf("[DHCP] Enviando DHCPDISCOVER...\n");
    dhcp_lease.valid = 0;
    dhcp_send(dev, DHCPDISCOVER, 0, 0);
}

void dhcp_handle_udp(uint8_t *segment, int len, struct device_handle *dev)
{
    if (len < (int)sizeof(udp_hdr_t) + (int)sizeof(dhcp_msg_t))
        return;

    udp_hdr_t *uh = (udp_hdr_t *)segment;
    uint16_t dst_port = ntohs(uh->dst_port);
    uint16_t src_port = ntohs(uh->src_port);

    if (dst_port != DHCP_CLIENT_PORT || src_port != DHCP_SERVER_PORT)
        return;

    dhcp_msg_t *dh = (dhcp_msg_t *)(segment + sizeof(udp_hdr_t));

    // Comprobar XID
    if (ntohl(dh->xid) != dhcp_xid)
        return;

    // Comprobar magic cookie
    if (memcmp(dh->options, dhcp_magic_cookie, 4) != 0)
        return;

    uint8_t *opts = dh->options + 4;
    int opts_len = sizeof(dh->options) - 4;

    uint8_t *val;
    uint8_t opt_len;
    uint8_t msg_type = 0;

    if (dhcp_find_option(opts, opts_len, DHCP_OPTION_MSG_TYPE, &val, &opt_len) && opt_len == 1)
        msg_type = val[0];

    if (msg_type == DHCPOFFER) {
        uint32_t offered_ip = ntohl(dh->yiaddr);
        printf("[DHCP] DHCPOFFER recibido. IP ofrecida: %u.%u.%u.%u\n",
               (offered_ip >> 24) & 0xFF,
               (offered_ip >> 16) & 0xFF,
               (offered_ip >> 8)  & 0xFF,
               offered_ip & 0xFF);

        uint32_t server_id = 0;
        if (dhcp_find_option(opts, opts_len, DHCP_OPTION_SERVERID, &val, &opt_len) && opt_len == 4)
            server_id = ntohl(*(uint32_t *)val);

        dhcp_lease.ip        = offered_ip;
        dhcp_lease.server_id = server_id;

        // Enviar DHCPREQUEST
        printf("[DHCP] Enviando DHCPREQUEST...\n");
        dhcp_send(dev, DHCPREQUEST, offered_ip, server_id);
    }
    else if (msg_type == DHCPACK) {
        uint32_t assigned_ip = ntohl(dh->yiaddr);
        dhcp_lease.ip = assigned_ip;

        // Subnet mask
        if (dhcp_find_option(opts, opts_len, DHCP_OPTION_SUBNET, &val, &opt_len) && opt_len == 4)
            dhcp_lease.subnet = ntohl(*(uint32_t *)val);

        // Router (gateway)
        if (dhcp_find_option(opts, opts_len, DHCP_OPTION_ROUTER, &val, &opt_len) && opt_len >= 4)
            dhcp_lease.gateway = ntohl(*(uint32_t *)val);

        // Lease time
        if (dhcp_find_option(opts, opts_len, DHCP_OPTION_LEASE, &val, &opt_len) && opt_len == 4)
            dhcp_lease.lease_time = ntohl(*(uint32_t *)val);

        dhcp_lease.valid = 1;

        // Escribir IP en dev->ip[4]
        dev->ip[0] = (assigned_ip >> 24) & 0xFF;
        dev->ip[1] = (assigned_ip >> 16) & 0xFF;
        dev->ip[2] = (assigned_ip >> 8)  & 0xFF;
        dev->ip[3] = assigned_ip & 0xFF;

        printf("[DHCP] DHCPACK recibido. IP asignada: %u.%u.%u.%u\n",
               dev->ip[0], dev->ip[1], dev->ip[2], dev->ip[3]);
    }
}