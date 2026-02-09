#ifndef _HAL_H
#define _HAL_H 

#define HAL_IFACE_NAMELEN 32

typedef struct device_handle {
    char name[HAL_IFACE_NAMELEN];
    int fd;
    int index;
    unsigned char mac[6];
    unsigned char ip[4];
    unsigned int mtu;
} device_handle;

void * hal_create_device();
void hal_remove_device(void *handle);
unsigned int hal_send(void * handle, void * data, unsigned int length);
unsigned int hal_receive(void * handle, void * buffer, unsigned int buffer_length);
void hal_get_mac_address(void * handle, unsigned char *mac);
unsigned int hal_get_mtu(void * handle);
#endif