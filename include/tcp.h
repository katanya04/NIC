#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include "hal.h"  // Para struct device_handle
#include "interface.h"

#define TCP_HEADER_LEN  20
#define TCP_FLAG_FIN    0x01
#define TCP_FLAG_SYN    0x02
#define TCP_FLAG_RST    0x04
#define TCP_FLAG_PSH    0x08
#define TCP_FLAG_ACK    0x10

#define TCP_PROTO_IP    6

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_hdr_t;

/* Conexión TCP simplificada */
typedef struct {
    uint32_t remote_ip;
    uint16_t remote_port;
    uint16_t local_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  state;
} tcp_conn_t;

void tcp_handler(uint8_t *packet, int len, struct device_handle *dev, nic_driver_t *drv,
                 uint32_t src_ip, uint32_t dst_ip);
int tcp_send(struct device_handle *dev, nic_driver_t *drv, uint32_t dst_ip, uint16_t src_port,
             uint16_t dst_port, uint32_t seq, uint32_t ack,
             uint8_t flags, const uint8_t *payload, uint16_t payload_len);

#endif