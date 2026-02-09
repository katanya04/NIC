#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>
#include "hal.h"
#include "interface.h"

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_hdr_t;

/* Handler de recepción ICMP */
void icmp_handler(uint8_t *packet, int len, device_handle *dev, 
                  nic_driver_t *drv, uint32_t src_ip);

/* Enviar Echo Reply (respuesta a ping) */
int icmp_send_echo_reply(device_handle *dev, nic_driver_t *drv,
                         uint32_t dst_ip, uint16_t id, uint16_t seq,
                         uint8_t *data, uint16_t data_len);

#endif