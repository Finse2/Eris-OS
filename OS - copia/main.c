#include "extra.h"
#include "ne2000.h"

#define PIC1_C 0x20
#define PIC1_D 0x21
#define PIC2_C 0xa0
#define PIC2_D 0xa1

#define ICW1_DEF 0x10
#define ICW1_ICW4 0x01
#define ICW4_x86 0x01

void cls();
void setMonitorColor(char);
void setCursor(int x, int y);
void enableCursor();
void disableCursor();
void printCharAt(char c, int x, int y, char color);
char getCharAt(int x, int y);

void printString(char*);
void printChar(char);

void scroll();

void printColorString(char* , char);
void printColorChar(char , char);

// PIT timer state -- 100 Hz tick rate
#define PIT_HZ 100
volatile unsigned int pit_ticks = 0;  // Incremented every tick (100 Hz)

void pit_init();
void initIDT();
extern  void loadIdt();
extern  void isr0_Handler();
extern  void isr1_Handler();
extern  void isr11_Handler();
extern  void isr12_Handler();
extern  void isr14_Handler();
void handleKeypress(int);
void pressed(char);
void handleBackspace();
void picRemap();
void initMouse();

unsigned char inportb(unsigned short);
void outportb(unsigned short , unsigned char);

int CELL;


char COMMAND[256];  // Increased buffer size for longer commands
int i = 0;

// Command history
#define CMD_HISTORY_MAX 16
char cmd_history[CMD_HISTORY_MAX][256];
int history_count = 0;   // Total commands stored (up to CMD_HISTORY_MAX)
int history_browse = -1; // Current browse position (-1 = not browsing)
char cmd_saved[256];     // Saves the in-progress line when browsing starts

// Scrolling terminal buffer (scrollback)
#define SCROLLBACK_LINES  100
#define VGA_LINE_BYTES    160   // 80 columns * 2 bytes (char + attr)
#define VGA_SCREEN_BYTES  4000  // 80 * 25 * 2
#define SCROLL_STEP       3     // Lines per scroll wheel tick

char scrollback_buf[SCROLLBACK_LINES * VGA_LINE_BYTES]; // Ring buffer of saved lines
char screen_backup[VGA_SCREEN_BYTES]; // VGA snapshot when entering scroll view
int scrollback_write = 0;  // Next write slot in ring buffer
int scrollback_count = 0;  // Total lines stored (max SCROLLBACK_LINES)
int scroll_offset = 0;     // 0 = live view, >0 = scrolled back N lines
int cell_backup = 0;       // Backup of CELL when in scroll view

// Extended scancode tracking
static int extended_scancode = 0;

// Shift key state tracking
int shift_pressed = 0;

// Editor mode flag (0 = shell, 1 = editor)
int editor_mode = 0;

// Arrow key codes (extended scancodes after 0xE0)
#define KEY_UP    0x48
#define KEY_DOWN  0x50
#define KEY_LEFT  0x4B
#define KEY_RIGHT 0x4D
#define KEY_ESC   0x01
#define KEY_F2    0x3C
#define KEY_PGUP  0x49
#define KEY_PGDN  0x51

// Shift key scancodes
#define KEY_LSHIFT_PRESS   0x2A
#define KEY_LSHIFT_RELEASE 0xAA
#define KEY_RSHIFT_PRESS   0x36
#define KEY_RSHIFT_RELEASE 0xB6

// Forward declarations for command history helpers
void historySave(char* cmd);
void shellReplaceLine(char* newcmd);

// Forward declarations for scrollback
void scrollViewRedraw();
void scrollPageUp();
void scrollPageDown();
void scrollExitView();

// Editor callbacks (defined in extra.h)
void editorHandleKey(int code, int is_extended, int shift);

struct IDT_ENTRY{
    unsigned short base_Lower;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_Higher;
};

struct IDT_ENTRY idt[256];
extern unsigned int isr0;
extern unsigned int isr1;
extern unsigned int isr11;
extern unsigned int isr12;
extern unsigned int isr14;
unsigned int base0;
unsigned int base;
unsigned int base11;
unsigned int base12;
unsigned int base14;

static void unmaskPicIrq(unsigned char irq_line) {
	if(irq_line < 8) {
		unsigned char mask = inportb(PIC1_D);
		mask = (unsigned char)(mask & (unsigned char)~(1u << irq_line));
		outportb(PIC1_D, mask);
	} else if(irq_line < 16) {
		unsigned char bit = (unsigned char)(irq_line - 8);
		unsigned char mask = inportb(PIC2_D);
		mask = (unsigned char)(mask & (unsigned char)~(1u << bit));
		outportb(PIC2_D, mask);
		// Keep cascade (IRQ2) unmasked on master PIC.
		mask = inportb(PIC1_D);
		mask = (unsigned char)(mask & (unsigned char)~(1u << 2));
		outportb(PIC1_D, mask);
	}
}

static void busyDelay(unsigned int count){
	volatile unsigned int i = 0;
	while(i < count){
		i++;
	}
}

// Mouse driver
static void mouse_wait(unsigned char type){
	int timeout = 100000;
	if(type == 0){
		while(timeout--){
			if((inportb(0x64) & 1) == 1) return;
		}
	} else {
		while(timeout--){
			if((inportb(0x64) & 2) == 0) return;
		}
	}
}

static void mouse_write(unsigned char data){
	mouse_wait(1);
	outportb(0x64, 0xD4);
	mouse_wait(1);
	outportb(0x60, data);
}

static unsigned char mouse_read(){
	mouse_wait(0);
	return inportb(0x60);
}

static unsigned char mouse_cycle = 0;
static unsigned char mouse_bytes[4];  // 4 bytes for Intellimouse (scroll wheel)
static unsigned char mouse_has_wheel = 0; // 1 if scroll wheel detected
int mouse_x = 40;
int mouse_y = 12;
int old_mouse_x = 40;
int old_mouse_y = 12;
unsigned char mouse_buttons = 0;
char saved_char = ' ';
char saved_attr = 0x0f;

static void ProcessMouse(){
	unsigned char data = inportb(0x60);
	
	if(mouse_cycle == 0 && !(data & 0x08)){
		return;
	}
	
	mouse_bytes[mouse_cycle] = data;
	mouse_cycle++;
	
	// Wait for all bytes: 3 for standard, 4 for Intellimouse (scroll wheel)
	int packet_size = mouse_has_wheel ? 4 : 3;
	if(mouse_cycle < packet_size) return;
	mouse_cycle = 0;
	
	mouse_buttons = mouse_bytes[0] & 0x07;
	int dx = (signed char)mouse_bytes[1];
	int dy = (signed char)mouse_bytes[2];

	// Handle scroll wheel (4th byte, Z-axis delta)
	if(mouse_has_wheel) {
		signed char dz = (signed char)mouse_bytes[3];
		if(dz < 0) {
			// Scroll wheel up -- scroll back through history
			scrollPageUp();
		} else if(dz > 0) {
			// Scroll wheel down -- scroll forward / return to live
			scrollPageDown();
		}
	}
	
	// Save old position
	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	// Graphics desktop uses pixel coordinates for smoother interaction.
	if(vgaIsGraphicsMode()) {
		mouse_x += dx;
		mouse_y -= dy;
		if(mouse_x < 0) mouse_x = 0;
		if(mouse_y < 0) mouse_y = 0;
		if(mouse_x >= (int)vesa_width) mouse_x = (int)vesa_width - 1;
		if(mouse_y >= (int)vesa_height) mouse_y = (int)vesa_height - 1;
		return;
	}

	// Text mode path (legacy 80x25 cursor in VGA text memory).
	mouse_x += dx;
	mouse_y -= dy;

	if(mouse_x < 0) mouse_x = 0;
	if(mouse_x > 79) mouse_x = 79;
	if(mouse_y < 0) mouse_y = 0;
	if(mouse_y > 24) mouse_y = 24;

	// Restore character at old position
	char* vga = (char*)0xb8000;
	int old_pos = (old_mouse_y * 80 + old_mouse_x) * 2;
	vga[old_pos] = saved_char;
	vga[old_pos + 1] = saved_attr;

	// Save character at new position and draw cursor
	int new_pos = (mouse_y * 80 + mouse_x) * 2;
	saved_char = vga[new_pos];
	saved_attr = vga[new_pos + 1];

	// Draw mouse cursor (block character with inverted colors)
	vga[new_pos] = 219;  // Full block character
	// Color based on button state: white=none, red=left, blue=right
	if(mouse_buttons & 0x01) {
		vga[new_pos + 1] = 0x4f;
	} else if(mouse_buttons & 0x02) {
		vga[new_pos + 1] = 0x1f;
	} else {
		vga[new_pos + 1] = 0x0f;
	}
}

void initMouse(){
	unsigned char status;
	mouse_wait(1);
	outportb(0x64, 0xA8);
	
	mouse_wait(1);
	outportb(0x64, 0x20);
	mouse_wait(0);
	status = inportb(0x60);
	status |= 2;
	
	mouse_wait(1);
	outportb(0x64, 0x60);
	mouse_wait(1);
	outportb(0x60, status);
	
	mouse_write(0xF6);  // Set defaults
	mouse_read();

	// Enable scroll wheel (Intellimouse protocol)
	// Magic sequence: set sample rate 200, then 100, then 80
	mouse_write(0xF3); mouse_read();  // Set sample rate command
	mouse_write(200);  mouse_read();  // 200 samples/sec
	mouse_write(0xF3); mouse_read();
	mouse_write(100);  mouse_read();  // 100 samples/sec
	mouse_write(0xF3); mouse_read();
	mouse_write(80);   mouse_read();  // 80 samples/sec

	// Query device ID to confirm scroll wheel support
	mouse_write(0xF2);  // Get device ID
	mouse_read();        // ACK
	unsigned char id = mouse_read();
	if(id == 3) {
		mouse_has_wheel = 1;  // Intellimouse detected: 4-byte packets
	}

	// Run at a higher sample rate for smoother desktop cursor motion.
	mouse_write(0xF3); mouse_read();
	mouse_write(200);  mouse_read();

	mouse_write(0xF4);  // Enable mouse
	mouse_read();
}

int start(){
	TM_START = (char*) 0xb8000;
	CELL = 0;
	base0 = (unsigned int)&isr0;
	base = (unsigned int)&isr1;
	base11 = (unsigned int)&isr11;
	base12 = (unsigned int)&isr12;
	base14 = (unsigned int)&isr14;

	cls();
	setMonitorColor(0xa5);

	char Loading[] = "Loading kernel";
	char LoadingDone[] = "Loading complete.\n\n";
	char Welcome[] = "Welcome To ErisOS\n";
	char Welcome2[] = "Command Line\n\n";
	char OSM[] = "OS0 > ";

	// Simple loading animation
	printString(Loading);
	busyDelay(2000000);
	printChar('.');
	busyDelay(2000000);
	printChar('.');
	busyDelay(2000000);
	printChar('.');
	printString("\n");

	printString(Welcome);
	printString(Welcome2);
	
	// Initialize file system (requires hard disk image, not floppy)
	fs_init();
	char fs_init_msg[] = "File system initialized.\n";
	printString(fs_init_msg);
	printString(LoadingDone);

	// Initialize interrupts/IDT first so faults are diagnosable.
	initIDT();
	
	// Initialize identity paging (virtual == physical for first 4 MB)
	paging_init();
	printString("Paging enabled (identity, 4 MB).\n");

	// Safe boot path: stay in text mode by default.
	// Launch desktop explicitly with DESKTOP while we stabilize graphics bring-up.
	printString("Graphics standby. Type DESKTOP to launch UI.\n");

	// Initialize the kernel heap (192 KB at 0x60000-0x8FFFF)
	heap_init();
	printString("Heap initialized (192 KB).\n");

	// Configure PIT to 100 Hz before enabling IRQ0
	pit_init();
	printString("PIT timer enabled (100 Hz).\n");

	if(ne2k_init()) {
		printString("NE2000 (RTL8029) initialized.\n");
		if(ne2k_get_irq_line() == 11) {
			unmaskPicIrq(11);
			ne2k_set_irq_enabled(1);
			printString("NE2000 IRQ-driven RX/TX enabled on IRQ11.\n");
		} else {
			printString("NE2000 IRQ-driven RX/TX not enabled (unsupported IRQ line).\n");
			ne2k_set_irq_enabled(0);
		}
		if(dhcpAcquireLease(0)) {
			printString("DHCP configured network settings.\n");
		} else {
			printString("DHCP lease not obtained; using fallback network settings.\n");
		}
	} else {
		printString("NE2000 (RTL8029) not found.\n");
	}

	// Print shell prompt as the final startup line, with high contrast.
	printColorString(OSM , 0xaf);

	// Keep CPU running and handling interrupts
	while(1) {
		desktopTick();
		__asm__ __volatile__("hlt");
	}
}


void cls(){
	if(scroll_offset > 0) scroll_offset = 0;
	int i = 0;
	CELL = 0;
	while(i < (2 * 80 * 25)){
		*(TM_START + i) = ' '; // Clear screen
		i += 2;
	}
	vesaRedrawTextScreen();
}

void setMonitorColor(char Color){
	int i = 1;
	while(i < (2 * 80 * 25)){
		*(TM_START + i) = Color;
		i += 2;
	}
	vesaRedrawTextScreen();
}

// VGA hardware cursor control
void setCursor(int x, int y) {
	int pos = y * 80 + x;
	outportb(0x3D4, 0x0F);        // Cursor location low register
	outportb(0x3D5, pos & 0xFF);
	outportb(0x3D4, 0x0E);        // Cursor location high register
	outportb(0x3D5, (pos >> 8) & 0xFF);
}

void enableCursor() {
	outportb(0x3D4, 0x0A);        // Cursor start register
	outportb(0x3D5, 0x00);        // Start scanline 0
	outportb(0x3D4, 0x0B);        // Cursor end register
	outportb(0x3D5, 0x0F);        // End scanline 15 (full block)
}

void disableCursor() {
	outportb(0x3D4, 0x0A);
	outportb(0x3D5, 0x20);        // Bit 5 disables cursor
}

// Print character at specific screen position
void printCharAt(char c, int x, int y, char color) {
	int offset = (y * 80 + x) * 2;
	*(TM_START + offset) = c;
	*(TM_START + offset + 1) = color;
	vesaRedrawTextScreen();
}

// Get character at specific screen position
char getCharAt(int x, int y) {
	int offset = (y * 80 + x) * 2;
	return *(TM_START + offset);
}

void printString(char* cA){
	int i = 0;
	while(*(cA + i) != '\0'){
		printChar(*(cA + i));
		i++;
	}
}

void printChar(char c){
	if(CELL == 2 * 80 * 25)
		scroll();
	if(c == '\n'){
		CELL = ((CELL + 160) - (CELL % 160));
		return;
	}
	*(TM_START + CELL) = c;
	CELL += 2;	
	vesaRedrawTextScreen();
}

void scroll(){
	// Save line 0 (the line about to be lost) to the scrollback buffer
	int s;
	int buf_offset = scrollback_write * VGA_LINE_BYTES;
	for(s = 0; s < VGA_LINE_BYTES; s++) {
		scrollback_buf[buf_offset + s] = *(TM_START + s);
	}
	scrollback_write = (scrollback_write + 1) % SCROLLBACK_LINES;
	if(scrollback_count < SCROLLBACK_LINES) scrollback_count++;

	// Shift all lines up by one (copy chars only, matching original behavior)
	int i = 160 , y = 0;
	while(i < 2 * 80 * 25){
		*(TM_START + y) = *(TM_START + i);
		i += 2;
		y += 2;
	}
	CELL = 2 * 80 * 24;
	i = 0;
	while(i < 160){
		*(TM_START + CELL + i) = ' ';
		i += 2;
	}
	vesaRedrawTextScreen();
}

// Redraw the VGA screen from the scrollback buffer + saved screen
// Virtual line numbering:
//   0 .. scrollback_count-1  = scrollback buffer (oldest first)
//   scrollback_count .. +24  = current (live) screen
// Viewport top = scrollback_count - scroll_offset
void scrollViewRedraw() {
	int viewport_top = scrollback_count - scroll_offset;
	int oldest_ring;
	if(scrollback_count < SCROLLBACK_LINES)
		oldest_ring = 0;
	else
		oldest_ring = scrollback_write;  // Oldest entry in ring buffer

	int r, c;
	for(r = 0; r < 25; r++) {
		int vline = viewport_top + r;
		int vga_off = r * VGA_LINE_BYTES;

		if(vline < 0) {
			// Before any content -- blank line
			for(c = 0; c < VGA_LINE_BYTES; c += 2) {
				*(TM_START + vga_off + c) = ' ';
			}
		}
		else if(vline < scrollback_count) {
			// From scrollback buffer
			int ring_idx = (oldest_ring + vline) % SCROLLBACK_LINES;
			int buf_off = ring_idx * VGA_LINE_BYTES;
			for(c = 0; c < VGA_LINE_BYTES; c++) {
				*(TM_START + vga_off + c) = scrollback_buf[buf_off + c];
			}
		}
		else {
			// From saved screen
			int screen_line = vline - scrollback_count;
			if(screen_line >= 0 && screen_line < 25) {
				int src_off = screen_line * VGA_LINE_BYTES;
				for(c = 0; c < VGA_LINE_BYTES; c++) {
					*(TM_START + vga_off + c) = screen_backup[src_off + c];
				}
			} else {
				for(c = 0; c < VGA_LINE_BYTES; c += 2) {
					*(TM_START + vga_off + c) = ' ';
				}
			}
		}
	}

	// Draw scroll indicator at bottom-right: " SCROLL "
	char label[] = " SCROLL ";
	int lpos = 24 * VGA_LINE_BYTES + (80 - 8) * 2; // Bottom-right
	int j;
	for(j = 0; label[j] != '\0'; j++) {
		*(TM_START + lpos + j * 2) = label[j];
		*(TM_START + lpos + j * 2 + 1) = 0x70; // Black on light gray
	}
	vesaRedrawTextScreen();
}

void scrollPageUp() {
	if(scrollback_count == 0) return;

	if(scroll_offset == 0) {
		// Entering scroll view -- snapshot the current VGA screen
		int s;
		for(s = 0; s < VGA_SCREEN_BYTES; s++) {
			screen_backup[s] = *(TM_START + s);
		}
		cell_backup = CELL;
	}

	scroll_offset += SCROLL_STEP;
	if(scroll_offset > scrollback_count) scroll_offset = scrollback_count;

	scrollViewRedraw();
}

void scrollPageDown() {
	if(scroll_offset == 0) return;

	if(scroll_offset <= SCROLL_STEP) {
		// Return to live view -- restore the screen
		scrollExitView();
	} else {
		scroll_offset -= SCROLL_STEP;
		scrollViewRedraw();
	}
}

void scrollExitView() {
	if(scroll_offset == 0) return;
	scroll_offset = 0;

	// Restore the saved VGA screen
	int s;
	for(s = 0; s < VGA_SCREEN_BYTES; s++) {
		*(TM_START + s) = screen_backup[s];
	}
	CELL = cell_backup;
	vesaRedrawTextScreen();
}

void printColorString(char* c , char co){
	int i = 0;
	while(*(c + i) != '\0'){
		printColorChar(*(c + i) , co);
		i++;
	}
}

void printColorChar(char c , char co){
	if(CELL == 2 * 80 * 25)
		scroll();
	if(c == '\n'){
		CELL = ((CELL + 160) - (CELL % 160));
		return;
	}
	*(TM_START + CELL) = c;
	*(TM_START + CELL + 1) = co;
	CELL += 2;	
	vesaRedrawTextScreen();
}

// Initialize the Programmable Interval Timer (PIT) channel 0
// Configures it to fire IRQ0 at PIT_HZ (100 Hz = every 10ms)
void pit_init() {
	unsigned int divisor = 1193182 / PIT_HZ;  // 1193182 Hz base / 100 = 11932
	outportb(0x43, 0x36);   // Channel 0, lobyte/hibyte, mode 3 (square wave)
	outportb(0x40, divisor & 0xFF);         // Low byte
	outportb(0x40, (divisor >> 8) & 0xFF);  // High byte
}

void initIDT(){
	picRemap();

	// Exception 14 - Page Fault (trap gate, 0x8F)
	idt[0x0E].base_Lower = (base14 & 0xFFFF);
	idt[0x0E].base_Higher = (base14 >> 16) & 0xFFFF;
	idt[0x0E].selector = 0x08;
	idt[0x0E].zero = 0;
	idt[0x0E].flags = 0x8F;

	// IRQ0 - PIT Timer (PIC1 at offset 0x20, so IRQ0 = 0x20)
	idt[0x20].base_Lower = (base0 & 0xFFFF);
	idt[0x20].base_Higher = (base0 >> 16) & 0xFFFF;
	idt[0x20].selector = 0x08;
	idt[0x20].zero = 0;
	idt[0x20].flags = 0x8e;

	// IRQ1 - Keyboard (PIC1 at offset 0x20, so IRQ1 = 0x21)
	idt[0x21].base_Lower = (base & 0xFFFF);
	idt[0x21].base_Higher = (base >> 16) & 0xFFFF;
	idt[0x21].selector = 0x08;
	idt[0x21].zero = 0;
	idt[0x21].flags = 0x8e;

	// IRQ11 - NE2000/RTL8029 (PIC2 at offset 0x28, so IRQ11 = 0x2B)
	idt[0x2B].base_Lower = (base11 & 0xFFFF);
	idt[0x2B].base_Higher = (base11 >> 16) & 0xFFFF;
	idt[0x2B].selector = 0x08;
	idt[0x2B].zero = 0;
	idt[0x2B].flags = 0x8e;

	// IRQ12 - Mouse (PIC2 at offset 0x28, so IRQ12 = 0x2C)
	idt[0x2C].base_Lower = (base12 & 0xFFFF);
	idt[0x2C].base_Higher = (base12 >> 16) & 0xFFFF;
	idt[0x2C].selector = 0x08;
	idt[0x2C].zero = 0;
	idt[0x2C].flags = 0x8e;

	// Unmask IRQ0 (timer), IRQ1 (keyboard), IRQ2 (cascade to PIC2)
	outportb(0x21 , 0xf8);  // 0xf8 = 11111000 - IRQ0, IRQ1, IRQ2 enabled
	// Unmask IRQ12 (mouse) on PIC2 - IRQ12 is bit 4
	outportb(0xa1 , 0xef);  // 0xef = 11101111 - IRQ12 enabled

	loadIdt();
	
	// Initialize mouse after IDT is set up
	initMouse();
}

unsigned char inportb(unsigned short _port){
    unsigned char rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

void outportb(unsigned short _port, unsigned char _data){
    __asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

extern void isr0_Handler(){
	pit_ticks++;
	outportb(0x20 , 0x20);  // EOI to PIC1
}

extern void isr1_Handler(){
	handleKeypress(inportb(0x60));
	outportb(0x20 , 0x20);  // EOI to PIC1
}

extern void isr11_Handler(){
	ne2k_handle_irq();
	outportb(0xa0 , 0x20);  // EOI to PIC2
	outportb(0x20 , 0x20);  // EOI to PIC1 (cascade)
}

extern void isr12_Handler(){
	ProcessMouse();
	outportb(0xa0 , 0x20);  // EOI to PIC2
	outportb(0x20 , 0x20);  // EOI to PIC1 (cascade)
}

void handleKeypress(int code){
	char OSM[] = "OS0 > ";
	static int last_code = -1;
	static unsigned int last_tick = 0;
	
	// Normal (unshifted) scancode table - lowercase letters
	char Scancode[] = {
		0 , 0 , '1' , '2' ,
		'3' , '4' , '5' , '6' , 
		'7' , '8' , '9' , '0' , 
		'-' , '=' , 0 , 0 , 'q' , 
		'w' , 'e' , 'r' , 't' , 'y' ,
		'u' , 'i' , 'o' , 'p' , '[' , ']' , 
		0 , 0 , 'a' , 's' , 'd' , 'f' , 'g' , 
		'h' , 'j' , 'k' , 'l' , ';' , '\'' , '`' , 
		0 , '\\' , 'z' , 'x' , 'c' , 'v' , 'b' , 'n' , 'm' ,
		',' , '.' , '/' , 0 , '*' , 0 , ' '
	};
	
	// Shifted scancode table
	char ShiftedScancode[] = {
		0 , 0 , '!' , '@' ,
		'#' , '$' , '%' , '^' , 
		'&' , '*' , '(' , ')' , 
		'_' , '+' , 0 , 0 , 'Q' , 
		'W' , 'E' , 'R' , 'T' , 'Y' ,
		'U' , 'I' , 'O' , 'P' , '{' , '}' , 
		0 , 0 , 'A' , 'S' , 'D' , 'F' , 'G' , 
		'H' , 'J' , 'K' , 'L' , ':' , '"' , '~' , 
		0 , '|' , 'Z' , 'X' , 'C' , 'V' , 'B' , 'N' , 'M' ,
		'<' , '>' , '?' , 0 , '*' , 0 , ' '
	};
	
	// Handle shift key press/release
	if(code == KEY_LSHIFT_PRESS || code == KEY_RSHIFT_PRESS) {
		shift_pressed = 1;
		return;
	}
	if(code == KEY_LSHIFT_RELEASE || code == KEY_RSHIFT_RELEASE) {
		shift_pressed = 0;
		return;
	}
	
	// Handle extended scancode prefix (0xE0)
	if(code == 0xE0) {
		extended_scancode = 1;
		return;
	}

	// Debounce duplicate make codes that arrive in the same tick window.
	// This avoids doubled characters when the controller generates duplicates.
	if(code < 0x80) {
		unsigned int now = pit_ticks;
		if(code == last_code && (now - last_tick) <= 1) {
			return;
		}
		last_code = code;
		last_tick = now;
	}

	// Check if this is an extended key
	int is_extended = extended_scancode;
	extended_scancode = 0;

	// Graphics mode keyboard handling:
	// ESC returns to text shell. Route other keys to desktop UI handlers.
	if(vgaIsGraphicsMode()) {
		if(!is_extended && code == KEY_ESC) {
			vgaExitToShell();
			return;
		}
		desktopHandleKey(code, is_extended);
		return;
	}
	
	// If in editor mode, route all keys to editor
	if(editor_mode) {
		editorHandleKey(code, is_extended, shift_pressed);
		return;
	}
	
	// Normal shell mode

	// Handle extended keys: history (Up/Down) and others
	if(is_extended) {
		// Any extended key exits scroll view first
		if(scroll_offset > 0) scrollExitView();

		if(code == KEY_UP) {
			// Navigate backward in history
			if(history_count == 0) return;

			if(history_browse == -1) {
				// Starting to browse -- save the current in-progress line
				int k;
				for(k = 0; k < i && k < 255; k++) cmd_saved[k] = COMMAND[k];
				cmd_saved[k] = '\0';
				history_browse = history_count;  // One past the last entry
			}

			if(history_browse > 0) {
				history_browse--;
				// Clamp to the oldest available entry
				int oldest = 0;
				if(history_count > CMD_HISTORY_MAX) oldest = history_count - CMD_HISTORY_MAX;
				if(history_browse < oldest) history_browse = oldest;

				int slot = history_browse % CMD_HISTORY_MAX;
				shellReplaceLine(cmd_history[slot]);
			}
			return;
		}
		else if(code == KEY_DOWN) {
			// Navigate forward in history
			if(history_browse == -1) return;  // Not browsing

			history_browse++;
			if(history_browse >= history_count) {
				// Past the newest entry -- restore the saved in-progress line
				history_browse = -1;
				shellReplaceLine(cmd_saved);
			} else {
				int slot = history_browse % CMD_HISTORY_MAX;
				shellReplaceLine(cmd_history[slot]);
			}
			return;
		}
		return;  // Ignore other extended keys in shell mode
	}

	// Exit scroll view on any regular keypress
	if(scroll_offset > 0) scrollExitView();

	if(code == 0x1c){ // Enter key
		// Ensure we're in text mode so command output is visible.
		if(vgaIsGraphicsMode()) {
			vgaEnterTextMode();
		}
		COMMAND[i] = '\0';
		historySave(COMMAND);
		history_browse = -1;  // Reset browse position
		i = 0;
		strEval(COMMAND);
		// If command entered graphics mode (e.g., DESKTOP), keep the desktop visible.
		if(!vgaIsGraphicsMode()) {
			printString("\n");
			printColorString(OSM , 0xaf);
		}
	}
	else if(code == 0x0e){ // Backspace key (press)
		handleBackspace();
	}
	else if(code < 0x3a) {
		// Use shifted or normal scancode based on shift state
		if(shift_pressed)
			pressed(ShiftedScancode[code]);
		else
			pressed(Scancode[code]);
	}
}

void pressed(char key){
	if(key == 0) return; // Ignore null characters
	
	if(i < 255){  // Allow up to 255 characters (leave room for null terminator)
		COMMAND[i] = key;
		i++;
		printChar(key);
	}
	else{
		blink();
	}
}

// Save a command to history (called on Enter)
void historySave(char* cmd) {
	// Don't save empty commands or duplicates of the last entry
	if(cmd[0] == '\0') return;
	if(history_count > 0) {
		int last = (history_count - 1) % CMD_HISTORY_MAX;
		int match = 1;
		int k;
		for(k = 0; k < 256; k++) {
			if(cmd_history[last][k] != cmd[k]) { match = 0; break; }
			if(cmd[k] == '\0') break;
		}
		if(match) return;  // Skip duplicate of most recent command
	}

	int slot = history_count % CMD_HISTORY_MAX;
	int k;
	for(k = 0; k < 255 && cmd[k] != '\0'; k++) {
		cmd_history[slot][k] = cmd[k];
	}
	cmd_history[slot][k] = '\0';
	history_count++;
}

// Erase the current input line on screen and replace it with 'newcmd'
void shellReplaceLine(char* newcmd) {
	// Erase all characters currently on the input line
	while(i > 0) {
		i--;
		if(CELL >= 2) {
			CELL -= 2;
			*(TM_START + CELL) = ' ';
		}
	}

	// Copy the new command into COMMAND and print it
	int k;
	for(k = 0; k < 255 && newcmd[k] != '\0'; k++) {
		COMMAND[k] = newcmd[k];
		printChar(newcmd[k]);
	}
	COMMAND[k] = '\0';
	i = k;
	vesaRedrawTextScreen();
}

void handleBackspace(){
	if(i > 0){
		i--; // Remove last character from buffer
		COMMAND[i] = '\0';
		
		// Move cursor back and erase character on screen
		if(CELL >= 2){
			CELL -= 2; // Move back one character position
			*(TM_START + CELL) = ' '; // Erase character
			// Keep the current color (don't change color attribute)
			// The color is already set from the previous character or prompt
		}
	}
	vesaRedrawTextScreen();
}

void picRemap(){
	// ICW1: Start initialization sequence
	outportb(PIC1_C , ICW1_DEF | ICW1_ICW4);
	outportb(PIC2_C , ICW1_DEF | ICW1_ICW4);

	// ICW2: Remap PIC1 to 0x20-0x27, PIC2 to 0x28-0x2F
	// This avoids conflict with CPU exceptions (0-31)
	outportb(PIC1_D , 0x20);  // PIC1 starts at interrupt 32
	outportb(PIC2_D , 0x28);  // PIC2 starts at interrupt 40

	// ICW3: Tell PICs about each other
	outportb(PIC1_D , 4);     // PIC1: slave on IRQ2
	outportb(PIC2_D , 2);     // PIC2: cascade identity

	// ICW4: 8086 mode
	outportb(PIC1_D , ICW4_x86);
	outportb(PIC2_D , ICW4_x86);

	// OCW1: Mask all interrupts initially
	outportb(PIC1_D , 0xff);
	outportb(PIC2_D , 0xff);
}
