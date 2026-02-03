#ifndef _HAL_H
#define _HAL_H 

#define HAL_IFACE_NAME "eth0"
#define HAL_IFACE_NAMELEN 32

void * hal_create_device();
void hal_remove_device(void *handle);
unsigned int hal_send(void * handle, void * data, unsigned int length);
unsigned int hal_receive(void * handle, void * buffer, unsigned int buffer_length);
void hal_get_mac_address(void * handle, unsigned char *mac);
unsigned int hal_get_mtu(void * handle);
#endif