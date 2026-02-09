#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "interface.h"
#include "ethernet.h"


nic_device_t nic;
nic_driver_t * drv;

void on_receive_packet(const void *data, unsigned int length) {
    struct device_handle *dev = (struct device_handle *)nic.hw_handle;
    
    printf("\nRX: %u bytes on %s\n", length, dev->name);
    ethernet_receive(data, length, dev, drv);
}

int main(int argc, char* argv[]) {
    drv = nic_get_driver();

    if (drv->init(&nic) != STATUS_OK) {
        printf("Failed to initialize NIC\n");
        return -1;
    }
    if (drv->ioctl(&nic, NIC_IOCTL_ADD_RX_CALLBACK, (void *)&on_receive_packet) != STATUS_OK) {
        printf("Failed to add RX callback\n");
        drv->shutdown(&nic);
        return -1;
    }

    ethernet_frame test_eth;
    unsigned int packet_length = eth_build_frame(
        &test_eth,
        nic.mac_address, // Source MAC
        (unsigned char[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Broadcast MAC
        ethtype_ECTP, // Ethertype for testing
        (unsigned char *)"Hello, this is a test payload", //Payload content
        29 // Payload length
    );
    
    if (drv->send_packet(&nic, &test_eth, packet_length) != STATUS_OK) {
        printf("Failed to send packet\n");
        drv->shutdown(&nic);
        return -1;
    }

    //Wait for key press to exit
    printf("Press Enter to exit...\n");
    getchar();

    if (drv->shutdown(&nic) != STATUS_OK) {
        printf("Failed to shutdown NIC\n");
        return -1;
    }
    return 0;
}