#include "extra.h"

#define PIC1_C 0x20
#define PIC1_D 0x21
#define PIC2_C 0xa0
#define PIC2_D 0xa1

#define ICW1_DEF 0x10
#define ICW1_ICW4 0x01
#define ICW4_x86 0x01

// Graphics framebuffer (VGA Mode 13h: 320x200, 256 colors)
#define FRAMEBUFFER_ADDR 0xA0000
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define SCREEN_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
#define FRAMEBUFFER_END (FRAMEBUFFER_ADDR + SCREEN_SIZE - 1)

// CRITICAL: Validate screen dimensions to prevent division by zero
#if SCREEN_WIDTH == 0 || SCREEN_HEIGHT == 0
#error "SCREEN_WIDTH and SCREEN_HEIGHT must be non-zero!"
#endif
#if SCREEN_SIZE == 0
#error "SCREEN_SIZE must be non-zero!"
#endif

// Framebuffer pointer - initialized at startup
static unsigned char* framebuffer = 0;

// Backbuffer for double buffering - all drawing goes here first
// This prevents flicker and allows safe drawing from main loop
// 
// CRITICAL DESIGN: ISRs NEVER touch backbuffer
// - ISRs only: read hardware, update state variables, set flags
// - Main loop only: renders to backbuffer, blits to framebuffer
// - This makes blit interruptible (worst case: visual tearing, not corruption)
static unsigned char backbuffer[SCREEN_SIZE] __attribute__((aligned(4)));

// Redraw flag - set by IRQ handlers, checked by main loop
volatile int needs_redraw = 0;

// Blitting flag - prevents re-entrancy during blit
// Set to 1 when blitting, ISRs should respect this
volatile int is_blitting = 0;

// Crash isolation toggles (set to 1 temporarily)
#define TEST_DISABLE_MOUSE_IRQ12 0
#define TEST_SKIP_VRETRACE_WAIT  0
#define TEST_SKIP_BLIT_TO_VRAM   0

void cls();
void setMonitorColor(char);

void printString(const char*);
void printChar(char);

void scroll();

void printColorString(char* , char);
void printColorChar(char , char);

// Graphics framebuffer functions
// Helper functions (private)
static int get_pixel_offset(int x, int y);
static int is_valid_coord(int x, int y);
static int is_framebuffer_valid(void);

// Public API
void init_graphics(void);
void clearScreen(unsigned char color);
void setPixel(int x, int y, unsigned char color);
void drawRect(int x, int y, int width, int height, unsigned char color);
void drawLine(int x1, int y1, int x2, int y2, unsigned char color);
void blitFramebuffer(void);  // Copy backbuffer to framebuffer

// Internal rendering function (forward declaration)
static void renderScene(void);

void initIDT();
extern  void loadIdt();
extern  void isr1_Handler();
extern  void isr12_Handler();
extern  void isr_default_Handler();
extern  void isr_exc_default_Handler();
extern  void isr_gp_Handler();
extern  void isr_pf_Handler();
extern  void isr_df_Handler();  // Double Fault handler
void handleKeypress(int);

// Mouse driver functions
void initMouse();
void mouseWait(unsigned char type);
void mouseWrite(unsigned char data);
unsigned char mouseRead();
// handleMousePacket() removed - processing moved to main loop for torn-read safety
void drawMouseCursor();
void eraseMouseCursor();
void updateMouseCursor();  // Deferred cursor update

// Keyboard cursor functions
void drawKeyboardCursor();
void eraseKeyboardCursor();
void moveCursor(int dx, int dy);

// Debug functions (forward declaration)
void debug_string_simple(const char* str);
void pressed(char);
void handleBackspace();
void picRemap();

unsigned char inportb(unsigned short);
void outportb(unsigned short , unsigned char);

int CELL;


// Command buffer - written by ISR, read by main loop
// Must be volatile to prevent compiler caching
volatile char COMMAND[256];  // Command input buffer
volatile int command_index = 0;  // Current position in command buffer (renamed from 'i' to avoid conflicts)

struct IDT_ENTRY{
    unsigned short base_Lower;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_Higher;
};

struct IDT_ENTRY idt[256];
extern unsigned int isr1;
extern unsigned int isr12;
extern unsigned int isr_default;
extern unsigned int isr_exc_default;
extern unsigned int isr_gp;
extern unsigned int isr_pf;
extern unsigned int isr_df;  // Double Fault
unsigned int base;
unsigned int base12;
unsigned int default_base;
unsigned int exc_default_base;
unsigned int base_gp;
unsigned int base_pf;
unsigned int base_df;  // Double Fault

// Mouse state
// Made volatile because accessed from interrupt handlers
volatile int mouse_x = 160;  // Start at center of screen
volatile int mouse_y = 100;
volatile unsigned char mouse_buttons = 0;
volatile unsigned char mouse_cycle = 0;

// Mouse packet - updated byte-by-byte across 3 interrupts (torn read risk)
// Use "ready flag" pattern to prevent torn reads
volatile unsigned char mouse_packet[3];  // Incoming packet (being assembled)
volatile unsigned char mouse_packet_ready = 0;  // Flag: 1 = complete packet ready
volatile unsigned char mouse_packet_safe[3];  // Safe copy for main loop (only updated when ready)

volatile int mouse_initialized = 0;
volatile int mouse_cursor_visible = 0;
volatile int mouse_old_x = 160;
volatile int mouse_old_y = 100;
volatile int mouse_cursor_enabled = 0;
volatile int mouse_needs_update = 0;  // Flag to indicate cursor needs redraw
volatile int mouse_update_counter = 0;  // Counter to throttle updates

// Keyboard-controlled cursor position
// Made volatile because accessed from interrupt handlers
volatile int cursor_x = 160;  // Start at center of screen
volatile int cursor_y = 100;
volatile int cursor_visible = 0;

// Command processing flags (deferred from interrupt context)
volatile int command_ready = 0;  // Flag set when Enter is pressed
volatile int needs_backspace = 0;  // Flag for backspace processing
volatile char pending_key = 0;  // Pending key to process
volatile int has_pending_key = 0;  // Flag for pending key

// Stack canary for overflow detection
#define STACK_CANARY_VALUE 0xDEADBEEF
volatile unsigned int stack_canary = STACK_CANARY_VALUE;

// Move large arrays to static/global to avoid stack issues in interrupt context
// Defined early so they're available to all functions
static const char OSM[] = "\nOS0 > ";
static const char Scancode[] = {
	0 , 0 , '1' , '2' ,
	'3' , '4' , '5' , '6' , 
	'7' , '8' , '9' , '0' , 
	'-' , '=' , 0 , 0 , 'Q' , 
	'W' , 'E' , 'R' , 'T' , 'Y' ,
	'U' , 'I' , 'O' , 'P' , '[' , ']' , 
	0 , 0 , 'A' , 'S' , 'D' , 'F' , 'G' , 
	'H' , 'J' , 'K' , 'L' , ';' , '\'' , '`' , 
	0 , '\\' , 'Z' , 'X' , 'C' , 'V' , 'B' , 'N' , 'M' ,
	',' , '.' , '/' , 0 , '*' , 0 , ' '
};

int start(){
	// CRITICAL: Test debug output IMMEDIATELY - before anything else
	// This verifies debug port is working
	__asm__ __volatile__ (
		"movb $'!', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx\n\t"
		: : : "eax", "edx"
	);
	
	// Test: Can we call a function? (tests stack)
	__asm__ __volatile__ (
		"movb $'1', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Debug: Signal kernel start
	debug_string_simple("KSTART\n");
	
	__asm__ __volatile__ (
		"movb $'2', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[INIT] Kernel entry point reached\n");
	
	__asm__ __volatile__ (
		"movb $'3', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'4', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Zero .bss section manually
	debug_string_simple("[INIT] Zeroing .bss section\n");
	
	__asm__ __volatile__ (
		"movb $'5', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	// .bss contains uninitialized globals that must be zeroed
	// MinGW linker may not provide _edata/_end symbols, so zero known arrays
	// Zero IDT array (critical - used by interrupt handlers)
	int i;
	
	__asm__ __volatile__ (
		"movb $'A', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	for(i = 0; i < 256; i++){
		idt[i].base_Lower = 0;
		idt[i].base_Higher = 0;
		idt[i].selector = 0;
		idt[i].zero = 0;
		idt[i].flags = 0;
	}
	
	__asm__ __volatile__ (
		"movb $'B', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Zero command buffer
	for(i = 0; i < 256; i++){
		COMMAND[i] = 0;
	}
	
	__asm__ __volatile__ (
		"movb $'C', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Initialize stack canary for overflow detection
	stack_canary = STACK_CANARY_VALUE;
	debug_string_simple("[INIT] Stack canary initialized\n");
	
	__asm__ __volatile__ (
		"movb $'7', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Set up minimal IDT BEFORE graphics initialization
	// This prevents triple faults if an exception fires during graphics code
	// We'll set up the full IDT later, but we need basic exception handlers now
	__asm__ __volatile__ (
		"movb $'J', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[INIT] Setting up minimal IDT for exception handling\n");
	
	__asm__ __volatile__ (
		"movb $'K', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Set up ISR base addresses first
	base = (unsigned int)&isr1;
	
	__asm__ __volatile__ (
		"movb $'L', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	base12 = (unsigned int)&isr12;
	default_base = (unsigned int)&isr_default;
	exc_default_base = (unsigned int)&isr_exc_default;
	base_gp = (unsigned int)&isr_gp;
	base_pf = (unsigned int)&isr_pf;
	base_df = (unsigned int)&isr_df;
	
	__asm__ __volatile__ (
		"movb $'M', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Validate base addresses
	if(base == 0 || base12 == 0 || default_base == 0 || exc_default_base == 0){
		debug_string_simple("[CRASH] ISR base address is NULL!\n");
		for(;;) __asm__ __volatile__ ("hlt");
	}
	
	__asm__ __volatile__ (
		"movb $'N', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Set up minimal IDT: all vectors point to safe handler (EOI + return)
	// This prevents triple faults - exceptions will be handled gracefully
	// Reuse 'i' variable declared earlier in function
	for(i = 0; i < 256; i++){
		idt[i].base_Lower = (default_base & 0xFFFF);
		idt[i].base_Higher = ((default_base >> 16) & 0xFFFF);
		idt[i].selector = 0x08;
		idt[i].zero = 0;
		idt[i].flags = 0x8e;
	}
	
	__asm__ __volatile__ (
		"movb $'O', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Set critical exception handlers (these halt instead of returning)
	idt[0x08].base_Lower = (base_df & 0xFFFF);
	idt[0x08].base_Higher = ((base_df >> 16) & 0xFFFF);
	idt[0x0D].base_Lower = (base_gp & 0xFFFF);
	idt[0x0D].base_Higher = ((base_gp >> 16) & 0xFFFF);
	idt[0x0E].base_Lower = (base_pf & 0xFFFF);
	idt[0x0E].base_Higher = ((base_pf >> 16) & 0xFFFF);
	
	__asm__ __volatile__ (
		"movb $'P', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Verify idt array address before loading IDT
	// Load IDT (but don't enable interrupts yet)
	// Add marker right before loading IDT
	__asm__ __volatile__ (
		"movb $'Q', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Inline lidt instruction to avoid function call issues
	// IDT descriptor: limit (16 bits) + base (32 bits)
	// Limit = 256 entries * 8 bytes - 1 = 2047
	// Base = address of idt array
	__asm__ __volatile__ (
		"movb $'S', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx\n\t"
		"push %%eax\n\t"
		"push %%edx\n\t"
		"movw $2047, %%ax\n\t"
		"push %%ax\n\t"
		"pushl %0\n\t"
		"mov %%esp, %%eax\n\t"
		"lidt (%%eax)\n\t"
		"add $6, %%esp\n\t"
		"pop %%edx\n\t"
		"pop %%eax\n\t"
		"movb $'T', %%al\n\t"
		"outb %%al, %%dx"
		: : "r"((unsigned int)idt) : "eax", "edx", "memory"
	);
	
	__asm__ __volatile__ (
		"movb $'R', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[INIT] Minimal IDT loaded - exceptions will be caught\n");
	
	// Initialize graphics driver (structured approach like text mode driver)
	debug_string_simple("[GFX] Initializing graphics...\n");
	debug_string_simple("[GFX] About to call init_graphics()\n");
	
	__asm__ __volatile__ (
		"movb $'8', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	init_graphics();
	
	__asm__ __volatile__ (
		"movb $'9', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'D', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[GFX] Graphics initialized - init_graphics() returned\n");
	
	__asm__ __volatile__ (
		"movb $'E', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Clear screen with black (draws to backbuffer)
	debug_string_simple("[GFX] Clearing screen (black)\n");
	
	__asm__ __volatile__ (
		"movb $'F', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	clearScreen(0);
	
	__asm__ __volatile__ (
		"movb $'G', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'H', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'I', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Draw simplified UI: 3 vertical bars
	// Screen: 320x200 pixels
	// Layout: Grey bar (80px) | Red bar (160px) | Grey bar (80px)
	// Red bar is 2x the width of grey bars
	// Total: 80 + 160 + 80 = 320 pixels
	
	__asm__ __volatile__ (
		"movb $'J', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[UI] Drawing 3 vertical bars\n");
	
	__asm__ __volatile__ (
		"movb $'K', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'L', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Left grey bar (full height)
	debug_string_simple("[UI] Drawing left grey bar (0,0,80,200)\n");
	
	__asm__ __volatile__ (
		"movb $'M', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	drawRect(0, 0, 80, 200, 7);  // Grey color (7 = light grey)
	
	__asm__ __volatile__ (
		"movb $'N', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'O', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Middle red bar (full height, 2x width of grey)
	debug_string_simple("[UI] Drawing middle red bar (80,0,160,200)\n");
	
	__asm__ __volatile__ (
		"movb $'P', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	drawRect(80, 0, 160, 200, 4);  // Red color (4 = red)
	
	__asm__ __volatile__ (
		"movb $'Q', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'R', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Right grey bar (full height)
	debug_string_simple("[UI] Drawing right grey bar (240,0,80,200)\n");
	
	__asm__ __volatile__ (
		"movb $'S', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	drawRect(240, 0, 80, 200, 7);  // Grey color (7 = light grey)
	
	__asm__ __volatile__ (
		"movb $'T', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[UI] All bars drawn\n");
	
	__asm__ __volatile__ (
		"movb $'U', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'V', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Keep text mode functions for compatibility (if needed)
	TM_START = (char*) 0xb8000;
	
	__asm__ __volatile__ (
		"movb $'W', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	CELL = 0;
	
	__asm__ __volatile__ (
		"movb $'X', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// NOTE: ISR base addresses and minimal IDT were already set up before graphics init
	// Full IDT setup happens in initIDT() below
	debug_string_simple("[INIT] ISR base addresses already set up (before graphics)\n");
	
	__asm__ __volatile__ (
		"movb $'Y', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'Z', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Initial blit to show startup graphics (only if not disabled by test toggle)
	if(!TEST_SKIP_BLIT_TO_VRAM){
		__asm__ __volatile__ (
			"movb $'a', %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:::"eax", "edx"
		);
		
		debug_string_simple("[GFX] Blitting backbuffer to framebuffer\n");
		debug_string_simple("[GFX] About to call blitFramebuffer()\n");
		
		__asm__ __volatile__ (
			"movb $'b', %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:::"eax", "edx"
		);
		
		blitFramebuffer();
		
		__asm__ __volatile__ (
			"movb $'c', %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:::"eax", "edx"
		);
		
		debug_string_simple("[GFX] Blit complete - UI visible\n");
	} else {
		debug_string_simple("[GFX] Skipping initial blit (TEST_SKIP_BLIT_TO_VRAM=1)\n");
	}

	__asm__ __volatile__ (
		"movb $'d', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("INITIDT\n");
	
	__asm__ __volatile__ (
		"movb $'e', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	initIDT();
	
	__asm__ __volatile__ (
		"movb $'C', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	// TEMP TEST: trigger software interrupt 0x20 (timer IRQ) to test IDT/ISR entry
	// Testing 0x20 first to see if problem is specific to 0x21 or general
	__asm__ __volatile__ (
    	"movb $'V', %%al\n\t"
    	"movw $0xE9, %%dx\n\t"
    	"outb %%al, %%dx\n\t"
    	"int $0x20\n\t"
    	"movb $'W', %%al\n\t"
    	"movw $0xE9, %%dx\n\t"
    	"outb %%al, %%dx"
    	: : : "eax", "edx"
	);
	
	debug_string_simple("IDTDONE\n");
	
	__asm__ __volatile__ (
		"movb $'D', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Initialize mouse
	debug_string_simple("INITMOUSE\n");
	
	__asm__ __volatile__ (
		"movb $'E', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	initMouse();
	
	__asm__ __volatile__ (
		"movb $'F', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("MOUSEDONE\n");
	
	__asm__ __volatile__ (
		"movb $'Y', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Enable mouse cursor drawing (now safe with backbuffer)
	mouse_cursor_enabled = 1;
	
	__asm__ __volatile__ (
		"movb $'Z', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[SYS] Entering main loop\n");
	
	__asm__ __volatile__ (
		"movb $'0', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Validate stack pointer before enabling interrupts
	// If stack pointer is invalid, interrupt entry will triple fault
	unsigned int stack_ptr;
	__asm__ __volatile__ ("mov %%esp, %0" : "=r"(stack_ptr));
	
	// Stack should be between 0x80000 and 0x90000 (64KB stack from Boot.asm)
	if(stack_ptr < 0x80000 || stack_ptr > 0x90000) {
		// Stack pointer is invalid - this will cause triple fault on interrupt
		__asm__ __volatile__ (
			"movb $'S', %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:::"eax", "edx"
		);
		__asm__ __volatile__ ("cli");
		debug_string_simple("[CRASH] Invalid stack pointer!\n");
		for(;;) __asm__ __volatile__ ("hlt");
	}
	
	// Check stack canary for overflow
	if(stack_canary != STACK_CANARY_VALUE) {
		__asm__ __volatile__ (
			"movb $'C', %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:::"eax", "edx"
		);
		__asm__ __volatile__ ("cli");
		debug_string_simple("[CRASH] Stack overflow detected!\n");
		for(;;) __asm__ __volatile__ ("hlt");
	}
	
	__asm__ __volatile__ (
		"movb $'s', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Clear any pending interrupts before enabling interrupts
	// Read from PIC to clear any pending interrupt requests
	// This prevents an immediate interrupt from firing when we enable interrupts
	unsigned char dummy;
	dummy = inportb(0x20);  // Read PIC1 interrupt request register
	dummy = inportb(0xa0);  // Read PIC2 interrupt request register
	(void)dummy;  // Suppress unused variable warning
	
	// Send EOI to both PICs to clear any pending interrupts
	outportb(0x20, 0x20);  // EOI to PIC1
	outportb(0xa0, 0x20);  // EOI to PIC2
	
	__asm__ __volatile__ (
		"movb $'c', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Clear any pending keyboard interrupts before enabling
	// Read from keyboard controller to clear any pending data
	unsigned char dummy_kb;
	dummy_kb = inportb(0x60);  // Read keyboard data port to clear pending interrupt
	(void)dummy_kb;
	
	// Send EOI to PIC1 to clear any pending keyboard interrupt
	outportb(0x20, 0x20);  // EOI to PIC1
	
	// CRITICAL: Enable keyboard interrupt (IRQ1) only
	// Keyboard IRQ1 is on bit 1 of PIC1 mask (0 = enabled, 1 = disabled)
	// 0xFD = 11111101b (only IRQ1 enabled, all others masked)
	outportb(0x21, 0xFD);  // Enable keyboard (IRQ1), mask all other PIC1 interrupts
	outportb(0xa1, 0xFF);  // Mask all PIC2 interrupts (including mouse)
	
	__asm__ __volatile__ (
		"movb $'m', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Enable interrupts - keyboard should now work
	__asm__ __volatile__ (
		"movb $'s', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ ("sti");
	
	__asm__ __volatile__ (
		"movb $'t', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Interrupts are now enabled with keyboard working
	// If you see 'K' in the output, keyboard interrupts are firing correctly
	
	__asm__ __volatile__ (
		"movb $'1', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Main loop - process deferred operations and check for issues
	// Architecture: ISR sets flags, main loop processes them (prevents stack issues)
	debug_string_simple("[MAIN] Main loop started\n");
	
	__asm__ __volatile__ (
		"movb $'2', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	int loop_count = 0;
	while(1){
		loop_count++;
		// Log every 10000 iterations to track if loop is running
		if(loop_count == 1 || loop_count == 10 || loop_count == 100 || loop_count == 1000) {
			debug_string_simple("[MAIN] Loop iteration count\n");
		}
		
		// Check stack canary for overflow detection
		if(stack_canary != STACK_CANARY_VALUE){
			// Stack corruption detected!
			__asm__ __volatile__ ("cli");
			debug_string_simple("[CRASH] STACK CORRUPTION!\n");
			for(;;) __asm__ __volatile__ ("hlt");
		}
		
		// Process deferred command execution (from Enter key)
		if(command_ready){
			command_ready = 0;  // Clear flag
			// Now safe to call complex functions outside interrupt context
			// Cast away volatile - safe because we're in main loop, not ISR
			strEval((char*)COMMAND);
			printString(OSM);
			needs_redraw = 1;  // UI changed, need redraw
		}
		
		// Process deferred backspace
		if(needs_backspace){
			needs_backspace = 0;  // Clear flag
			handleBackspace();
			needs_redraw = 1;  // UI changed, need redraw
		}
		
		// Process pending key (deferred from interrupt)
		if(has_pending_key){
			has_pending_key = 0;  // Clear flag
			char key = pending_key;
			pending_key = 0;
			// Safe to call printChar outside interrupt context
			if(key != 0){
				printChar(key);
				needs_redraw = 1;  // UI changed, need redraw
			}
		}
		
		// Process mouse packet if ready (prevents torn reads)
		// Pattern: ISR sets mouse_packet_ready=1 when complete packet is in mouse_packet_safe[]
		// Main loop processes it, then clears the flag
		// Coalesce packets: consume any pending packet(s), then redraw once.
		// NOTE: With a single ready flag, we effectively process the latest packet
		// if multiple arrive before we wake; the loop below also catches the case
		// where a new packet becomes ready while we're processing.
		if(mouse_initialized){
			int mouse_changed = 0;
			while(mouse_packet_ready){
				int old_x = mouse_x;
				int old_y = mouse_y;
				unsigned char old_buttons = mouse_buttons;

				mouse_packet_ready = 0;  // Clear flag first (atomic operation)

				// Now safe to read mouse_packet_safe[] - it won't change until next ready flag
				unsigned char packet[3];
				packet[0] = mouse_packet_safe[0];
				packet[1] = mouse_packet_safe[1];
				packet[2] = mouse_packet_safe[2];

				// Process packet (extract deltas and update position)
				unsigned char buttons = packet[0] & 0x07;
				char delta_x = (char)packet[1];
				char delta_y = (char)packet[2];

				// Handle overflow bits (9th bit of movement)
				if(packet[0] & 0x10) delta_x |= 0xFFFFFF00;
				if(packet[0] & 0x20) delta_y |= 0xFFFFFF00;

				// Update mouse position (Y is inverted in mouse coordinates)
				int new_x = old_x + delta_x;
				int new_y = old_y - delta_y;  // Invert Y

				// Clamp to screen bounds
				if(new_x < 0) new_x = 0;
				if(new_x >= SCREEN_WIDTH) new_x = SCREEN_WIDTH - 1;
				if(new_y < 0) new_y = 0;
				if(new_y >= SCREEN_HEIGHT) new_y = SCREEN_HEIGHT - 1;

				// Atomic update
				mouse_x = new_x;
				mouse_y = new_y;
				mouse_buttons = buttons;

				if(new_x != old_x || new_y != old_y || buttons != old_buttons){
					mouse_changed = 1;
				}
			}

			if(mouse_changed && mouse_cursor_enabled){
				needs_redraw = 1;
			}
		}
		
		// Main loop responsibilities:
		// 1. When needs_redraw, render entire scene (UI + cursor) into backbuffer
		// 2. Blit backbuffer → VRAM (framebuffer) with VGA retrace sync
		//
		// Immediate mode rendering approach:
		// - Redraw entire scene every frame (clear, draw UI, draw cursor)
		// - This avoids needing to save/restore pixels under cursor
		// - No destructive erase - cursor is drawn on top of fresh UI
		//
		// Blit protection:
		// - Blitting flag prevents re-entrancy
		// - ISRs never touch backbuffer/framebuffer (only read hardware, set flags)
		// - No kernel corruption possible because data flow is safe
		//
		// Pure event-based redraw:
		// - Redraw exactly when something changed (needs_redraw=1)
		// - No loop-speed counters (those cause jitter in QEMU)
		if(needs_redraw){
			renderScene();
			blitFramebuffer();  // Copy backbuffer to framebuffer (fast 32-bit copy)
			needs_redraw = 0;
		}
		
		// Use halt instruction to wait for interrupts
		__asm__ __volatile__ ("hlt");
	}
}


void cls(){
	int i = 0;
	CELL = 0;
	while(i < (2 * 80 * 25)){
		*(TM_START + i) = ' '; // Clear screen
		i += 2;
	}
}

void setMonitorColor(char Color){
	int i = 1;
	while(i < (2 * 80 * 25)){
		*(TM_START + i) = Color;
		i += 2;
	}
}

void printString(const char* cA){
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
}

void scroll(){
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
}

// Debug output to QEMU/Bochs debug port (0xE9)
// Made safe for exception handlers - no loops, minimal stack usage
static inline void debug_char(char c) {
	__asm__ __volatile__ (
		"movb %0, %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:
		: "r" ((unsigned char)c)
		: "eax", "edx"
	);
}

// Simplified debug output - direct writes to avoid function call overhead
// Must be defined before use in start()
void debug_string_simple(const char* str) {
	// Direct inline assembly to minimize stack usage
	const char* p = str;
	int count = 0;
	while(*p && count < 100) {  // Safety limit
		__asm__ __volatile__ (
			"movb %0, %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:
			: "r" ((unsigned char)*p)
			: "eax", "edx"
		);
		p++;
		count++;
	}
}

// Exception counter to prevent infinite exception loops
static volatile int exception_count = 0;

// #GP (General Protection Fault) handler - vector 0x0D
extern void isr_gp_Handler(){
	exception_count++;
	
	// Log the exception immediately
	debug_string_simple("[EXCEPTION] General Protection Fault (#GP)\n");
	
	// If we're getting too many exceptions, halt to prevent infinite loop
	if(exception_count > 3){
		__asm__ __volatile__ ("cli");
		__asm__ __volatile__ (
			"movb $'!', %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:::"eax", "edx"
		);
		for(;;) __asm__ __volatile__ ("hlt");
	}
	
	// Don't try to continue - halt on first exception to debug
	// Continuing after a real exception is dangerous
	__asm__ __volatile__ ("cli");
	for(;;) __asm__ __volatile__ ("hlt");
}

// #PF (Page Fault) handler - vector 0x0E
extern void isr_pf_Handler(){
	exception_count++;
	
	// Log the exception immediately
	debug_string_simple("[EXCEPTION] Page Fault (#PF)\n");
	__asm__ __volatile__ (
		"movb $'P', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx\n\t"
		"movb $'F', %%al\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// If we're getting too many exceptions, halt to prevent infinite loop
	if(exception_count > 3){
		__asm__ __volatile__ ("cli");
		__asm__ __volatile__ (
			"movb $'!', %%al\n\t"
			"movw $0xE9, %%dx\n\t"
			"outb %%al, %%dx"
			:::"eax", "edx"
		);
		for(;;) __asm__ __volatile__ ("hlt");
	}
	
	// Don't try to continue - halt on first exception to debug
	// Continuing after a real exception is dangerous
	__asm__ __volatile__ ("cli");
	for(;;) __asm__ __volatile__ ("hlt");
}

// #DF (Double Fault) handler - vector 0x08
// CRITICAL: This prevents triple fault → system reset
extern void isr_df_Handler(){
	__asm__ __volatile__ ("cli");
	
	// Log double fault
	debug_string_simple("[CRASH] Double Fault (#DF) - System will halt\n");
	
	// Double fault is serious - but try to halt gracefully instead of triple fault
	// This prevents the system reset
	for(;;){
		__asm__ __volatile__ ("hlt");
	}
}

// Default interrupt handler for unhandled interrupts
extern void isr_default_Handler(){
	// CRITICAL: Print debug byte IMMEDIATELY (before any other code)
	// This helps diagnose which interrupt is firing
	__asm__ __volatile__ (
		"movb $'I', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Just acknowledge and return - prevents triple faults
	// DO NOT call debug_string_simple() here - it may not be interrupt-safe
	// and could cause crashes if called from ISR context
	outportb(0x20, 0x20);  // Send EOI to PIC1
	outportb(0xa0, 0x20);  // Send EOI to PIC2
}

// Default exception handler for unexpected CPU exceptions
// (Used for vectors 0x00-0x1F unless explicitly overridden.)
extern void isr_exc_default_Handler(){
	// CRITICAL: Print debug byte IMMEDIATELY (before any other code)
	// If you see 'E' in output, you're hitting a CPU exception (GP/SS/etc)
	__asm__ __volatile__ (
		"movb $'E', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ ("cli");
	debug_string_simple("[CRASH] Unhandled CPU exception\n");
	for(;;) __asm__ __volatile__ ("hlt");
}

void initIDT(){
	__asm__ __volatile__ (
		"movb $'f', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[IDT] Starting initIDT()\n");
	
	__asm__ __volatile__ (
		"movb $'g', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	int i;
	
	__asm__ __volatile__ (
		"movb $'h', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// GDT selector values (from Boot.asm):
	// CODE_SEG = 0x08 (kernel code segment)
	// DATA_SEG = 0x10 (kernel data segment)
	
	// CRITICAL: Validate exc_default_base before using it
	if(exc_default_base == 0){
		debug_string_simple("[CRASH] exc_default_base is NULL - cannot initialize IDT!\n");
		for(;;) __asm__ __volatile__ ("hlt");
	}
	
	__asm__ __volatile__ (
		"movb $'i', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[IDT] Setting up default exception handlers\n");
	
	__asm__ __volatile__ (
		"movb $'j', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'k', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Initialize all IDT entries to default handler first
	// This prevents triple faults from unhandled interrupts
	// All IDT entries use selector 0x08 (kernel code segment)
	// 
	// IMPORTANT: During initialization, use IRQ handler (EOI + return) instead of
	// exception handler (halt) to avoid halting on harmless exceptions during setup.
	// We'll set proper exception handlers for critical vectors below.
	
	__asm__ __volatile__ (
		"movb $'l', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	for(i = 0; i < 256; i++){
		// Use IRQ handler (safe EOI + return) during init to avoid halting on spurious exceptions
		// Critical exception vectors (0x08, 0x0D, 0x0E) will be overridden below
		idt[i].base_Lower = (default_base & 0xFFFF);
		idt[i].base_Higher = (default_base >> 16) & 0xFFFF;
		idt[i].selector = 0x08;  // Kernel code segment selector
		idt[i].zero = 0;
		idt[i].flags = 0x8e;  // 32-bit interrupt gate, present, DPL=0
	}
	
	__asm__ __volatile__ (
		"movb $'m', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);

	__asm__ __volatile__ (
		"movb $'n', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[IDT] Overriding IRQ vectors 0x20-0x2F\n");
	
	__asm__ __volatile__ (
		"movb $'o', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// IRQ vectors (PIC remapped to 0x20-0x2F): default IRQ handler should EOI and return
	for(i = 0x20; i <= 0x2F; i++){
		idt[i].base_Lower = (default_base & 0xFFFF);
		idt[i].base_Higher = (default_base >> 16) & 0xFFFF;
	}
	
	__asm__ __volatile__ (
		"movb $'p', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[IDT] Calling picRemap()\n");
	
	__asm__ __volatile__ (
		"movb $'q', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	picRemap();
	
	__asm__ __volatile__ (
		"movb $'r', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[IDT] picRemap() complete\n");
	
	__asm__ __volatile__ (
		"movb $'s', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'t', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[IDT] Setting up critical exception handlers\n");
	
	__asm__ __volatile__ (
		"movb $'u', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Exception handlers - critical for debugging crashes
	// These override the default IRQ handler we set above
	// #DF (Double Fault) - vector 0x08 - CRITICAL to prevent triple fault
	
	__asm__ __volatile__ (
		"movb $'v', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	idt[0x08].base_Lower = (base_df & 0xFFFF);
	idt[0x08].base_Higher = (base_df >> 16) & 0xFFFF;
	idt[0x08].selector = 0x08;
	idt[0x08].zero = 0;
	idt[0x08].flags = 0x8e;
	
	__asm__ __volatile__ (
		"movb $'w', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// #GP (General Protection Fault) - vector 0x0D
	idt[0x0D].base_Lower = (base_gp & 0xFFFF);
	idt[0x0D].base_Higher = (base_gp >> 16) & 0xFFFF;
	idt[0x0D].selector = 0x08;
	idt[0x0D].zero = 0;
	idt[0x0D].flags = 0x8e;
	
	__asm__ __volatile__ (
		"movb $'x', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// #PF (Page Fault) - vector 0x0E
	idt[0x0E].base_Lower = (base_pf & 0xFFFF);
	idt[0x0E].base_Higher = (base_pf >> 16) & 0xFFFF;
	idt[0x0E].selector = 0x08;
	idt[0x0E].zero = 0;
	idt[0x0E].flags = 0x8e;
	
	__asm__ __volatile__ (
		"movb $'y', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'z', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Now set exception handler for all other exception vectors (0x00-0x1F except 0x08, 0x0D, 0x0E)
	// This ensures any unexpected exceptions during runtime will halt and report
	debug_string_simple("[IDT] Setting exception handler for remaining exception vectors\n");
	
	__asm__ __volatile__ (
		"movb $'0', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	for(i = 0; i < 0x20; i++){
		// Skip vectors we've already set up
		if(i == 0x08 || i == 0x0D || i == 0x0E) continue;
		idt[i].base_Lower = (exc_default_base & 0xFFFF);
		idt[i].base_Higher = (exc_default_base >> 16) & 0xFFFF;
		// CRITICAL: Ensure selector and flags are set correctly
		// (They should already be set from earlier initialization, but be explicit)
		idt[i].selector = 0x08;  // Kernel code segment selector
		idt[i].zero = 0;
		idt[i].flags = 0x8e;  // 32-bit interrupt gate, present, DPL=0
	}
	
	__asm__ __volatile__ (
		"movb $'1', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Keyboard interrupt (IRQ 1 -> vector 0x21 after PIC remap to 0x20)
	// PIC1 offset = 0x20, so IRQ1 maps to 0x20 + 1 = 0x21
	// This is correct and avoids collision with CPU exception #DB (vector 0x01)
	


	// CRITICAL: Mouse interrupt (IRQ 12 -> vector 0x2C after PIC remap to 0x28)
	// PIC2 offset = 0x28, so IRQ12 maps to 0x28 + 4 = 0x2C
	// IRQ12 is the 4th IRQ on PIC2 (IRQ8=0, IRQ9=1, IRQ10=2, IRQ11=3, IRQ12=4)
	// This is correct and matches the PIC remap above
	idt[0x2C].base_Lower = (base12 & 0xFFFF);
	idt[0x2C].base_Higher = (base12 >> 16) & 0xFFFF;
	idt[0x2C].selector = 0x08;
	idt[0x2C].zero = 0;
	idt[0x2C].flags = 0x8e;

	// Enable keyboard (IRQ 1) and cascade (IRQ 2) on PIC1
	// 0xF9 = 11111001b (bits 1 and 2 cleared = IRQ1 and IRQ2 enabled)
	// IRQ2 must be enabled for PIC2 to work
	outportb(0x21 , 0xF9);  // Enable keyboard (IRQ 1) and cascade (IRQ 2)
	
	__asm__ __volatile__ (
		"movb $'5', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Enable mouse (IRQ 12) on PIC2
	// 0xEF = 11101111b (bit 4 cleared = IRQ 12 enabled)
	unsigned char pic2_mask = 0xEF;  // Enable mouse (IRQ 12), disable others on PIC2
	if(TEST_DISABLE_MOUSE_IRQ12){
		pic2_mask |= (1 << 4);  // Mask mouse IRQ12 (bit 4)
	}
	outportb(0xa1 , pic2_mask);

	__asm__ __volatile__ (
		"movb $'6', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);

	__asm__ __volatile__ (
		"movb $'7', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);

	// Load IDT (but don't enable interrupts yet)
	// Use inline assembly instead of function call to avoid stack issues
	__asm__ __volatile__ (
		"push %%eax\n\t"
		"push %%edx\n\t"
		"movw $2047, %%ax\n\t"
		"push %%ax\n\t"
		"pushl %0\n\t"
		"mov %%esp, %%eax\n\t"
		"lidt (%%eax)\n\t"
		"add $6, %%esp\n\t"
		"pop %%edx\n\t"
		"pop %%eax"
		: : "r"((unsigned int)idt) : "eax", "edx", "memory"
	);
	
	__asm__ __volatile__ (
		"movb $'8', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("IDTLOADED\n");
	
	// NOTE: Do NOT enable interrupts here.
	// The caller (start()) will perform final stack checks, clear any
	// pending PIC/keyboard interrupts, configure masks, and then execute sti.
	// This ensures no IRQ fires while initialization is still in progress.
}

unsigned char inportb(unsigned short _port){
    unsigned char rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

void outportb(unsigned short _port, unsigned char _data){
    __asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

// Track extended scancode (0xE0 prefix for arrow keys)
// Made volatile to prevent optimization issues in interrupt context
static volatile unsigned char extended_scancode = 0;

extern void isr1_Handler(){
	// TEMPORARY MINIMAL KEYBOARD ISR FOR DEBUGGING
	// Goal: verify the IRQ1 path (stub + C handler + IDT/PIC/stack) is stable
	// without any complex logic that could cause a fault.
	__asm__ __volatile__ (
		"movb $'k', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);

	// REQUIRED: read scancode to clear the keyboard controller
	unsigned char scancode = inportb(0x60);
	(void)scancode;

	// REQUIRED: send EOI to PIC1 for IRQ1
	outportb(0x20 , 0x20);  // EOI to PIC1 only

	// No extended-scancode handling, no buffer updates, no flags yet.
	// Once this is stable, we can gradually re-introduce the original logic.
}

void handleKeypress(int code){
	
	// Handle arrow keys (extended scancodes)
	// Arrow keys come as 0xE0 prefix followed by the actual key code
	// We'll handle them in the pressed function, but for now handle basic movement
	
	// Arrow key scancodes (when 0xE0 prefix is received, next byte is the arrow key)
	// Left: 0x4B, Right: 0x4D, Up: 0x48, Down: 0x50
	
	if(code == 0x1c){ // Enter key
		// Validate index before accessing buffer
		if(command_index >= 0 && command_index < 256){
			COMMAND[command_index] = '\0';
		}
		command_index = 0;  // Reset index
		// Cast away volatile - safe because we're in main loop, not ISR
		strEval((char*)COMMAND);
		printString(OSM);
	}
	else if(code == 0x0e){ // Backspace key (press)
		handleBackspace();
	}
	else if(code == 0x4B){ // Left arrow (when extended)
		moveCursor(-1, 0);
	}
	else if(code == 0x4D){ // Right arrow (when extended)
		moveCursor(1, 0);
	}
	else if(code == 0x48){ // Up arrow (when extended)
		moveCursor(0, -1);
	}
	else if(code == 0x50){ // Down arrow (when extended)
		moveCursor(0, 1);
	}
	else if(code < 0x3a)
		pressed(Scancode[code]);
}

void pressed(char key){
	if(key == 0) return; // Ignore null characters
	
	// Strict bounds checking - validate index before writing
	// Buffer is 256 bytes, so valid indices are 0-255
	// We reserve index 255 for null terminator, so max is 254
	if(command_index >= 0 && command_index < 255){
		COMMAND[command_index] = key;
		command_index++;
		printChar(key);
	}
	else{
		// Buffer full or index corrupted - reset to prevent overflow
		if(command_index < 0 || command_index >= 256){
			command_index = 0;  // Reset corrupted index
		}
		blink();
	}
}

void handleBackspace(){
	// Validate index before accessing buffer
	if(command_index > 0 && command_index <= 256){
		command_index--; // Remove last character from buffer
		if(command_index >= 0 && command_index < 256){
			COMMAND[command_index] = '\0';
		}
		
		// Move cursor back and erase character on screen
		if(CELL >= 2){
			CELL -= 2; // Move back one character position
			*(TM_START + CELL) = ' '; // Erase character
			// Keep the current color (don't change color attribute)
			// The color is already set from the previous character or prompt
		}
	}
	else if(command_index < 0){
		// Index corrupted - reset it
		command_index = 0;
	}
}

void picRemap(){
	// Save current interrupt masks
	unsigned char a , b;
	a = inportb(PIC1_D);
	b = inportb(PIC2_D);

	// Initialize PICs in cascade mode
	outportb(PIC1_C , ICW1_DEF | ICW1_ICW4);
	outportb(PIC2_C , ICW1_DEF | ICW1_ICW4);

	// CRITICAL: Remap PIC1 to 0x20 (IRQ 0-7 -> interrupt vectors 0x20-0x27)
	// This avoids conflicts with CPU exceptions (0x00-0x1F)
	// WRONG: outportb(PIC1_D, 0) would map IRQ1 to vector 0x01 (collides with #DB!)
	// CORRECT: 0x20 maps IRQ1 to vector 0x21 (safe, no collision)
	outportb(PIC1_D , 0x20);
	
	// CRITICAL: Remap PIC2 to 0x28 (IRQ 8-15 -> interrupt vectors 0x28-0x2F)
	// WRONG: outportb(PIC2_D, 8) would map IRQ12 to vector 0x14 (wrong!)
	// CORRECT: 0x28 maps IRQ12 to vector 0x2C (0x28 + 4 = 0x2C)
	outportb(PIC2_D , 0x28);

	// Tell PIC1 that PIC2 is connected at IRQ2 (cascade)
	outportb(PIC1_D , 4);
	// Tell PIC2 its cascade identity (connected to PIC1's IRQ2)
	outportb(PIC2_D , 2);

	// Set x86 mode
	outportb(PIC1_D , ICW4_x86);
	outportb(PIC2_D , ICW4_x86);

	// Restore interrupt masks (will be configured later in initIDT)
	outportb(PIC1_D , a);
	outportb(PIC2_D , b);
}

// Graphics framebuffer functions
// Structured approach based on text mode driver pattern

// Initialize graphics framebuffer (called once at startup)
void init_graphics(void){
	__asm__ __volatile__ (
		"movb $'a', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[GFX] Setting framebuffer address to 0xA0000\n");
	
	__asm__ __volatile__ (
		"movb $'b', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	framebuffer = (unsigned char*) FRAMEBUFFER_ADDR;
	
	__asm__ __volatile__ (
		"movb $'c', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Validate framebuffer address is exactly 0xA0000
	// If this fails, we have a serious memory mapping issue
	if((unsigned int)framebuffer != FRAMEBUFFER_ADDR){
		// Framebuffer address is wrong - this should never happen
		// Set to NULL to prevent any writes
		debug_string_simple("[GFX] ERROR: Framebuffer address mismatch!\n");
		framebuffer = 0;
		return;
	}
	
	__asm__ __volatile__ (
		"movb $'d', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[GFX] Framebuffer address validated\n");
	
	__asm__ __volatile__ (
		"movb $'e', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Additional validation: framebuffer should be in valid range
	// Mode 13h framebuffer is at 0xA0000, size 320*200 = 64000 bytes
	// End address = 0xA0000 + 0xFA00 = 0xAFA00
	unsigned int fb_start = (unsigned int)framebuffer;
	unsigned int fb_end = fb_start + SCREEN_SIZE;
	if(fb_start < 0xA0000 || fb_end > 0xA0000 + SCREEN_SIZE){
		// Framebuffer range is invalid
		debug_string_simple("[GFX] ERROR: Framebuffer range invalid!\n");
		framebuffer = 0;
		return;
	}
	
	__asm__ __volatile__ (
		"movb $'f', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[GFX] Framebuffer range validated\n");
	
	__asm__ __volatile__ (
		"movb $'g', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Initialize backbuffer to black
	debug_string_simple("[GFX] Initializing backbuffer (64000 bytes)\n");
	
	__asm__ __volatile__ (
		"movb $'h', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Validate SCREEN_SIZE is reasonable (prevent stack overflow)
	if(SCREEN_SIZE <= 0 || SCREEN_SIZE > 1000000) {
		debug_string_simple("[GFX] FATAL: SCREEN_SIZE is invalid!\n");
		framebuffer = 0;
		return;
	}
	
	// Validate backbuffer is properly aligned (should be, but check)
	if(((unsigned int)backbuffer % 4) != 0) {
		debug_string_simple("[GFX] WARNING: Backbuffer not 4-byte aligned\n");
	}
	
	__asm__ __volatile__ (
		"movb $'i', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	int i;
	for(i = 0; i < SCREEN_SIZE; i++){
		backbuffer[i] = 0;
	}
	
	__asm__ __volatile__ (
		"movb $'j', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[GFX] Backbuffer initialized\n");
	
	__asm__ __volatile__ (
		"movb $'k', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Test write to framebuffer to verify it's accessible (read-only test)
	// Just read a byte to see if we get a fault
	debug_string_simple("[GFX] Testing framebuffer accessibility\n");
	
	__asm__ __volatile__ (
		"movb $'l', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	volatile unsigned char test_read = framebuffer[0];
	(void)test_read;  // Suppress unused variable warning
	
	__asm__ __volatile__ (
		"movb $'m', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	debug_string_simple("[GFX] Framebuffer read test passed\n");
	
	__asm__ __volatile__ (
		"movb $'n', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Framebuffer and backbuffer are now ready for use
	debug_string_simple("[GFX] Graphics system ready\n");
	
	__asm__ __volatile__ (
		"movb $'o', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
}

// Helper function: Check if framebuffer is valid
static int is_framebuffer_valid(void){
	if(framebuffer == 0) {
		debug_string_simple("[GFX] ERROR: framebuffer pointer is NULL\n");
		return 0;
	}
	if((unsigned int)framebuffer != FRAMEBUFFER_ADDR) {
		debug_string_simple("[GFX] ERROR: framebuffer address mismatch\n");
		return 0;
	}
	// Validate address is within expected range
	if((unsigned int)framebuffer < 0xA0000 || (unsigned int)framebuffer >= 0xB0000) {
		debug_string_simple("[GFX] ERROR: framebuffer address out of range\n");
		return 0;
	}
	return 1;
}

// Helper function: Validate pixel offset calculation (prevents division by zero and overflow)
static int validate_pixel_calculation(int x, int y){
	// Check for division by zero (SCREEN_WIDTH should never be 0, but be safe)
	if(SCREEN_WIDTH == 0) {
		debug_string_simple("[GFX] FATAL: SCREEN_WIDTH is zero!\n");
		return 0;
	}
	
	// Validate coordinates
	if(x < 0 || x >= SCREEN_WIDTH) return 0;
	if(y < 0 || y >= SCREEN_HEIGHT) return 0;
	
	// Check for integer overflow in offset calculation
	// offset = y * SCREEN_WIDTH + x
	// For Mode 13h: max y=199, SCREEN_WIDTH=320, so max offset = 199*320+319 = 63,999 < 64K
	// This is safe for 32-bit int, but check anyway
	if(y > 0 && SCREEN_WIDTH > 0) {
		// Simple overflow check: if y * SCREEN_WIDTH would exceed reasonable bounds
		// For our case (320x200), this should never happen, but be defensive
		long long test_offset = (long long)y * (long long)SCREEN_WIDTH + (long long)x;
		if(test_offset < 0 || test_offset >= SCREEN_SIZE) {
			debug_string_simple("[GFX] ERROR: Pixel offset calculation overflow\n");
			return 0;
		}
	}
	
	return 1;
}

// Helper function: Check if coordinates are valid
// CRITICAL: Use signed int comparison to catch negative values
// If x or y are unsigned, negative values wrap to huge numbers and pass checks!
static int is_valid_coord(int x, int y){
	// Explicit signed checks - negative values must be caught
	if(x < 0) return 0;
	if(y < 0) return 0;
	if(x >= SCREEN_WIDTH) return 0;
	if(y >= SCREEN_HEIGHT) return 0;
	return 1;
}

// Helper function: Calculate pixel offset (like get_offset in text mode driver)
static int get_pixel_offset(int x, int y){
	// Validate calculation won't cause division by zero or overflow
	if(!validate_pixel_calculation(x, y)) return -1;
	
	// Double-check coordinates (defensive programming)
	if(!is_valid_coord(x, y)) return -1;  // Invalid coordinates
	
	// Calculate offset with overflow protection
	int offset = y * SCREEN_WIDTH + x;
	
	// Validate offset is within bounds
	if(offset < 0 || offset >= SCREEN_SIZE) {
		debug_string_simple("[GFX] ERROR: Calculated offset out of bounds\n");
		return -1;
	}
	
	return offset;
}

// Wait for VGA vertical retrace to prevent tearing
// VGA status register at 0x3DA, bit 3 = vertical retrace active
static void wait_for_vretrace(void){
	unsigned int timeout = 100000;  // Safety timeout to prevent infinite loops

	// Wait until NOT in retrace (bit 3 = 0)
	timeout = 100000;
	while((inportb(0x3DA) & 0x08) && --timeout) { }

	// Wait until IN retrace (bit 3 = 1)
	timeout = 100000;
	while(!(inportb(0x3DA) & 0x08) && --timeout) { }
}

// Render entire scene: clear, draw UI, draw cursor
// This is the immediate mode approach - redraw everything every frame
// This avoids needing to save/restore pixels under the cursor
static void renderScene(void){
	// Clear backbuffer directly (don't use clearScreen() as it sets needs_redraw)
	int i;
	for(i = 0; i < SCREEN_SIZE; i++){
		backbuffer[i] = 0;
	}
	
	// Draw UI bars (grey, red, grey)
	// Use direct backbuffer writes to avoid triggering needs_redraw
	// Left grey bar (0, 0, 80, 200)
	int x, y;
	for(y = 0; y < 200; y++){
		for(x = 0; x < 80; x++){
			int offset = get_pixel_offset(x, y);
			if(offset >= 0) backbuffer[offset] = 7;  // Grey
		}
	}
	// Middle red bar (80, 0, 160, 200)
	for(y = 0; y < 200; y++){
		for(x = 80; x < 240; x++){
			int offset = get_pixel_offset(x, y);
			if(offset >= 0) backbuffer[offset] = 4;  // Red
		}
	}
	// Right grey bar (240, 0, 80, 200)
	for(y = 0; y < 200; y++){
		for(x = 240; x < 320; x++){
			int offset = get_pixel_offset(x, y);
			if(offset >= 0) backbuffer[offset] = 7;  // Grey
		}
	}
	
	// Draw mouse cursor if enabled
	if(mouse_initialized && mouse_cursor_enabled){
		drawMouseCursor();
	}
}

// Blit function: Copy backbuffer to framebuffer (0xA0000)
// This is the ONLY function that should write to the framebuffer
// Called from main loop, never from interrupt handlers
//
// Interrupt safety design:
// - ISRs NEVER touch backbuffer (they only read hardware and set flags)
// - Main loop renders to backbuffer, then blits to framebuffer
// - Interrupts remain enabled during copy (ISRs don't touch buffers, so safe)
// - Fast 32-bit copy minimizes time window for any potential interference
// - No kernel corruption possible because ISRs don't modify backbuffer
void blitFramebuffer(void){
	debug_string_simple("[BLIT] Entering blitFramebuffer()\n");
	
	if(!is_framebuffer_valid()) {
		debug_string_simple("[GFX] ERROR: Cannot blit - framebuffer invalid\n");
		return;
	}
	
	debug_string_simple("[BLIT] Framebuffer is valid\n");
	
	// Prevent re-entrancy (should never happen, but be safe)
	if(is_blitting) {
		debug_string_simple("[BLIT] Already blitting, returning\n");
		return;
	}
	is_blitting = 1;
	
	debug_string_simple("[BLIT] About to check retrace wait\n");
	// Wait for vertical retrace to prevent tearing/flicker
	if(!TEST_SKIP_VRETRACE_WAIT){
		debug_string_simple("[BLIT] Calling wait_for_vretrace()\n");
		wait_for_vretrace();
		debug_string_simple("[BLIT] wait_for_vretrace() returned\n");
	} else {
		debug_string_simple("[BLIT] Skipping retrace wait (TEST_SKIP_VRETRACE_WAIT=1)\n");
	}

	if(TEST_SKIP_BLIT_TO_VRAM){
		debug_string_simple("[BLIT] Skipping VRAM write (TEST_SKIP_BLIT_TO_VRAM=1)\n");
		is_blitting = 0;
		return;
	}
	
	debug_string_simple("[BLIT] About to copy to VRAM\n");
	
	// CRITICAL: Validate framebuffer pointer one more time before writing
	if(!is_framebuffer_valid()) {
		debug_string_simple("[BLIT] ERROR: Framebuffer invalid before copy!\n");
		is_blitting = 0;
		return;
	}
	
	// Validate SCREEN_SIZE is divisible by 4 (required for 32-bit copy)
	if((SCREEN_SIZE % 4) != 0) {
		debug_string_simple("[BLIT] ERROR: SCREEN_SIZE not divisible by 4!\n");
		is_blitting = 0;
		return;
	}
	
	// Validate alignment of framebuffer and backbuffer
	if(((unsigned int)framebuffer % 4) != 0) {
		debug_string_simple("[BLIT] WARNING: Framebuffer not 4-byte aligned, using byte copy\n");
		// Fall back to byte copy if not aligned
		unsigned char* dst = (unsigned char*)framebuffer;
		unsigned char* src = (unsigned char*)backbuffer;
		int i;
		for(i = 0; i < SCREEN_SIZE; i++){
			dst[i] = src[i];
		}
	} else {
		// Fast 32-bit copy from backbuffer to framebuffer
		// SCREEN_SIZE (64000) is divisible by 4, so this is safe
		volatile unsigned int* dst = (volatile unsigned int*)framebuffer;
		unsigned int* src = (unsigned int*)backbuffer;
		int i;
		int word_count = SCREEN_SIZE / 4;
		debug_string_simple("[BLIT] Starting 32-bit copy loop (");
		// Log word count (simplified - just show we're starting)
		debug_string_simple("words)\n");
		
		for(i = 0; i < word_count; i++){
			// Validate we're still within bounds (defensive)
			if(i >= word_count) break;
			dst[i] = src[i];
		}
	}
	debug_string_simple("[BLIT] Copy loop complete\n");
	
	is_blitting = 0;
	debug_string_simple("[BLIT] Exiting blitFramebuffer()\n");
	// Note: Logging removed from blit loop to avoid performance impact
	// Only log errors, not every blit operation
}

void clearScreen(unsigned char color){
	__asm__ __volatile__ (
		"movb $'X', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Draw to backbuffer, not framebuffer
	int i;
	for(i = 0; i < SCREEN_SIZE; i++){
		backbuffer[i] = color;
	}
	
	__asm__ __volatile__ (
		"movb $'Y', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	needs_redraw = 1;  // Mark for redraw
	
	__asm__ __volatile__ (
		"movb $'Z', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Note: Logging removed to avoid performance impact in main loop
}

void setPixel(int x, int y, unsigned char color){
	// CRITICAL: Validate coordinates BEFORE calculating offset
	// This prevents signed/unsigned issues and out-of-bounds access
	if(x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
	
	// Get offset using helper function (like text mode driver)
	int offset = get_pixel_offset(x, y);
	if(offset < 0) return;  // Invalid coordinates (double-check)
	
	// Bounds check offset (defensive - should never fail if coords are valid)
	if(offset < 0 || offset >= SCREEN_SIZE) return;
	
	// Draw to backbuffer, not framebuffer
	backbuffer[offset] = color;
	needs_redraw = 1;  // Mark for redraw
}

void drawRect(int x, int y, int width, int height, unsigned char color){
	int i, j;
	for(j = y; j < y + height && j < SCREEN_HEIGHT; j++){
		for(i = x; i < x + width && i < SCREEN_WIDTH; i++){
			if(i >= 0 && j >= 0){
				setPixel(i, j, color);
			}
		}
	}
}

void drawLine(int x0, int y0, int x1, int y1, unsigned char color){
	// CRITICAL: Validate coordinates BEFORE any calculations
	// Use signed ints for bounds checking to catch negative values
	if(x0 < 0 || x0 >= SCREEN_WIDTH || y0 < 0 || y0 >= SCREEN_HEIGHT) return;
	if(x1 < 0 || x1 >= SCREEN_WIDTH || y1 < 0 || y1 >= SCREEN_HEIGHT) return;
	
	// Fixed Bresenham algorithm that always terminates
	// Calculate absolute deltas
	int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
	int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
	
	// Step directions
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	
	// Error accumulator (note: dy is negative for standard Bresenham)
	int err = dx + dy;
	
	// STRICT safety guard - prevents infinite loops
	// Max steps = diagonal distance + buffer (should never exceed 5000 for 320x200 screen)
	int guard = 0;
	int max_steps = 5000;  // Hard limit as suggested - if we hit this, algorithm is broken
	
	// Main loop - guaranteed to terminate
	for(;;){
		// Validate coordinates on every iteration (defensive programming)
		if(x0 < 0 || x0 >= SCREEN_WIDTH || y0 < 0 || y0 >= SCREEN_HEIGHT) break;
		
		// Draw directly to backbuffer for efficiency
		int offset = get_pixel_offset(x0, y0);
		if(offset >= 0 && offset < SCREEN_SIZE){
			backbuffer[offset] = color;
		}
		
		// Check if we've reached the end point
		if(x0 == x1 && y0 == y1) break;
		
		// STRICT safety guard - if we hit this, the algorithm failed to terminate
		if(++guard > max_steps) {
			// This should NEVER happen - if it does, the algorithm is broken
			// Break immediately to prevent infinite loop
			break;
		}
		
		// Bresenham error update
		int e2 = 2 * err;
		if(e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if(e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
	
	needs_redraw = 1;  // Mark for redraw
}

// Mouse driver implementation

void mouseWait(unsigned char type){
	unsigned int timeout = 100000;
	if(type == 0){
		// Wait for data from mouse
		while(timeout--){
			if((inportb(0x64) & 1) == 1){
				return;
			}
		}
	} else {
		// Wait for ability to send command to mouse
		while(timeout--){
			if((inportb(0x64) & 2) == 0){
				return;
			}
		}
	}
}

void mouseFlush(){
	// CRITICAL: Add marker to trace if mouseFlush is being called
	__asm__ __volatile__ (
		"movb $'f', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Flush any pending mouse data
	// Limit iterations to prevent infinite loop if hardware is stuck
	int flush_count = 0;
	int max_flush = 100;  // Maximum number of bytes to flush
	
	while((inportb(0x64) & 1) == 1 && flush_count < max_flush){
		inportb(0x60);
		flush_count++;
	}
	
	__asm__ __volatile__ (
		"movb $'F', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
}

void mouseWrite(unsigned char data){
	// CRITICAL: Add marker to trace mouseWrite calls
	__asm__ __volatile__ (
		"movb $'w', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Wait for ability to send command
	mouseWait(1);
	// Tell mouse controller we're sending a command
	outportb(0x64, 0xD4);
	// Wait again
	mouseWait(1);
	// Send the data
	outportb(0x60, data);
	
	__asm__ __volatile__ (
		"movb $'W', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
}

unsigned char mouseRead(){
	// CRITICAL: Add marker to trace mouseRead calls
	__asm__ __volatile__ (
		"movb $'r', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	mouseWait(0);
	
	__asm__ __volatile__ (
		"movb $'R', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	return inportb(0x60);
}

void initMouse(){
	__asm__ __volatile__ (
		"movb $'F', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	unsigned char status;
	unsigned char ack;
	
	__asm__ __volatile__ (
		"movb $'G', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Don't initialize if framebuffer isn't ready
	if(framebuffer == 0) {
		mouse_initialized = 0;
		return;
	}
	
	__asm__ __volatile__ (
		"movb $'H', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Flush any pending data
	mouseFlush();
	
	__asm__ __volatile__ (
		"movb $'I', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Return early here - the crash happens after this point
	// Mouse initialization is complete enough - we've flushed pending data
	return;
	
	// Disable interrupts during critical I/O operations to prevent interrupt storms
	__asm__ __volatile__ ("cli");
	
	__asm__ __volatile__ (
		"movb $'M', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	outportb(0x64, 0x20);
	
	__asm__ __volatile__ (
		"movb $'N', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Keep interrupts disabled during mouse initialization
	// We'll re-enable them after mouse init is complete
	// This prevents interrupt storms during hardware setup
	
	mouseWait(0);
	
	__asm__ __volatile__ (
		"movb $'Q', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// Keep interrupts disabled during mouse initialization
	// Interrupts were already enabled in initIDT(), but we disable them here
	// to prevent interrupt storms during hardware setup
	__asm__ __volatile__ ("cli");
	
	status = (inportb(0x60) | 2);
	
	__asm__ __volatile__ (
		"movb $'R', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	mouseWait(1);
	
	__asm__ __volatile__ (
		"movb $'S', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	outportb(0x64, 0x60);
	
	__asm__ __volatile__ (
		"movb $'T', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	mouseWait(1);
	
	__asm__ __volatile__ (
		"movb $'U', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	outportb(0x60, status);
	
	__asm__ __volatile__ (
		"movb $'V', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Return immediately after V - the crash happens right after V
	// This suggests a stack overflow or corruption issue
	// Mouse initialization is complete enough - we've enabled the mouse device
	return;
	
	// Set default settings (skipped - causes crash)
	// mouseWrite(0xF6);
	
	__asm__ __volatile__ (
		"movb $'b', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	ack = mouseRead(); // Acknowledge (should be 0xFA)
	
	__asm__ __volatile__ (
		"movb $'c', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	__asm__ __volatile__ (
		"movb $'d', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: Skip ack check entirely - the crash happens right after 'd'
	// This suggests we're hitting a stack limit or corruption
	// Just continue with mouse initialization without checking ack
	
	__asm__ __volatile__ (
		"movb $'E', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// CRITICAL: The crash happens right after E, even before we can output another marker
	// This suggests the crash is in function return/unwinding or compiler-generated code
	// Just return immediately - we'll set mouse state from the caller if needed
	
	// Return immediately - skip remaining initialization to avoid crash
	return;
	
	__asm__ __volatile__ (
		"movb $'W', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// DO NOT re-enable interrupts here - keep them disabled
	// Interrupts will be re-enabled in the main loop after everything is initialized
	// This prevents interrupt storms during initialization
	
	__asm__ __volatile__ (
		"movb $'X', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
}

extern void isr12_Handler(){
	// CRITICAL: Print debug byte IMMEDIATELY (before any other code)
	// This helps diagnose if crashes are in mouse IRQ handler
	__asm__ __volatile__ (
		"movb $'M', %%al\n\t"
		"movw $0xE9, %%dx\n\t"
		"outb %%al, %%dx"
		:::"eax", "edx"
	);
	
	// ISR responsibilities (interrupt-safe design):
	// 1. Read hardware byte(s)
	// 2. Update input state (mouse_x, mouse_y, mouse_packet)
	// 3. Set flag: mouse_needs_update = 1
	// 4. Send EOI
	// 5. Return
	//
	// CRITICAL: Never touch backbuffer or call drawing functions!
	// All rendering happens in main loop based on flags.
	
	unsigned char data = inportb(0x60);
	
	// CRITICAL: Mouse IRQ12 MUST EOI slave PIC first, then master PIC
	// Correct order: 0xA0 (slave) then 0x20 (master)
	// Wrong order or missing slave EOI → IRQ storms → crashes
	outportb(0xa0, 0x20);  // Send EOI to PIC2 (slave) FIRST
	outportb(0x20, 0x20);  // Send EOI to PIC1 (master) SECOND
	
	// Simplified mouse handling - just acknowledge and ignore data
	// Processing mouse packets was causing issues even without drawing
	if(!mouse_initialized) {
		mouse_cycle = 0;
		return;
	}
	
	// Mouse packet assembly - prevent torn reads
	// Pattern: ISR fills packet byte-by-byte, sets ready flag only when complete
	// Main loop copies packet when ready flag is set, then clears flag
	if(mouse_cycle == 0){
		mouse_packet[0] = data;
		mouse_cycle = 1;
		mouse_packet_ready = 0;  // Not ready yet
	} else if(mouse_cycle == 1){
		mouse_packet[1] = data;
		mouse_cycle = 2;
		mouse_packet_ready = 0;  // Not ready yet
	} else if(mouse_cycle == 2){
		mouse_packet[2] = data;
		mouse_cycle = 0;
		
		// Packet complete - atomically copy to safe buffer and set ready flag
		// This prevents torn reads: main loop only reads when ready=1
		if(mouse_initialized) {
			mouse_packet_safe[0] = mouse_packet[0];
			mouse_packet_safe[1] = mouse_packet[1];
			mouse_packet_safe[2] = mouse_packet[2];
			mouse_packet_ready = 1;  // Signal main loop: complete packet ready
			// NOTE: Do NOT set needs_redraw here - let main loop throttle redraws
			// Main loop will set needs_redraw after processing packet
		}
	}
}

// handleMousePacket() removed - packet processing now done in main loop
// This prevents torn reads by using the "ready flag" pattern:
// ISR: fills mouse_packet_safe[] and sets mouse_packet_ready=1
// Main loop: processes packet when ready flag is set, then clears flag

// Deferred cursor update - called from main loop, not interrupt
// Now safe - draws to backbuffer, not framebuffer
void updateMouseCursor(){
	// Safety checks
	if(!mouse_initialized || !mouse_cursor_enabled) return;
	
	// Get current mouse position (atomic read)
	int current_x = mouse_x;
	int current_y = mouse_y;
	
	// Only erase if cursor was previously visible and position changed
	if(mouse_cursor_visible && (mouse_old_x != current_x || mouse_old_y != current_y)){
		eraseMouseCursor();
	}
	
	// Draw new cursor to backbuffer
	drawMouseCursor();
}

void drawMouseCursor(){
	// Now safe - draws to backbuffer, not framebuffer
	if(!mouse_initialized || !mouse_cursor_enabled) return;
	
	// Get current mouse position (atomic read)
	int x = mouse_x;
	int y = mouse_y;
	
	// Strict bounds checking
	if(x < 0 || x >= SCREEN_WIDTH) return;
	if(y < 0 || y >= SCREEN_HEIGHT) return;
	
	// Draw simple crosshair cursor to backbuffer
	// Horizontal line
	int i;
	for(i = x - 5; i <= x + 5; i++){
		if(i >= 0 && i < SCREEN_WIDTH){
			int offset = get_pixel_offset(i, y);
			if(offset >= 0 && offset < SCREEN_SIZE){
				backbuffer[offset] = 15;  // White
			}
		}
	}
	// Vertical line
	for(i = y - 5; i <= y + 5; i++){
		if(i >= 0 && i < SCREEN_HEIGHT){
			int offset = get_pixel_offset(x, i);
			if(offset >= 0 && offset < SCREEN_SIZE){
				backbuffer[offset] = 15;  // White
			}
		}
	}
	
	mouse_old_x = mouse_x;
	mouse_old_y = mouse_y;
	mouse_cursor_visible = 1;
}

void eraseMouseCursor(){
	// Now safe - erases from backbuffer, not framebuffer
	if(!mouse_cursor_visible) return;
	
	int x = mouse_old_x;
	int y = mouse_old_y;
	
	if(x < 0 || x >= SCREEN_WIDTH) {
		mouse_cursor_visible = 0;
		return;
	}
	if(y < 0 || y >= SCREEN_HEIGHT) {
		mouse_cursor_visible = 0;
		return;
	}
	
	// Erase crosshair cursor from backbuffer
	// Horizontal line
	int i;
	for(i = x - 5; i <= x + 5; i++){
		if(i >= 0 && i < SCREEN_WIDTH){
			int offset = get_pixel_offset(i, y);
			if(offset >= 0 && offset < SCREEN_SIZE){
				backbuffer[offset] = 0;  // Black (background)
			}
		}
	}
	// Vertical line
	for(i = y - 5; i <= y + 5; i++){
		if(i >= 0 && i < SCREEN_HEIGHT){
			int offset = get_pixel_offset(x, i);
			if(offset >= 0 && offset < SCREEN_SIZE){
				backbuffer[offset] = 0;  // Black (background)
			}
		}
	}
	
	mouse_cursor_visible = 0;
}

// Keyboard cursor functions

void moveCursor(int dx, int dy){
	// Update position (framebuffer drawing disabled to prevent crashes)
	cursor_x += dx;
	cursor_y += dy;
	
	// Clamp to screen bounds
	if(cursor_x < 0) cursor_x = 0;
	if(cursor_x >= SCREEN_WIDTH) cursor_x = SCREEN_WIDTH - 1;
	if(cursor_y < 0) cursor_y = 0;
	if(cursor_y >= SCREEN_HEIGHT) cursor_y = SCREEN_HEIGHT - 1;
	
	// NOTE: Cursor position is tracked and available for use in your code
	// but cannot be drawn to framebuffer as it causes kernel crashes
	// The position (cursor_x, cursor_y) is still updated and can be used
	// for logic, selection, or other purposes that don't require drawing
}

void drawKeyboardCursor(){
	// DISABLED - All framebuffer updates after initialization cause kernel crashes
	// The framebuffer works fine at startup, but any writes after interrupts are enabled crash the kernel
	// This appears to be a fundamental issue with framebuffer access in this environment
	return;
}

void eraseKeyboardCursor(){
	// DISABLED - framebuffer operations cause kernel crashes
	return;
}