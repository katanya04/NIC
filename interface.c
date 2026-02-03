
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "interface.h"
#include "hal.h"

typedef unsigned char flags_t;
typedef enum {
    NIC_BUFFER_RX,
    NIC_BUFFER_TX
} nic_buffer_id;

#define __TX_FLAGS_NONE         0x00
#define __TX_CB_FLAG            0x01
#define __RX_CB_FLAG            0x02
#define __ERROR_CB_FLAG         0x08
#define __CLEAR_ALL_FLAGS(flags)   ((flags) = __TX_FLAGS_NONE)
#define __GET_TX_CB(flags)      ((flags) & __TX_CB_FLAG)
#define __GET_RX_CB(flags)      ((flags) & __RX_CB_FLAG)
#define __GET_ERROR_CB(flags)   ((flags) & __ERROR_CB_FLAG)
#define __SET_TX_CB(flags)      ((flags) |= __TX_CB_FLAG)
#define __SET_RX_CB(flags)      ((flags) |= __RX_CB_FLAG)
#define __SET_ERROR_CB(flags)   ((flags) |= __ERROR_CB_FLAG)

status_t __nic_add_callback(nic_callback_t **callback_list, nic_event_callback_t callback) {
    nic_callback_t *new_callback = (nic_callback_t *)malloc(sizeof(nic_callback_t));
    if (!new_callback) {
        return STATUS_ERROR;
    }
    new_callback->callback = callback;
    new_callback->next = *callback_list;
    *callback_list = new_callback;
    return STATUS_OK;
}

status_t __nic_remove_callback(nic_callback_t **callback_list, nic_event_callback_t callback) {
    nic_callback_t *current = *callback_list;
    nic_callback_t *previous = NULL;
    while (current) {
        if (current->callback == callback) {
            if (previous) {
                previous->next = current->next;
            } else {
                *callback_list = current->next;
            }
            free(current);
            return STATUS_OK;
        }
        previous = current;
        current = current->next;
    }
    return STATUS_NOT_SUPPORTED; // Callback not found
}

void __nic_thread(void * args) {
    nic_device_t *device = (nic_device_t *)args;
    //Main NIC processing loop
    //1) update the rx buffer reading from hardware and update stats
    //2) send everything in the tx buffer to hardware and update stats
    //3) trigger callbacks as needed
    unsigned char working_buffer[device->mtu+NIC_EXTRA_SIZE];
    unsigned int received_length = 0;
    flags_t internal_flags = __TX_FLAGS_NONE;
    while (device->is_up) {
        __CLEAR_ALL_FLAGS(internal_flags);
        //Step 1: Receive packets from hardware into working buffer
        received_length = hal_receive(device->hw_handle, working_buffer, device->mtu+NIC_EXTRA_SIZE);
        if (received_length > 0) {
            //Update rx statistics
            device->stats.rx_packets++;
            //Copy received data into rx buffer
            nic_buffer_t *new_rx_buffer = (nic_buffer_t *)malloc(sizeof(nic_buffer_t));
            if (new_rx_buffer) {
                new_rx_buffer->data = malloc(received_length);
                if (new_rx_buffer->data) {
                    memcpy(new_rx_buffer->data, working_buffer, received_length);
                    new_rx_buffer->length = received_length;
                    new_rx_buffer->next = device->rx_buffer;
                    device->rx_buffer = new_rx_buffer;
                    __SET_RX_CB(internal_flags);
                } else {
                    free(new_rx_buffer);
                    device->stats.rx_errors++;
                    __SET_ERROR_CB(internal_flags);
                    nic_callback_t *error_cb = device->error_callbacks;
                    while (error_cb) {
                        if (error_cb->callback) error_cb->callback(NULL, 0);
                        error_cb = error_cb->next;
                    }
                }
            }
        }
        //Step 2: Send packets from tx buffer to hardware
        nic_buffer_t *tx_buf = device->tx_buffer;
        while (tx_buf) {
            unsigned int sent_length = hal_send(device->hw_handle, tx_buf->data, tx_buf->length);
            if (sent_length == tx_buf->length) {
                device->stats.tx_packets++;
                __SET_TX_CB(internal_flags);
            } else {
                device->stats.tx_errors++;
                __SET_ERROR_CB(internal_flags);
                nic_callback_t *error_cb = device->error_callbacks;
                while (error_cb) {
                    if (error_cb->callback) error_cb->callback(NULL, 0);
                    error_cb = error_cb->next;
                }
            }
            nic_buffer_t *temp = tx_buf;
            tx_buf = tx_buf->next;
            free(temp->data);
            free(temp);
        }
        device->tx_buffer = NULL;
        //Step 3: Trigger callbacks based on internal flags
        if (__GET_RX_CB(internal_flags)) {
            nic_callback_t *cb = device->rx_callbacks;
            while (cb) {
                if (cb->callback) cb->callback(working_buffer, received_length);
                cb = cb->next;
            }
        }
        if (__GET_TX_CB(internal_flags)) {
            nic_callback_t *cb = device->tx_callbacks;
            while (cb) {
                if (cb->callback) cb->callback(NULL, 0);
                cb = cb->next;
            }
        }
        //Sleep or yield to avoid busy waiting
        usleep(1000); // Sleep for 1ms
    }
}

status_t __nic_thread_control(nic_device_t *device, int start) {
    if (start) {
        device->is_up = 1;
        if (pthread_create(&device->thread, NULL, (void *)__nic_thread, (void *)device) != 0) {
            device->is_up = 0;
            return STATUS_ERROR;
        }
        return STATUS_OK;
    } else {
        device->is_up = 0;
        if (pthread_join(device->thread, NULL) != 0) {
            return STATUS_ERROR;
        }
        return STATUS_OK;
    }
}

status_t nic_init(nic_device_t *device) {
    // Initialize the NIC device (e.g., allocate resources, set default values)
    if (!device) {
        return STATUS_INVALID_PARAM;
    }
    
    // Set default MTU and MAC address
    device->mtu = NIC_DEFAULT_MTU;
    device->promiscuous_mode = 0;
    unsigned char default_mac[6] = NIC_DEFAULT_MAC;
    for (int i = 0; i < 6; i++) {
        device->mac_address[i] = default_mac[i];
    }

    // Set statistics to zero
    device->stats.tx_packets = 0;
    device->stats.rx_packets = 0;
    device->stats.tx_errors = 0;
    device->stats.rx_errors = 0;
    device->stats.collisions = 0;

    // Set underlying hardware handle
    device->hw_handle = hal_create_device();
    if (!device->hw_handle) {
        return STATUS_ERROR;
    }
    device->mtu = hal_get_mtu(device->hw_handle);
    hal_get_mac_address(device->hw_handle, device->mac_address);

    // Initialize internal buffers and callback lists to NULL
    device->rx_buffer = NULL;
    device->tx_buffer = NULL;
    device->rx_callbacks = NULL;
    device->tx_callbacks = NULL;
    device->error_callbacks = NULL;

    // Init the thread for NIC processing
    device->is_up = 0;
    if (__nic_thread_control(device, 1) != STATUS_OK) {
        hal_remove_device(device->hw_handle);
        device->hw_handle = NULL;
        return STATUS_ERROR;
    }

    // Additional initialization code here
    return STATUS_OK;
}

status_t nic_shutdown(nic_device_t *device) {
    // Shutdown the NIC device (e.g., free resources)
    if (!device) {
        return STATUS_INVALID_PARAM;
    }

    // Stop the NIC processing thread
    if (__nic_thread_control(device, 0) != STATUS_OK) {
        return STATUS_ERROR;
    }

    // Free internal buffers
    nic_buffer_t *buf;
    while (device->rx_buffer) {
        buf = device->rx_buffer;
        device->rx_buffer = buf->next;
        free(buf->data);
        free(buf);
    }
    while (device->tx_buffer) {
        buf = device->tx_buffer;
        device->tx_buffer = buf->next;
        free(buf->data);
        free(buf);
    }
    // Free callback lists
    nic_callback_t *cb;
    while (device->rx_callbacks) {
        cb = device->rx_callbacks;
        device->rx_callbacks = cb->next;
        free(cb);
    }
    while (device->tx_callbacks) {
        cb = device->tx_callbacks;
        device->tx_callbacks = cb->next;
        free(cb);
    }
    while (device->error_callbacks) {
        cb = device->error_callbacks;
        device->error_callbacks = cb->next;
        free(cb);
    }

    // Remove hardware handle
    hal_remove_device(device->hw_handle);
    device->hw_handle = NULL;

    // Additional shutdown code here
    return STATUS_OK;
}

status_t nic_ioctl(nic_device_t *device, unsigned int command, void *arg) {
    switch (command) {
        case NIC_IOCTL_CHANGE_MAC: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            for (int i = 0; i < 6; i++) {
                device->mac_address[i] = ((unsigned char *)arg)[i];
            }
            return STATUS_OK;
        }
        case NIC_IOCTL_SET_MTU: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            device->mtu = *(unsigned int *)arg;
            return STATUS_OK;
        }
        case NIC_IOCTL_GET_STATS: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            *(nic_stats_t *)arg = device->stats;
            return STATUS_OK;
        }
        case NIC_IOCTL_RESET_STATS: {
            if (!device) {
                return STATUS_INVALID_PARAM;
            }
            device->stats.tx_packets = 0;
            device->stats.rx_packets = 0;
            device->stats.tx_errors = 0;
            device->stats.rx_errors = 0;
            device->stats.collisions = 0;
            return STATUS_OK;
        }
        case NIC_IOCTL_ADD_RX_CALLBACK: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_add_callback(&device->rx_callbacks, (nic_event_callback_t)arg);
        }
        case NIC_IOCTL_REMOVE_RX_CALLBACK: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_remove_callback(&device->rx_callbacks, (nic_event_callback_t)arg);
        }
        case NIC_IOCTL_ADD_TX_CALLBACK: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_add_callback(&device->tx_callbacks, (nic_event_callback_t)arg);
        }
        case NIC_IOCTL_REMOVE_TX_CALLBACK: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_remove_callback(&device->tx_callbacks, (nic_event_callback_t)arg);
        }
        case NIC_IOCTL_ADD_ERROR_CALLBACK: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_add_callback(&device->error_callbacks, (nic_event_callback_t )arg);
        }
        case NIC_IOCTL_REMOVE_ERROR_CALLBACK: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_remove_callback(&device->error_callbacks, (nic_event_callback_t)arg);
        }
        case NIC_IOCTL_SET_PROMISCUOUS_MODE: {
            if (!device || !arg) {
                return STATUS_INVALID_PARAM;
            }
            device->promiscuous_mode = *(unsigned short *)arg;
            return STATUS_OK;
        }
        case NIC_IOCTL_UP: {
            if (!device) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_thread_control(device, 1);
        }
        case NIC_IOCTL_DOWN: {
            if (!device) {
                return STATUS_INVALID_PARAM;
            }
            return __nic_thread_control(device, 0);
        }
        default:
            return STATUS_NOT_SUPPORTED;
    }
}
        
status_t nic_send_packet(nic_device_t *device, const void *data, unsigned int length) {
    // Send a packet through the NIC by writing to the tx buffer
    if (!device || !data || length == 0 || length > device->mtu+NIC_EXTRA_SIZE) {
        return STATUS_INVALID_PARAM;
    }

    nic_buffer_t *new_tx_buffer = (nic_buffer_t *)malloc(sizeof(nic_buffer_t));
    if (!new_tx_buffer) {
        return STATUS_ERROR;
    }

    new_tx_buffer->data = malloc(length);
    if (!new_tx_buffer->data) {
        free(new_tx_buffer);
        return STATUS_ERROR;
    }
    memcpy(new_tx_buffer->data, data, length);
    new_tx_buffer->length = length;
    new_tx_buffer->next = NULL;
    // Append to the end of the tx buffer list
    if (!device->tx_buffer) {
        device->tx_buffer = new_tx_buffer;
    } else {
        nic_buffer_t *tx_buf = device->tx_buffer;
        while (tx_buf->next) {
            tx_buf = tx_buf->next;
        }
        tx_buf->next = new_tx_buffer;
    }

    return STATUS_OK;
}

status_t nic_receive_packet(nic_device_t *device, void *buffer, unsigned int buffer_length) {
    // Receive a packet from the NIC by reading from the rx buffer
    if (!device || !buffer || buffer_length == 0) {
        return STATUS_INVALID_PARAM;
    }

    if (!device->rx_buffer) {
        return STATUS_NOT_SUPPORTED; // No packets available
    }

    nic_buffer_t *rx_buf = device->rx_buffer;
    if (rx_buf->length > buffer_length) {
        return STATUS_INVALID_PARAM; // Buffer too small
    }

    memcpy(buffer, rx_buf->data, rx_buf->length);
    unsigned int received_length = rx_buf->length;

    // Remove the buffer from the rx list
    device->rx_buffer = rx_buf->next;
    free(rx_buf->data);
    free(rx_buf);

    return received_length;
}

nic_driver_t nic_driver = {
    .init = nic_init,
    .shutdown = nic_shutdown,
    .send_packet = nic_send_packet,
    .receive_packet = nic_receive_packet,
    .ioctl = nic_ioctl
};

nic_driver_t * nic_get_driver() {
    return &nic_driver;
}

