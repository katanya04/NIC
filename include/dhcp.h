#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>
#include "hal.h"
#include "ipv4.h"

// Puertos estándar DHCP
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

// Tipos de mensaje DHCP
#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7
#define DHCPINFORM   8

// Opciones DHCP
#define DHCP_OPTION_SUBNET   1
#define DHCP_OPTION_ROUTER   3
#define DHCP_OPTION_REQ_IP   50
#define DHCP_OPTION_LEASE    51
#define DHCP_OPTION_MSG_TYPE 53
#define DHCP_OPTION_SERVERID 54
#define DHCP_OPTION_END      255

// BOOTP opcodes
#define BOOTREQUEST 1
#define BOOTREPLY   2

// Cabecera UDP mínima
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} udp_hdr_t;

// Cabecera DHCP (formato BOOTP + opciones)
typedef struct __attribute__((packed)) {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;

    uint32_t xid;
    uint16_t secs;
    uint16_t flags;

    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;

    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];

    uint8_t  options[312];
} dhcp_msg_t;

typedef struct {
    uint32_t ip;
    uint32_t subnet;
    uint32_t gateway;
    uint32_t server_id;
    uint32_t lease_time;
    int      valid;
} dhcp_lease_t;

// Estado global sencillo de DHCP
extern dhcp_lease_t dhcp_lease;

// Arranca el proceso DHCP (envía DHCPDISCOVER)
void dhcp_start(struct device_handle *dev);

// Maneja un segmento UDP que podría contener DHCP (UDP payload completo)
void dhcp_handle_udp(uint8_t *segment, int len, struct device_handle *dev);

#endif