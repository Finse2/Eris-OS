#ifndef NE2000_H
#define NE2000_H

// Initialize RTL8029/NE2000 over PCI + ISA-compatible I/O.
// Returns 1 on success, 0 on failure.
int ne2k_init(void);

// Returns 1 when NIC is initialized and ready.
int ne2k_is_ready(void);

// Accessors for diagnostics.
unsigned short ne2k_get_io_base(void);
unsigned char ne2k_get_irq_line(void);
void ne2k_get_mac(unsigned char* out_mac6);

// Send one raw Ethernet frame. Returns bytes sent, or 0 on failure.
int ne2k_send_frame(const unsigned char* data, unsigned short len);

// Handle one NE2000 hardware IRQ event (called from the NIC IRQ ISR).
void ne2k_handle_irq(void);

// Poll for one received frame.
// Returns:
//   >0 bytes copied into buffer
//    0 no packet available
//   -1 invalid arguments / not ready
int ne2k_poll_receive(unsigned char* buffer, unsigned short max_len);

// Enable/disable NIC IRQ generation at the device level.
void ne2k_set_irq_enabled(int enabled);

#endif
