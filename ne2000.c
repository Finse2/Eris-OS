#include "ne2000.h"

// Port I/O helpers implemented in main.c
extern unsigned char inportb(unsigned short port);
extern void outportb(unsigned short port, unsigned char data);

// PCI config mechanism #1 ports
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// Target NIC: Realtek RTL8029AS (NE2000-compatible)
#define RTL8029_VENDOR_ID  0x10EC
#define RTL8029_DEVICE_ID  0x8029

// NE2000 register offsets from io_base
#define NE_CMD     0x00
#define NE_PSTART  0x01
#define NE_PSTOP   0x02
#define NE_BNRY    0x03
#define NE_TPSR    0x04
#define NE_TBCR0   0x05
#define NE_TBCR1   0x06
#define NE_ISR     0x07
#define NE_RSAR0   0x08
#define NE_RSAR1   0x09
#define NE_RBCR0   0x0A
#define NE_RBCR1   0x0B
#define NE_RCR     0x0C
#define NE_TCR     0x0D
#define NE_DCR     0x0E
#define NE_IMR     0x0F
#define NE_DATA    0x10
#define NE_RESET   0x1F

// Page 1 register aliases
#define NE_PAR0    0x01
#define NE_CURR    0x07
#define NE_MAR0    0x08

// Command register bits
#define CR_STP     0x01
#define CR_STA     0x02
#define CR_TXP     0x04
#define CR_RD0     0x08
#define CR_RD1     0x10
#define CR_RD2     0x20
#define CR_PS0     0x40
#define CR_PS1     0x80

// Interrupt status bits
#define ISR_PRX    0x01
#define ISR_PTX    0x02
#define ISR_RXE    0x04
#define ISR_TXE    0x08
#define ISR_OVW    0x10
#define ISR_CNT    0x20
#define ISR_RDC    0x40
#define ISR_RST    0x80

// NIC memory layout (16KB)
#define TX_PAGE_START 0x40
#define TX_PAGE_COUNT 0x06
#define RX_PAGE_START 0x46
#define RX_PAGE_STOP  0x80

#define ETH_MIN_FRAME 60
#define ETH_MAX_FRAME 1518

static unsigned short g_io_base = 0;
static unsigned char g_irq_line = 0;
static unsigned char g_mac[6];
static int g_ready = 0;
static volatile unsigned char g_tx_done = 0;
static volatile unsigned char g_tx_error = 0;
static volatile unsigned char g_rx_pending = 0;
static volatile unsigned char g_irq_enabled = 0;

static unsigned int inportl(unsigned short port) {
	unsigned int value;
	__asm__ __volatile__("inl %1, %0" : "=a"(value) : "dN"(port));
	return value;
}

static void outportl(unsigned short port, unsigned int value) {
	__asm__ __volatile__("outl %0, %1" : : "a"(value), "dN"(port));
}

static unsigned int pci_config_read_dword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset) {
	unsigned int address = (1u << 31)
		| ((unsigned int)bus << 16)
		| ((unsigned int)slot << 11)
		| ((unsigned int)func << 8)
		| (offset & 0xFC);
	outportl(PCI_CONFIG_ADDRESS, address);
	return inportl(PCI_CONFIG_DATA);
}

static void pci_config_write_dword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset, unsigned int value) {
	unsigned int address = (1u << 31)
		| ((unsigned int)bus << 16)
		| ((unsigned int)slot << 11)
		| ((unsigned int)func << 8)
		| (offset & 0xFC);
	outportl(PCI_CONFIG_ADDRESS, address);
	outportl(PCI_CONFIG_DATA, value);
}

static int pci_find_rtl8029(unsigned char* out_bus, unsigned char* out_slot, unsigned char* out_func) {
	unsigned int bus;
	for(bus = 0; bus < 256; bus++) {
		unsigned int slot;
		for(slot = 0; slot < 32; slot++) {
			unsigned int header_dword = pci_config_read_dword((unsigned char)bus, (unsigned char)slot, 0, 0x0C);
			unsigned char header_type = (unsigned char)((header_dword >> 16) & 0xFF);
			unsigned int funcs = (header_type & 0x80) ? 8 : 1;
			unsigned int func;
			for(func = 0; func < funcs; func++) {
				unsigned int id = pci_config_read_dword((unsigned char)bus, (unsigned char)slot, (unsigned char)func, 0x00);
				unsigned short vendor = (unsigned short)(id & 0xFFFF);
				unsigned short device = (unsigned short)((id >> 16) & 0xFFFF);
				if(vendor == 0xFFFF) {
					if(func == 0) break;
					continue;
				}
				if(vendor == RTL8029_VENDOR_ID && device == RTL8029_DEVICE_ID) {
					*out_bus = (unsigned char)bus;
					*out_slot = (unsigned char)slot;
					*out_func = (unsigned char)func;
					return 1;
				}
			}
		}
	}
	return 0;
}

static void ne_write(unsigned char reg, unsigned char value) {
	outportb((unsigned short)(g_io_base + reg), value);
}

static unsigned char ne_read(unsigned char reg) {
	return inportb((unsigned short)(g_io_base + reg));
}

static int ne_wait_isr(unsigned char mask, unsigned int spins) {
	while(spins--) {
		if(ne_read(NE_ISR) & mask) {
			return 1;
		}
	}
	return 0;
}

static int ne_remote_read(unsigned short nic_addr, unsigned char* dst, unsigned short len) {
	if(len == 0) return 1;
	ne_write(NE_ISR, ISR_RDC);
	ne_write(NE_RBCR0, (unsigned char)(len & 0xFF));
	ne_write(NE_RBCR1, (unsigned char)((len >> 8) & 0xFF));
	ne_write(NE_RSAR0, (unsigned char)(nic_addr & 0xFF));
	ne_write(NE_RSAR1, (unsigned char)((nic_addr >> 8) & 0xFF));
	ne_write(NE_CMD, CR_STA | CR_RD0); // Remote read
	{
		unsigned short i;
		for(i = 0; i < len; i++) {
			dst[i] = ne_read(NE_DATA);
		}
	}
	if(!ne_wait_isr(ISR_RDC, 200000)) return 0;
	ne_write(NE_ISR, ISR_RDC);
	ne_write(NE_CMD, CR_STA | CR_RD2);
	return 1;
}

static int ne_remote_write(unsigned short nic_addr, const unsigned char* src, unsigned short len) {
	if(len == 0) return 1;
	ne_write(NE_ISR, ISR_RDC);
	ne_write(NE_RBCR0, (unsigned char)(len & 0xFF));
	ne_write(NE_RBCR1, (unsigned char)((len >> 8) & 0xFF));
	ne_write(NE_RSAR0, (unsigned char)(nic_addr & 0xFF));
	ne_write(NE_RSAR1, (unsigned char)((nic_addr >> 8) & 0xFF));
	ne_write(NE_CMD, CR_STA | CR_RD1); // Remote write
	{
		unsigned short i;
		for(i = 0; i < len; i++) {
			ne_write(NE_DATA, src[i]);
		}
	}
	if(!ne_wait_isr(ISR_RDC, 200000)) return 0;
	ne_write(NE_ISR, ISR_RDC);
	ne_write(NE_CMD, CR_STA | CR_RD2);
	return 1;
}

static void ne_read_prom_mac(unsigned char* mac) {
	unsigned char prom[32];
	unsigned int i;
	for(i = 0; i < 32; i++) prom[i] = 0;

	// Read 32 bytes from station PROM at NIC memory address 0x0000
	(void)ne_remote_read(0x0000, prom, 32);

	// Some cards mirror each MAC byte twice in 16-bit mode.
	{
		int duplicated = 1;
		for(i = 0; i < 6; i++) {
			if(prom[i * 2] != prom[i * 2 + 1]) {
				duplicated = 0;
				break;
			}
		}
		if(duplicated) {
			for(i = 0; i < 6; i++) mac[i] = prom[i * 2];
		} else {
			for(i = 0; i < 6; i++) mac[i] = prom[i];
		}
	}
}

static int ne_configure_runtime(void) {
	unsigned int i;

	// Stop NIC and select page 0
	ne_write(NE_CMD, CR_STP | CR_RD2);

	// Byte-wide FIFO, little-endian, normal operation.
	ne_write(NE_DCR, 0x48);
	ne_write(NE_RBCR0, 0x00);
	ne_write(NE_RBCR1, 0x00);
	ne_write(NE_RCR, 0x20); // Monitor mode during setup
	ne_write(NE_TCR, 0x02); // Internal loopback during setup
	ne_write(NE_TPSR, TX_PAGE_START);
	ne_write(NE_PSTART, RX_PAGE_START);
	ne_write(NE_BNRY, RX_PAGE_START);
	ne_write(NE_PSTOP, RX_PAGE_STOP);
	ne_write(NE_ISR, 0xFF); // Clear all pending IRQ flags
	ne_write(NE_IMR, 0x00); // Mask IRQs for polling driver

	ne_read_prom_mac(g_mac);

	// Program station MAC on page 1.
	ne_write(NE_CMD, CR_STP | CR_RD2 | CR_PS0);
	for(i = 0; i < 6; i++) {
		ne_write((unsigned char)(NE_PAR0 + i), g_mac[i]);
	}
	for(i = 0; i < 8; i++) {
		ne_write((unsigned char)(NE_MAR0 + i), 0xFF); // Accept broadcast/multicast
	}
	ne_write(NE_CURR, RX_PAGE_START + 1);

	// Back to page 0 and start.
	ne_write(NE_CMD, CR_STA | CR_RD2);
	ne_write(NE_TCR, 0x00); // Normal transmit mode
	ne_write(NE_RCR, 0x04); // Accept broadcasts
	ne_write(NE_BNRY, RX_PAGE_START);
	ne_write(NE_ISR, 0xFF);
	ne_write(NE_IMR, ISR_PRX | ISR_RXE | ISR_OVW); // RX IRQs only; TX completion uses polling for stability

	return 1;
}

int ne2k_init(void) {
	unsigned char bus = 0, slot = 0, func = 0;
	unsigned int bar0;
	unsigned int cfg4;
	unsigned int irq_dword;

	g_ready = 0;
	g_io_base = 0;
	g_irq_line = 0;
	g_tx_done = 0;
	g_tx_error = 0;
	g_rx_pending = 0;
	g_irq_enabled = 0;
	g_mac[0] = g_mac[1] = g_mac[2] = g_mac[3] = g_mac[4] = g_mac[5] = 0;

	if(!pci_find_rtl8029(&bus, &slot, &func)) {
		return 0;
	}

	// Enable PCI I/O space and bus mastering.
	cfg4 = pci_config_read_dword(bus, slot, func, 0x04);
	cfg4 |= 0x00000005;
	pci_config_write_dword(bus, slot, func, 0x04, cfg4);

	bar0 = pci_config_read_dword(bus, slot, func, 0x10);
	if((bar0 & 0x1) == 0) {
		return 0; // Unexpected MMIO BAR
	}
	g_io_base = (unsigned short)(bar0 & 0xFFFC);
	if(g_io_base == 0) {
		return 0;
	}

	irq_dword = pci_config_read_dword(bus, slot, func, 0x3C);
	g_irq_line = (unsigned char)(irq_dword & 0xFF);

	// Hardware reset.
	(void)inportb((unsigned short)(g_io_base + NE_RESET));
	outportb((unsigned short)(g_io_base + NE_RESET), 0x00);
	if(!ne_wait_isr(ISR_RST, 400000)) {
		return 0;
	}
	ne_write(NE_ISR, ISR_RST);

	if(!ne_configure_runtime()) {
		return 0;
	}

	// Sanity check MAC.
	{
		unsigned int i;
		int all_zero = 1;
		int all_ff = 1;
		for(i = 0; i < 6; i++) {
			if(g_mac[i] != 0x00) all_zero = 0;
			if(g_mac[i] != 0xFF) all_ff = 0;
		}
		if(all_zero || all_ff) return 0;
	}

	g_ready = 1;
	return 1;
}

int ne2k_is_ready(void) {
	return g_ready;
}

unsigned short ne2k_get_io_base(void) {
	return g_io_base;
}

unsigned char ne2k_get_irq_line(void) {
	return g_irq_line;
}

void ne2k_get_mac(unsigned char* out_mac6) {
	if(!out_mac6) return;
	out_mac6[0] = g_mac[0];
	out_mac6[1] = g_mac[1];
	out_mac6[2] = g_mac[2];
	out_mac6[3] = g_mac[3];
	out_mac6[4] = g_mac[4];
	out_mac6[5] = g_mac[5];
}

int ne2k_send_frame(const unsigned char* data, unsigned short len) {
	unsigned short send_len;
	unsigned int tx_timeout;
	if(!g_ready || !data) return 0;
	if(len == 0 || len > ETH_MAX_FRAME) return 0;

	send_len = len;
	if(send_len < ETH_MIN_FRAME) send_len = ETH_MIN_FRAME;

	if(!ne_remote_write((unsigned short)(TX_PAGE_START << 8), data, len)) {
		return 0;
	}
	if(send_len > len) {
		// Zero-pad to Ethernet minimum frame size
		unsigned char pad[ETH_MIN_FRAME];
		unsigned int i;
		for(i = 0; i < ETH_MIN_FRAME; i++) pad[i] = 0;
		if(!ne_remote_write((unsigned short)((TX_PAGE_START << 8) + len), pad, (unsigned short)(send_len - len))) {
			return 0;
		}
	}

	g_tx_done = 0;
	g_tx_error = 0;
	ne_write(NE_ISR, ISR_PTX | ISR_TXE);
	ne_write(NE_TPSR, TX_PAGE_START);
	ne_write(NE_TBCR0, (unsigned char)(send_len & 0xFF));
	ne_write(NE_TBCR1, (unsigned char)((send_len >> 8) & 0xFF));
	ne_write(NE_CMD, CR_STA | CR_TXP | CR_RD2);

	tx_timeout = 400000;
	while(tx_timeout--) {
		unsigned char isr = ne_read(NE_ISR);
		// Robust completion check:
		// - IRQ-driven path updates g_tx_done/g_tx_error
		// - Fallback directly inspects NIC ISR bits in case IRQ delivery is delayed/masked
		if(g_tx_error || (isr & ISR_TXE)) return 0;
		if(g_tx_done || (isr & ISR_PTX)) {
			ne_write(NE_ISR, ISR_PTX | ISR_TXE);
			return (int)send_len;
		}
	}
	return 0;
}

void ne2k_handle_irq(void) {
	unsigned char isr;
	if(!g_ready) return;

	isr = ne_read(NE_ISR);
	if(isr == 0) return;

	if(isr & (ISR_PRX | ISR_RXE | ISR_OVW)) {
		g_rx_pending = 1;
	}
	// TX completion is handled by polling in ne2k_send_frame().

	// Acknowledge only network completion/error bits.
	ne_write(NE_ISR, (unsigned char)(isr & (ISR_PRX | ISR_PTX | ISR_RXE | ISR_TXE | ISR_OVW | ISR_CNT)));
}

void ne2k_set_irq_enabled(int enabled) {
	if(!g_ready) return;
	if(enabled) {
		g_irq_enabled = 1;
		ne_write(NE_IMR, ISR_PRX | ISR_RXE | ISR_OVW);
	} else {
		g_irq_enabled = 0;
		ne_write(NE_IMR, 0x00);
	}
}

int ne2k_poll_receive(unsigned char* buffer, unsigned short max_len) {
	unsigned char curr;
	unsigned char bnry;
	unsigned char next_packet_page;
	unsigned short packet_len;
	unsigned short payload_len;
	unsigned short copy_len;
	unsigned short packet_addr;
	unsigned short payload_addr;
	unsigned char header[4];
	unsigned char new_bnry;
	if(!g_ready || !buffer || max_len == 0) return -1;

	// Read CURR from page 1.
	ne_write(NE_CMD, CR_STA | CR_RD2 | CR_PS0);
	curr = ne_read(NE_CURR);
	ne_write(NE_CMD, CR_STA | CR_RD2);

	bnry = ne_read(NE_BNRY);
	next_packet_page = (unsigned char)(bnry + 1);
	if(next_packet_page >= RX_PAGE_STOP) next_packet_page = RX_PAGE_START;

	if(next_packet_page == curr) {
		// No packet available in ring.
		g_rx_pending = 0;
		return 0;
	}

	packet_addr = (unsigned short)(next_packet_page << 8);
	if(!ne_remote_read(packet_addr, header, 4)) {
		return -1;
	}

	next_packet_page = header[1];
	packet_len = (unsigned short)(header[2] | ((unsigned short)header[3] << 8));
	if(packet_len < 4) {
		ne_write(NE_ISR, ISR_RXE);
		return -1;
	}
	payload_len = (unsigned short)(packet_len - 4);
	copy_len = payload_len;
	if(copy_len > max_len) copy_len = max_len;

	payload_addr = (unsigned short)(packet_addr + 4);
	if(!ne_remote_read(payload_addr, buffer, copy_len)) {
		return -1;
	}

	// Move boundary to packet before CURR.
	new_bnry = (unsigned char)(next_packet_page - 1);
	if(new_bnry < RX_PAGE_START || new_bnry >= RX_PAGE_STOP) {
		new_bnry = (unsigned char)(RX_PAGE_STOP - 1);
	}
	ne_write(NE_BNRY, new_bnry);
	g_rx_pending = 1;
	return (int)copy_len;
}
