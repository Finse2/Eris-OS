#include "ne2000.h"

void setMonitorColor(char);
void cls();
void printString(char*);
void printChar(char);
void printColorString(char* , char);
void printColorChar(char , char);
void vid();
void put();
void get();

extern void read();
extern void write();

// Port I/O (defined in main.c)
unsigned char inportb(unsigned short);
void outportb(unsigned short, unsigned char);

// Mouse state (defined in main.c)
extern int mouse_x;
extern int mouse_y;
extern unsigned char mouse_buttons;

// File system functions and flags
#define FS_FILE_EXECUTABLE 0x02

void fs_init();
int fs_create_file(char* filename, unsigned char flags);
int fs_delete_file(char* filename);
int fs_read_file(char* filename, char* buffer, unsigned int max_size);
int fs_write_file(char* filename, char* data, unsigned int size);
int fs_list_files();
int fs_find_file(char* filename);
int fs_get_file_size(char* filename);
int fs_execute_file(char* filename);
int fs_mark_executable(char* filename);


int blockAddr;
char At[1024];

char* TM_START;



void blink(){
	setMonitorColor(0x59);
	int TIME_OUT = 0x10fffff;
	while(--TIME_OUT);
	setMonitorColor(0xa5);
}

// Current color attribute (stored for BG/FG changes)
static unsigned char currentColorAttr = 0xa5;

// Graphics mode state: 0 = text (80x25), 1 = graphics (VESA high resolution)
static int vga_graphics_enabled = 0;

static unsigned char vga_mode_text_regs[] = {
	0x67,
	0x03, 0x00, 0x03, 0x00, 0x02,
	0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x4F,
	0x0D, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x50, 0x9C, 0x0E, 0x8F,
	0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF,
	0x00, 0x0F, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x0E, 0x0F,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00
};

// Bochs/QEMU VBE (VESA-like) interface
#define VBE_DISPI_IOPORT_INDEX     0x01CE
#define VBE_DISPI_IOPORT_DATA      0x01CF
#define VBE_DISPI_INDEX_ID         0x0
#define VBE_DISPI_INDEX_XRES       0x1
#define VBE_DISPI_INDEX_YRES       0x2
#define VBE_DISPI_INDEX_BPP        0x3
#define VBE_DISPI_INDEX_ENABLE     0x4
#define VBE_DISPI_DISABLED         0x00
#define VBE_DISPI_ENABLED          0x01
#define VBE_DISPI_LFB_ENABLED      0x40
#define VBE_DISPI_ID0              0xB0C0
#define VBE_DISPI_ID5              0xB0C5

#define VESA_LFB_BASE              0xE0000000u
#define VESA_LFB_BASE_ALT          0xFD000000u
#define VESA_DEFAULT_WIDTH         800
#define VESA_DEFAULT_HEIGHT        600
#define VESA_DEFAULT_BPP           32

static unsigned int vesa_width = 0;
static unsigned int vesa_height = 0;
static unsigned int vesa_bpp = 0;
static unsigned int vesa_pitch = 0;
static volatile unsigned int* vesa_lfb = (volatile unsigned int*)VESA_LFB_BASE;
static volatile unsigned int* vesa_lfb_alt = (volatile unsigned int*)VESA_LFB_BASE_ALT;
static volatile unsigned int* vesa_active_lfb = (volatile unsigned int*)VESA_LFB_BASE;
static int desktop_backbuffer_enabled = 0;
#define DESKTOP_BACKBUFFER_BASE 0x200000u
static unsigned int* desktop_backbuffer = (unsigned int*)DESKTOP_BACKBUFFER_BASE;

static void desktopPresent() {
	if(!desktop_backbuffer_enabled) return;
	unsigned int pixels = vesa_width * vesa_height;
	unsigned int i;
	for(i = 0; i < pixels; i++) {
		vesa_active_lfb[i] = desktop_backbuffer[i];
	}
}

static inline void vesaStorePixelIndex(unsigned int idx, unsigned int color) {
	if(desktop_backbuffer_enabled) {
		desktop_backbuffer[idx] = color;
	} else {
		vesa_active_lfb[idx] = color;
	}
}

int vgaIsGraphicsMode();
static int vgaEnterVesaMode(unsigned short width, unsigned short height, unsigned short bpp);

// Minimal 8x8 font (ASCII 0-127). Each byte is one row, MSB on the left.
static const unsigned char font8x8_basic[128][8] = {
	{0,0,0,0,0,0,0,0}, // 0
	{0,0,0,0,0,0,0,0}, // 1
	{0,0,0,0,0,0,0,0}, // 2
	{0,0,0,0,0,0,0,0}, // 3
	{0,0,0,0,0,0,0,0}, // 4
	{0,0,0,0,0,0,0,0}, // 5
	{0,0,0,0,0,0,0,0}, // 6
	{0,0,0,0,0,0,0,0}, // 7
	{0,0,0,0,0,0,0,0}, // 8
	{0,0,0,0,0,0,0,0}, // 9
	{0,0,0,0,0,0,0,0}, // 10
	{0,0,0,0,0,0,0,0}, // 11
	{0,0,0,0,0,0,0,0}, // 12
	{0,0,0,0,0,0,0,0}, // 13
	{0,0,0,0,0,0,0,0}, // 14
	{0,0,0,0,0,0,0,0}, // 15
	{0,0,0,0,0,0,0,0}, // 16
	{0,0,0,0,0,0,0,0}, // 17
	{0,0,0,0,0,0,0,0}, // 18
	{0,0,0,0,0,0,0,0}, // 19
	{0,0,0,0,0,0,0,0}, // 20
	{0,0,0,0,0,0,0,0}, // 21
	{0,0,0,0,0,0,0,0}, // 22
	{0,0,0,0,0,0,0,0}, // 23
	{0,0,0,0,0,0,0,0}, // 24
	{0,0,0,0,0,0,0,0}, // 25
	{0,0,0,0,0,0,0,0}, // 26
	{0,0,0,0,0,0,0,0}, // 27
	{0,0,0,0,0,0,0,0}, // 28
	{0,0,0,0,0,0,0,0}, // 29
	{0,0,0,0,0,0,0,0}, // 30
	{0,0,0,0,0,0,0,0}, // 31
	{0,0,0,0,0,0,0,0}, // 32 ' '
	{24,24,24,24,24,0,24,0}, // 33 '!'
	{54,54,36,0,0,0,0,0}, // 34 '"'
	{54,54,127,54,127,54,54,0}, // 35 '#'
	{24,62,96,60,6,124,24,0}, // 36 '$'
	{98,102,12,24,48,102,70,0}, // 37 '%'
	{28,54,28,118,59,54,111,0}, // 38 '&'
	{12,12,8,0,0,0,0,0}, // 39 '''
	{6,12,24,24,24,12,6,0}, // 40 '('
	{48,24,12,12,12,24,48,0}, // 41 ')'
	{0,54,28,127,28,54,0,0}, // 42 '*'
	{0,24,24,126,24,24,0,0}, // 43 '+'
	{0,0,0,0,0,24,24,48}, // 44 ','
	{0,0,0,126,0,0,0,0}, // 45 '-'
	{0,0,0,0,0,24,24,0}, // 46 '.'
	{2,6,12,24,48,96,64,0}, // 47 '/'
	{62,99,107,107,99,99,62,0}, // 48 '0'
	{24,56,24,24,24,24,126,0}, // 49 '1'
	{62,99,3,14,56,96,127,0}, // 50 '2'
	{62,99,3,30,3,99,62,0}, // 51 '3'
	{14,30,54,102,127,6,15,0}, // 52 '4'
	{127,96,124,6,3,99,62,0}, // 53 '5'
	{30,48,96,124,99,99,62,0}, // 54 '6'
	{127,99,6,12,24,24,24,0}, // 55 '7'
	{62,99,99,62,99,99,62,0}, // 56 '8'
	{62,99,99,63,3,6,60,0}, // 57 '9'
	{0,24,24,0,0,24,24,0}, // 58 ':'
	{0,24,24,0,0,24,24,48}, // 59 ';'
	{6,12,24,48,24,12,6,0}, // 60 '<'
	{0,0,126,0,126,0,0,0}, // 61 '='
	{48,24,12,6,12,24,48,0}, // 62 '>'
	{62,99,6,12,24,0,24,0}, // 63 '?'
	{62,99,123,123,123,96,62,0}, // 64 '@'
	{28,54,99,99,127,99,99,0}, // 65 'A'
	{124,102,102,124,102,102,124,0}, // 66 'B'
	{62,99,96,96,96,99,62,0}, // 67 'C'
	{124,102,99,99,99,102,124,0}, // 68 'D'
	{127,96,96,124,96,96,127,0}, // 69 'E'
	{127,96,96,124,96,96,96,0}, // 70 'F'
	{62,99,96,111,99,99,62,0}, // 71 'G'
	{99,99,99,127,99,99,99,0}, // 72 'H'
	{126,24,24,24,24,24,126,0}, // 73 'I'
	{31,6,6,6,6,102,60,0}, // 74 'J'
	{99,102,108,120,108,102,99,0}, // 75 'K'
	{96,96,96,96,96,96,127,0}, // 76 'L'
	{99,119,127,107,99,99,99,0}, // 77 'M'
	{99,115,123,111,103,99,99,0}, // 78 'N'
	{62,99,99,99,99,99,62,0}, // 79 'O'
	{124,102,102,124,96,96,96,0}, // 80 'P'
	{62,99,99,99,99,107,62,3}, // 81 'Q'
	{124,102,102,124,108,102,99,0}, // 82 'R'
	{62,99,96,62,3,99,62,0}, // 83 'S'
	{126,24,24,24,24,24,24,0}, // 84 'T'
	{99,99,99,99,99,99,62,0}, // 85 'U'
	{99,99,99,99,99,54,28,0}, // 86 'V'
	{99,99,99,107,127,119,99,0}, // 87 'W'
	{99,99,54,28,54,99,99,0}, // 88 'X'
	{102,102,102,60,24,24,24,0}, // 89 'Y'
	{127,3,6,12,24,48,127,0}, // 90 'Z'
	{30,24,24,24,24,24,30,0}, // 91 '['
	{64,96,48,24,12,6,2,0}, // 92 '\'
	{60,12,12,12,12,12,60,0}, // 93 ']'
	{8,28,54,99,0,0,0,0}, // 94 '^'
	{0,0,0,0,0,0,0,255}, // 95 '_'
	{12,12,24,0,0,0,0,0}, // 96 '`'
	{0,0,60,6,62,102,63,0}, // 97 'a'
	{96,96,124,102,102,102,124,0}, // 98 'b'
	{0,0,62,99,96,99,62,0}, // 99 'c'
	{6,6,62,102,102,102,63,0}, // 100 'd'
	{0,0,62,99,127,96,62,0}, // 101 'e'
	{14,27,24,60,24,24,24,0}, // 102 'f'
	{0,0,63,102,102,62,6,124}, // 103 'g'
	{96,96,124,102,102,102,102,0}, // 104 'h'
	{24,0,56,24,24,24,60,0}, // 105 'i'
	{6,0,6,6,6,102,102,60}, // 106 'j'
	{96,96,102,108,120,108,102,0}, // 107 'k'
	{56,24,24,24,24,24,60,0}, // 108 'l'
	{0,0,118,127,107,107,99,0}, // 109 'm'
	{0,0,124,102,102,102,102,0}, // 110 'n'
	{0,0,62,99,99,99,62,0}, // 111 'o'
	{0,0,124,102,102,124,96,96}, // 112 'p'
	{0,0,63,102,102,62,6,6}, // 113 'q'
	{0,0,109,118,96,96,96,0}, // 114 'r'
	{0,0,63,96,62,3,126,0}, // 115 's'
	{24,24,126,24,24,27,14,0}, // 116 't'
	{0,0,102,102,102,102,63,0}, // 117 'u'
	{0,0,102,102,102,60,24,0}, // 118 'v'
	{0,0,99,107,107,127,54,0}, // 119 'w'
	{0,0,99,54,28,54,99,0}, // 120 'x'
	{0,0,102,102,102,62,6,124}, // 121 'y'
	{0,0,127,12,24,48,127,0}, // 122 'z'
	{14,24,24,112,24,24,14,0}, // 123 '{'
	{24,24,24,0,24,24,24,0}, // 124 '|'
	{112,24,24,14,24,24,112,0}, // 125 '}'
	{50,76,0,0,0,0,0,0}, // 126 '~'
	{0,0,0,0,0,0,0,0} // 127
};

static const unsigned int vga_palette_32[16] = {
	0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
	0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
	0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
	0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

#define VESA_BG_COLOR   0x202020
#define VESA_BAR_COLOR  0x000000
#define VESA_CENTER_COLOR 0xAA0000
#define VESA_SIDE_BAR_WIDTH 80
#define VESA_CENTER_BAR_WIDTH 16

static void vesaPutPixel(int x, int y, unsigned int color) {
	if(x < 0 || y < 0) return;
	if((unsigned int)x >= vesa_width || (unsigned int)y >= vesa_height) return;
	vesaStorePixelIndex(y * (vesa_pitch >> 2) + (unsigned int)x, color);
}

static void vesaDrawCharAt(int col, int row, unsigned char ch, unsigned char attr) {
	int x = col * 8;
	int y = row * 16;
	unsigned int fg = vga_palette_32[attr & 0x0F];
	unsigned int bg = vga_palette_32[(attr >> 4) & 0x0F];
	const unsigned char* glyph = font8x8_basic[ch & 0x7F];
	int gy, gx;
	for(gy = 0; gy < 8; gy++) {
		unsigned char bits = glyph[gy];
		for(gx = 0; gx < 8; gx++) {
			unsigned int color = (bits & (1 << (7 - gx))) ? fg : bg;
			vesaPutPixel(x + gx, y + gy * 2, color);
			vesaPutPixel(x + gx, y + gy * 2 + 1, color);
		}
	}
}

void vesaRedrawTextScreen() {
	if(!vgaIsGraphicsMode()) return;
	// Background: dark gray with black side bars and a red center bar.
	unsigned int x, y;
	unsigned int width = vesa_width;
	unsigned int height = vesa_height;
	unsigned int center_x = width / 2;
	unsigned int center_half = VESA_CENTER_BAR_WIDTH / 2;
	for(y = 0; y < height; y++) {
		for(x = 0; x < width; x++) {
			unsigned int color = VESA_BG_COLOR;
			if(x < VESA_SIDE_BAR_WIDTH || x >= (width - VESA_SIDE_BAR_WIDTH)) {
				color = VESA_BAR_COLOR;
			}
			if(x >= (center_x - center_half) && x < (center_x + center_half)) {
				color = VESA_CENTER_COLOR;
			}
			vesaStorePixelIndex(y * (vesa_pitch >> 2) + x, color);
		}
	}

	int row, col;
	char* vga = (char*)0xB8000;
	for(row = 0; row < 25; row++) {
		for(col = 0; col < 80; col++) {
			int off = (row * 80 + col) * 2;
			unsigned char ch = (unsigned char)vga[off];
			unsigned char attr = (unsigned char)vga[off + 1];
			vesaDrawCharAt(col, row, ch, attr);
		}
	}
}

static unsigned short vbeReadReg(unsigned short index) {
	// Bochs/QEMU VBE uses 16-bit index/data registers at 0x1CE/0x1CF.
	// Byte-wise access to INDEX+1/DATA+1 hits wrong ports and breaks mode setup.
	__asm__ __volatile__("outw %0, %1" : : "a"(index), "Nd"((unsigned short)VBE_DISPI_IOPORT_INDEX));
	unsigned short value;
	__asm__ __volatile__("inw %1, %0" : "=a"(value) : "Nd"((unsigned short)VBE_DISPI_IOPORT_DATA));
	return value;
}

static void vbeWriteReg(unsigned short index, unsigned short value) {
	__asm__ __volatile__("outw %0, %1" : : "a"(index), "Nd"((unsigned short)VBE_DISPI_IOPORT_INDEX));
	__asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"((unsigned short)VBE_DISPI_IOPORT_DATA));
}

static int vbeAvailable() {
	unsigned short id = vbeReadReg(VBE_DISPI_INDEX_ID);
	return (id >= VBE_DISPI_ID0 && id <= VBE_DISPI_ID5);
}

void vgaWriteRegisters(unsigned char* regs) {
	unsigned int i;

	// Write MISC register.
	outportb(0x3C2, *regs);
	regs++;

	// Sequencer registers.
	for(i = 0; i < 5; i++) {
		outportb(0x3C4, (unsigned char)i);
		outportb(0x3C5, *regs);
		regs++;
	}

	// Unlock CRTC registers 0x03 and 0x11.
	outportb(0x3D4, 0x03);
	outportb(0x3D5, inportb(0x3D5) | 0x80);
	outportb(0x3D4, 0x11);
	outportb(0x3D5, inportb(0x3D5) & (unsigned char)~0x80);

	// Write CRTC registers.
	for(i = 0; i < 25; i++) {
		outportb(0x3D4, (unsigned char)i);
		outportb(0x3D5, *regs);
		regs++;
	}

	// Graphics controller registers.
	for(i = 0; i < 9; i++) {
		outportb(0x3CE, (unsigned char)i);
		outportb(0x3CF, *regs);
		regs++;
	}

	// Attribute controller registers.
	for(i = 0; i < 21; i++) {
		(void)inportb(0x3DA);
		outportb(0x3C0, (unsigned char)i);
		outportb(0x3C0, *regs);
		regs++;
	}

	(void)inportb(0x3DA);
	outportb(0x3C0, 0x20);
}

void vgaSetPaletteEntry(unsigned char index, unsigned char r, unsigned char g, unsigned char b) {
	outportb(0x3C8, index);
	outportb(0x3C9, r);
	outportb(0x3C9, g);
	outportb(0x3C9, b);
}

void vgaLoadDefaultPalette() {
	unsigned int i;
	for(i = 0; i < 256; i++) {
		unsigned char r = (unsigned char)((i & 0x07) * 9);
		unsigned char g = (unsigned char)(((i >> 3) & 0x07) * 9);
		unsigned char b = (unsigned char)(((i >> 6) & 0x03) * 21);
		vgaSetPaletteEntry((unsigned char)i, r, g, b);
	}
}

int vgaIsGraphicsMode() {
	if(!vga_graphics_enabled) return 0;
	// Self-heal if hardware mode is no longer enabled.
	if((vbeReadReg(VBE_DISPI_INDEX_ENABLE) & VBE_DISPI_ENABLED) == 0) {
		vga_graphics_enabled = 0;
		return 0;
	}
	return 1;
}

void vgaPlotPixel(int x, int y, unsigned char color) {
	if(!vga_graphics_enabled) return;
	if(x < 0 || y < 0) return;
	if((unsigned int)x >= vesa_width || (unsigned int)y >= vesa_height) return;
	vesaStorePixelIndex(y * (vesa_pitch >> 2) + (unsigned int)x, (unsigned int)color * 0x010101u);
}

void vgaClearGraphics(unsigned char color) {
	if(!vga_graphics_enabled) return;
	unsigned int pixels = vesa_width * vesa_height;
	unsigned int packed = (unsigned int)color * 0x010101u;
	unsigned int i;
	for(i = 0; i < pixels; i++) {
		vesaStorePixelIndex(i, packed);
	}
}

void vgaDrawDemoPattern() {
	int x, y;
	if(!vga_graphics_enabled) return;

	// Horizontal gradient background.
	for(y = 0; y < (int)vesa_height; y++) {
		for(x = 0; x < (int)vesa_width; x++) {
			unsigned char shade = (unsigned char)(((x * 255u) / vesa_width + (y * 255u) / vesa_height) >> 1);
			vgaPlotPixel(x, y, shade);
		}
	}

	// Center rectangle.
	int left = (int)(vesa_width / 4);
	int right = (int)(vesa_width - vesa_width / 4);
	int top = (int)(vesa_height / 4);
	int bottom = (int)(vesa_height - vesa_height / 4);
	for(y = top; y < bottom; y++) {
		for(x = left; x < right; x++) {
			unsigned int r = (unsigned int)((x - left) * 255u) / (unsigned int)(right - left);
			unsigned int g = (unsigned int)((y - top) * 255u) / (unsigned int)(bottom - top);
			unsigned int b = 200u;
			vesaStorePixelIndex(y * (vesa_pitch >> 2) + (unsigned int)x, (r << 16) | (g << 8) | b);
		}
	}
}

static void vesaFillRect(int x, int y, int w, int h, unsigned int color) {
	int xx, yy;
	if(w <= 0 || h <= 0) return;
	for(yy = 0; yy < h; yy++) {
		int py = y + yy;
		if(py < 0 || (unsigned int)py >= vesa_height) continue;
		for(xx = 0; xx < w; xx++) {
			int px = x + xx;
			if(px < 0 || (unsigned int)px >= vesa_width) continue;
			vesaStorePixelIndex((unsigned int)py * (vesa_pitch >> 2) + (unsigned int)px, color);
		}
	}
}

static unsigned int desktopLerpColor(unsigned int a, unsigned int b, unsigned int t, unsigned int tmax) {
	unsigned int ar = (a >> 16) & 0xFF;
	unsigned int ag = (a >> 8) & 0xFF;
	unsigned int ab = a & 0xFF;
	unsigned int br = (b >> 16) & 0xFF;
	unsigned int bg = (b >> 8) & 0xFF;
	unsigned int bb = b & 0xFF;
	unsigned int r = ar + ((br - ar) * t) / (tmax ? tmax : 1u);
	unsigned int g = ag + ((bg - ag) * t) / (tmax ? tmax : 1u);
	unsigned int bl = ab + ((bb - ab) * t) / (tmax ? tmax : 1u);
	return (r << 16) | (g << 8) | bl;
}

static void vesaDrawText(int col, int row, const char* s, unsigned char attr) {
	int c = col;
	while(*s) {
		vesaDrawCharAt(c, row, (unsigned char)(*s), attr);
		s++;
		c++;
	}
}

extern volatile unsigned int pit_ticks;

static void vesaDrawGlyphTransparent(int x, int y, unsigned char ch, unsigned int color) {
	const unsigned char* glyph = font8x8_basic[ch & 0x7F];
	int gy, gx;
	for(gy = 0; gy < 8; gy++) {
		unsigned char bits = glyph[gy];
		for(gx = 0; gx < 8; gx++) {
			if(bits & (1 << (7 - gx))) {
				vesaPutPixel(x + gx, y + gy * 2, color);
				vesaPutPixel(x + gx, y + gy * 2 + 1, color);
			}
		}
	}
}

static void vesaDrawTextTransparent(int x, int y, const char* s, unsigned int color) {
	int px = x;
	while(*s) {
		vesaDrawGlyphTransparent(px, y, (unsigned char)(*s), color);
		px += 8;
		s++;
	}
}

static void vesaDrawDottedRect(int x, int y, int w, int h, unsigned int color) {
	int i;
	for(i = 0; i < w; i += 2) {
		vesaPutPixel(x + i, y, color);
		vesaPutPixel(x + i, y + h - 1, color);
	}
	for(i = 0; i < h; i += 2) {
		vesaPutPixel(x, y + i, color);
		vesaPutPixel(x + w - 1, y + i, color);
	}
}

static void vesaDraw3DFrame(int x, int y, int w, int h, unsigned int light, unsigned int dark) {
	vesaFillRect(x, y, w, 1, light);
	vesaFillRect(x, y, 1, h, light);
	vesaFillRect(x, y + h - 1, w, 1, dark);
	vesaFillRect(x + w - 1, y, 1, h, dark);
}

static void vesaDrawVerticalGradientRect(int x, int y, int w, int h, unsigned int c0, unsigned int c1) {
	int yy;
	for(yy = 0; yy < h; yy++) {
		unsigned int c = desktopLerpColor(c0, c1, (unsigned int)yy, (unsigned int)(h > 1 ? h - 1 : 1));
		vesaFillRect(x, y + yy, w, 1, c);
	}
}

static int desktopStartsWith(const char* text, const char* prefix) {
	int i = 0;
	while(prefix[i] != '\0') {
		char a = text[i];
		char b = prefix[i];
		if(a == '\0') return 0;
		if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
		if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
		if(a != b) return 0;
		i++;
	}
	return 1;
}

static void desktopFormatClock(char* out) {
	unsigned int total = pit_ticks / 100u;
	unsigned int h = (total / 3600u) % 24u;
	unsigned int m = (total / 60u) % 60u;
	unsigned int s = total % 60u;
	out[0] = (char)('0' + (h / 10u));
	out[1] = (char)('0' + (h % 10u));
	out[2] = ':';
	out[3] = (char)('0' + (m / 10u));
	out[4] = (char)('0' + (m % 10u));
	out[5] = ':';
	out[6] = (char)('0' + (s / 10u));
	out[7] = (char)('0' + (s % 10u));
	out[8] = '\0';
}

static int desktop_enabled = 0;
static int desktop_show_start = 0;
static int desktop_selected_icon = 0;
static int desktop_active_app = 1;
static int desktop_win_x = 250;
static int desktop_win_y = 80;
static int desktop_dragging = 0;
static int desktop_drag_off_x = 0;
static int desktop_drag_off_y = 0;
static int desktop_prev_left = 0;
static int desktop_last_click_icon = -1;
static unsigned int desktop_last_click_tick = 0;
static unsigned int desktop_last_clock_sec = 0xFFFFFFFFu;
static int desktop_needs_redraw = 0;

static const char* desktop_icon_labels[6] = {
	"My Computer", "Recycle Bin", "My Documents",
	"Notepad", "Internet Explorer", "Control Panel"
};

static int desktopIconToApp(int icon_idx) {
	switch(icon_idx) {
		case 0: return 1;
		case 1: return 2;
		case 2: return 3;
		case 3: return 4;
		case 4: return 5;
		case 5: return 6;
		default: return 1;
	}
}

static int desktopPointInRect(int px, int py, int x, int y, int w, int h) {
	return (px >= x && py >= y && px < (x + w) && py < (y + h));
}

static int desktopHitTestIcon(int px, int py) {
	int k;
	for(k = 0; k < 6; k++) {
		int ix = 26;
		int iy = 82 + k * 92;
		if(desktopPointInRect(px, py, ix, iy, 32, 32) ||
		   desktopPointInRect(px, py, ix - 8, iy + 38, 82, 18)) {
			return k;
		}
	}
	return -1;
}

static void desktopApplyOption(const char* mode_opt) {
	if(!mode_opt || mode_opt[0] == '\0') return;
	if(desktopStartsWith(mode_opt, "START")) {
		desktop_show_start = 1;
		desktop_active_app = 0;
	} else if(desktopStartsWith(mode_opt, "MYCOMPUTER")) {
		desktop_active_app = 1; desktop_selected_icon = 0;
	} else if(desktopStartsWith(mode_opt, "RECYCLE")) {
		desktop_active_app = 2; desktop_selected_icon = 1;
	} else if(desktopStartsWith(mode_opt, "DOC")) {
		desktop_active_app = 3; desktop_selected_icon = 2;
	} else if(desktopStartsWith(mode_opt, "NOTE")) {
		desktop_active_app = 4; desktop_selected_icon = 3;
	} else if(desktopStartsWith(mode_opt, "IE") || desktopStartsWith(mode_opt, "INTERNET")) {
		desktop_active_app = 5; desktop_selected_icon = 4;
	} else if(desktopStartsWith(mode_opt, "CONTROL")) {
		desktop_active_app = 6; desktop_selected_icon = 5;
	} else if(desktopStartsWith(mode_opt, "HELP")) {
		desktop_active_app = 7;
	} else if(desktopStartsWith(mode_opt, "SHUT")) {
		desktop_active_app = 8;
		desktop_show_start = 1;
	}
}

static void vesaDrawDesktopIcon(int x, int y, const char* label, int selected) {
	// Drop shadow
	vesaFillRect(x + 2, y + 2, 32, 32, 0x111111);
	// Icon body (white)
	vesaFillRect(x, y, 32, 32, 0xF0F0F0);
	vesaDraw3DFrame(x, y, 32, 32, 0xFFFFFF, 0x7A7A7A);
	vesaFillRect(x + 6, y + 6, 20, 12, 0xD0D0D0);
	vesaFillRect(x + 6, y + 20, 20, 6, 0xC0C0C0);
	vesaDraw3DFrame(x + 6, y + 6, 20, 20, 0xFFFFFF, 0x808080);

	if(selected) {
		vesaFillRect(x - 8, y + 38, 82, 18, 0x742424);
		vesaDrawDottedRect(x - 8, y + 38, 82, 18, 0xFF4A4A);
	}
	vesaDrawTextTransparent(x - 4 + 1, y + 42 + 1, label, 0x000000);
	vesaDrawTextTransparent(x - 4, y + 42, label, 0xFFFFFF);
}

static void vesaDrawClassicWindow(int x, int y, int w, int h, const char* title, int active) {
	unsigned int bg = 0xC0C0C0;
	unsigned int hi = 0xDFDFDF;
	unsigned int lo = 0x404040;
	vesaFillRect(x, y, w, h, bg);
	vesaDraw3DFrame(x, y, w, h, hi, lo);
	vesaDraw3DFrame(x + 1, y + 1, w - 2, h - 2, 0xFFFFFF, 0x707070);

	if(active) {
		vesaDrawVerticalGradientRect(x + 4, y + 4, w - 8, 22, 0x5A0F0F, 0x9B2020);
	} else {
		vesaDrawVerticalGradientRect(x + 4, y + 4, w - 8, 22, 0x7A7A7A, 0x9A9A9A);
	}
	vesaDrawText((x + 10) / 8, (y + 8) / 16, (char*)title, active ? 0x0F : 0x0F);

	// Window controls
	vesaFillRect(x + w - 70, y + 6, 18, 16, 0xC0C0C0);
	vesaFillRect(x + w - 48, y + 6, 18, 16, 0xC0C0C0);
	vesaFillRect(x + w - 26, y + 6, 18, 16, 0xC0C0C0);
	vesaDraw3DFrame(x + w - 70, y + 6, 18, 16, 0xFFFFFF, 0x404040);
	vesaDraw3DFrame(x + w - 48, y + 6, 18, 16, 0xFFFFFF, 0x404040);
	vesaDraw3DFrame(x + w - 26, y + 6, 18, 16, 0xFFFFFF, 0x404040);
	vesaFillRect(x + w - 66, y + 17, 10, 2, 0x202020); // minimize
	vesaDraw3DFrame(x + w - 44, y + 10, 10, 8, 0x202020, 0x202020); // maximize
	vesaFillRect(x + w - 22, y + 10, 10, 2, 0x202020); // close X
	vesaFillRect(x + w - 18, y + 14, 2, 6, 0x202020);

	// Menu bar + content + status
	vesaFillRect(x + 4, y + 28, w - 8, 20, 0xC0C0C0);
	vesaDrawText((x + 10) / 8, (y + 32) / 16, "File  Edit  View  Help", 0x70);
	vesaFillRect(x + 4, y + 50, w - 8, h - 78, 0xFFFFFF);
	vesaDraw3DFrame(x + 4, y + 50, w - 8, h - 78, 0x707070, 0xE8E8E8);
	vesaFillRect(x + 4, y + h - 24, w - 8, 20, 0xC0C0C0);
	vesaDraw3DFrame(x + 4, y + h - 24, w - 8, 20, 0x707070, 0xF8F8F8);
	vesaDrawText((x + 10) / 8, (y + h - 20) / 16, "Ready", 0x70);
}

static void vesaDrawDesktopMainWindow(int app_id) {
	int x = desktop_win_x;
	int y = desktop_win_y;
	int w = 520;
	int h = 360;

	// Inactive background window (z-index cue)
	vesaDrawClassicWindow(x + 90, y + 40, 320, 230, "Internet Explorer", 0);
	vesaDrawText((x + 110) / 8, (y + 108) / 16, "Cannot connect to server.", 0xF0);

	switch(app_id) {
		case 1: vesaDrawClassicWindow(x, y, w, h, "My Computer", 1); break;
		case 2: vesaDrawClassicWindow(x, y, w, h, "Recycle Bin", 1); break;
		case 3: vesaDrawClassicWindow(x, y, w, h, w > 0 ? "My Documents" : "My Documents", 1); break;
		case 4: vesaDrawClassicWindow(x, y, w, h, "Notepad", 1); break;
		case 5: vesaDrawClassicWindow(x, y, w, h, "Internet Explorer", 1); break;
		case 6: vesaDrawClassicWindow(x, y, w, h, "Control Panel", 1); break;
		case 7: vesaDrawClassicWindow(x, y, w, h, "Help", 1); break;
		case 8: vesaDrawClassicWindow(x + 80, y + 90, 360, 220, "Shut Down Windows", 1); break;
		default: vesaDrawClassicWindow(x, y, w, h, "My Computer", 1); break;
	}

	if(app_id == 1 || app_id == 0) {
		vesaDrawText((x + 24) / 8, (y + 72) / 16, "[C:] System", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 96) / 16, "[D:] Data", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 120) / 16, "[DIR] Documents", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 144) / 16, "[DIR] Programs", 0xF0);
	}
	if(app_id == 2) {
		vesaDrawText((x + 24) / 8, (y + 90) / 16, "Recycle Bin is empty.", 0xF0);
	}
	if(app_id == 3) {
		vesaDrawText((x + 24) / 8, (y + 90) / 16, "resume.txt", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 114) / 16, "todo.txt", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 138) / 16, "notes.doc", 0xF0);
	}
	if(app_id == 4) {
		vesaDrawText((x + 24) / 8, (y + 72) / 16, "C:\\> Notepad", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 96) / 16, "Retro UI spec loaded.", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 120) / 16, "Press ESC to return to shell.", 0xF0);
	}
	if(app_id == 5) {
		vesaDrawText((x + 24) / 8, (y + 90) / 16, "Cannot connect to server.", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 114) / 16, "Check DHCP, DNS, HTTPGET.", 0xF0);
	}
	if(app_id == 6) {
		vesaDrawText((x + 24) / 8, (y + 76) / 16, "Display", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 100) / 16, "Mouse", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 124) / 16, "Network", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 148) / 16, "System", 0xF0);
	}
	if(app_id == 7) {
		vesaDrawText((x + 24) / 8, (y + 72) / 16, "Desktop controls:", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 96) / 16, "DESKTOP START", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 120) / 16, "DESKTOP NOTEPAD", 0xF0);
		vesaDrawText((x + 24) / 8, (y + 144) / 16, "DESKTOP SHUTDOWN", 0xF0);
	}
	if(app_id == 8) {
		int sx = x + 80;
		int sy = y + 90;
		vesaDrawText((sx + 24) / 8, (sy + 70) / 16, "What do you want the computer to do?", 0xF0);
		vesaFillRect(sx + 26, sy + 100, 88, 26, 0xC0C0C0);
		vesaFillRect(sx + 132, sy + 100, 88, 26, 0xC0C0C0);
		vesaFillRect(sx + 238, sy + 100, 88, 26, 0xC0C0C0);
		vesaDraw3DFrame(sx + 26, sy + 100, 88, 26, 0xFFFFFF, 0x404040);
		vesaDraw3DFrame(sx + 132, sy + 100, 88, 26, 0xFFFFFF, 0x404040);
		vesaDraw3DFrame(sx + 238, sy + 100, 88, 26, 0xFFFFFF, 0x404040);
		vesaDrawText((sx + 38) / 8, (sy + 108) / 16, "Shut down", 0x70);
		vesaDrawText((sx + 152) / 8, (sy + 108) / 16, "Restart", 0x70);
		vesaDrawText((sx + 264) / 8, (sy + 108) / 16, "Stand by", 0x70);
	}
}

static void vesaDrawStartMenu(int screen_h) {
	int x = 2;
	int y = screen_h - 342;
	int w = 270;
	int h = 300;
	vesaFillRect(x, y, w, h, 0xC0C0C0);
	vesaDraw3DFrame(x, y, w, h, 0xFFFFFF, 0x404040);
	vesaFillRect(x + 4, y + 4, 32, h - 8, 0x651717);
	vesaDrawText((x + 10) / 8, (y + 26) / 16, "R", 0x4F);
	vesaDrawText((x + 10) / 8, (y + 46) / 16, "e", 0x4F);
	vesaDrawText((x + 10) / 8, (y + 66) / 16, "d", 0x4F);
	vesaDrawText((x + 10) / 8, (y + 86) / 16, "O", 0x4F);
	vesaDrawText((x + 10) / 8, (y + 106) / 16, "S", 0x4F);
	vesaDrawText((x + 10) / 8, (y + 146) / 16, "9", 0x4F);
	vesaDrawText((x + 10) / 8, (y + 166) / 16, "8", 0x4F);

	vesaFillRect(x + 42, y + 24, 220, 24, 0x8B2525);
	vesaDrawText((x + 52) / 8, (y + 30) / 16, "Programs", 0x0F);
	vesaDrawText((x + 52) / 8, (y + 58) / 16, "Documents", 0x70);
	vesaDrawText((x + 52) / 8, (y + 86) / 16, "Settings", 0x70);
	vesaDrawText((x + 52) / 8, (y + 114) / 16, "Find", 0x70);
	vesaDrawText((x + 52) / 8, (y + 142) / 16, "Help", 0x70);
	vesaDrawText((x + 52) / 8, (y + 170) / 16, "Run", 0x70);
	vesaFillRect(x + 46, y + 214, 212, 1, 0x808080);
	vesaFillRect(x + 46, y + 215, 212, 1, 0xFFFFFF);
	vesaDrawText((x + 52) / 8, (y + 234) / 16, "Shut Down", 0x70);
}

static void vesaDrawTaskbar(int screen_w, int screen_h, int start_open, int active_app) {
	char clock_buf[9];
	desktopFormatClock(clock_buf);

	vesaFillRect(0, screen_h - 40, screen_w, 40, 0xC0C0C0);
	vesaFillRect(0, screen_h - 40, screen_w, 1, 0xDFDFDF);
	vesaFillRect(0, screen_h - 39, screen_w, 1, 0xFFFFFF);

	// Start button
	vesaFillRect(6, screen_h - 34, 90, 30, 0xC0C0C0);
	if(start_open) {
		vesaDraw3DFrame(6, screen_h - 34, 90, 30, 0x404040, 0xFFFFFF);
	} else {
		vesaDraw3DFrame(6, screen_h - 34, 90, 30, 0xFFFFFF, 0x404040);
	}
	vesaFillRect(14, screen_h - 28, 16, 16, 0x8B2525);
	vesaDrawText(3, (screen_h - 27) / 16, "R", 0x0F);
	vesaDrawText(5, (screen_h - 27) / 16, "Start", 0x70);

	// Open window buttons
	vesaFillRect(108, screen_h - 34, 170, 30, active_app == 4 ? 0xFFFFFF : 0xC0C0C0);
	vesaFillRect(286, screen_h - 34, 170, 30, active_app == 1 ? 0xFFFFFF : 0xC0C0C0);
	vesaDraw3DFrame(108, screen_h - 34, 170, 30, 0xFFFFFF, 0x404040);
	vesaDraw3DFrame(286, screen_h - 34, 170, 30, 0xFFFFFF, 0x404040);
	vesaDrawText(15, (screen_h - 26) / 16, "Notepad", 0x70);
	vesaDrawText(37, (screen_h - 26) / 16, "My Computer", 0x70);

	// Tray
	vesaFillRect(screen_w - 176, screen_h - 34, 170, 30, 0xB8B8B8);
	vesaDraw3DFrame(screen_w - 176, screen_h - 34, 170, 30, 0x808080, 0xFFFFFF);
	vesaFillRect(screen_w - 166, screen_h - 24, 10, 10, 0x404040); // volume
	vesaFillRect(screen_w - 150, screen_h - 24, 3, 10, 0x404040);
	vesaFillRect(screen_w - 145, screen_h - 22, 3, 8, 0x404040);
	vesaFillRect(screen_w - 140, screen_h - 20, 3, 6, 0x404040);
	vesaFillRect(screen_w - 124, screen_h - 24, 12, 3, 0x404040); // wifi
	vesaFillRect(screen_w - 122, screen_h - 20, 8, 3, 0x404040);
	vesaFillRect(screen_w - 120, screen_h - 16, 4, 3, 0x404040);
	vesaDrawText((screen_w - 92) / 8, (screen_h - 26) / 16, clock_buf, 0x70);
}

static void vgaDrawDesktopUi(const char* mode_opt) {
	unsigned int x, y;
	unsigned int width = vesa_width;
	unsigned int height = vesa_height;
	unsigned int c0 = 0x5C1A1A;
	unsigned int c1 = 0x8B2525;
	unsigned int c2 = 0x4A1515;
	int k;

	if(mode_opt && mode_opt[0] != '\0') desktopApplyOption(mode_opt);
	if(!desktop_enabled) return;

	// Desktop background gradient.
	for(y = 0; y < height; y++) {
		unsigned int c;
		if(y < (height / 2u)) {
			c = desktopLerpColor(c0, c1, y, (height / 2u) ? (height / 2u) : 1u);
		} else {
			c = desktopLerpColor(c1, c2, y - (height / 2u), (height / 2u) ? (height / 2u) : 1u);
		}
		for(x = 0; x < width; x++) {
			vesaStorePixelIndex(y * (vesa_pitch >> 2) + x, c);
		}
	}

	// Desktop icons on the left side.
	for(k = 0; k < 6; k++) {
		vesaDrawDesktopIcon(26, 82 + k * 92, desktop_icon_labels[k], k == desktop_selected_icon);
	}

	vesaDrawDesktopMainWindow(desktop_active_app);
	if(desktop_show_start) vesaDrawStartMenu((int)height);
	vesaDrawTaskbar((int)width, (int)height, desktop_show_start, desktop_active_app);
	desktopPresent();
}

static int desktopIsActive() {
	return desktop_enabled;
}

static void desktopLaunch(const char* mode_opt) {
	if(vgaEnterVesaMode(VESA_DEFAULT_WIDTH, VESA_DEFAULT_HEIGHT, VESA_DEFAULT_BPP)) {
		desktop_enabled = 1;
		desktop_backbuffer_enabled = 0;
		desktop_show_start = 0;
		desktop_dragging = 0;
		mouse_x = (int)(vesa_width / 2);
		mouse_y = (int)(vesa_height / 2);
		desktopApplyOption(mode_opt);
		desktop_last_clock_sec = 0xFFFFFFFFu;
		desktop_needs_redraw = 1;
		vgaDrawDesktopUi("");
	} else {
		printString("\nDesktop unavailable: VESA mode not supported.\n");
	}
}

static void desktopHandleKey(int code, int is_extended) {
	if(!desktop_enabled) return;

	if(is_extended) {
		if(code == 0x48 && desktop_selected_icon > 0) { // Up
			desktop_selected_icon--;
			desktop_needs_redraw = 1;
		}
		else if(code == 0x50 && desktop_selected_icon < 5) { // Down
			desktop_selected_icon++;
			desktop_needs_redraw = 1;
		}
		return;
	}

	if(code == 0x1C) { // Enter
		desktop_active_app = desktopIconToApp(desktop_selected_icon);
		desktop_show_start = 0;
		desktop_needs_redraw = 1;
	}
	else if(code == 0x3B) { // F1
		desktop_show_start = !desktop_show_start;
		desktop_needs_redraw = 1;
	}
	else if(code == 0x0F) { // Tab
		desktop_active_app++;
		if(desktop_active_app > 6) desktop_active_app = 1;
		desktop_needs_redraw = 1;
	}
	else if(code == 0x3C) { // F2
		desktop_active_app = 4;
		desktop_show_start = 0;
		desktop_needs_redraw = 1;
	}
}

static void desktopTick() {
	int left;
	int px, py;
	unsigned int sec;
	int h;
	int icon;
	unsigned int now;

	if(!desktop_enabled || !vgaIsGraphicsMode()) return;

	sec = pit_ticks / 100u;
	if(sec != desktop_last_clock_sec) {
		desktop_last_clock_sec = sec;
		desktop_needs_redraw = 1;
	}

	px = mouse_x;
	py = mouse_y;
	h = (int)vesa_height;
	left = (mouse_buttons & 1) ? 1 : 0;

	if(left && !desktop_prev_left) {
		// Start button
		if(desktopPointInRect(px, py, 6, h - 34, 90, 30)) {
			desktop_show_start = !desktop_show_start;
			desktop_needs_redraw = 1;
		}
		// Taskbar buttons
		else if(desktopPointInRect(px, py, 108, h - 34, 170, 30)) {
			desktop_active_app = 4;
			desktop_show_start = 0;
			desktop_needs_redraw = 1;
		}
		else if(desktopPointInRect(px, py, 286, h - 34, 170, 30)) {
			desktop_active_app = 1;
			desktop_show_start = 0;
			desktop_needs_redraw = 1;
		}
		else if(desktop_show_start && desktopPointInRect(px, py, 44, h - 342 + 228, 214, 24)) {
			desktop_active_app = 8;
			desktop_show_start = 0;
			desktop_needs_redraw = 1;
		}
		else if(desktop_show_start && desktopPointInRect(px, py, 44, h - 342 + 136, 214, 24)) {
			desktop_active_app = 7;
			desktop_show_start = 0;
			desktop_needs_redraw = 1;
		}
		else {
			icon = desktopHitTestIcon(px, py);
			if(icon >= 0) {
				desktop_selected_icon = icon;
				now = pit_ticks;
				if(desktop_last_click_icon == icon && (now - desktop_last_click_tick) <= 40u) {
					desktop_active_app = desktopIconToApp(icon);
					desktop_show_start = 0;
					desktop_last_click_icon = -1;
				} else {
					desktop_last_click_icon = icon;
					desktop_last_click_tick = now;
				}
				desktop_needs_redraw = 1;
			} else {
				// Window title bar drag
				if(desktopPointInRect(px, py, desktop_win_x + 4, desktop_win_y + 4, 512, 22)) {
					desktop_dragging = 1;
					desktop_drag_off_x = px - desktop_win_x;
					desktop_drag_off_y = py - desktop_win_y;
				} else if(desktop_show_start) {
					desktop_show_start = 0;
					desktop_needs_redraw = 1;
				}
			}
		}
	}

	if(left && desktop_dragging) {
		int nx = px - desktop_drag_off_x;
		int ny = py - desktop_drag_off_y;
		if(nx < 220) nx = 220;
		if(ny < 20) ny = 20;
		if(nx > ((int)vesa_width - 540)) nx = (int)vesa_width - 540;
		if(ny > ((int)vesa_height - 410)) ny = (int)vesa_height - 410;
		if(nx != desktop_win_x || ny != desktop_win_y) {
			desktop_win_x = nx;
			desktop_win_y = ny;
			desktop_needs_redraw = 1;
		}
	}

	if(!left) desktop_dragging = 0;
	desktop_prev_left = left;

	if(desktop_needs_redraw) {
		desktop_needs_redraw = 0;
		vgaDrawDesktopUi("");
	}
}

static int vgaEnterVesaMode(unsigned short width, unsigned short height, unsigned short bpp) {
	if(!vbeAvailable()) {
		return 0;
	}
	vbeWriteReg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	vbeWriteReg(VBE_DISPI_INDEX_XRES, width);
	vbeWriteReg(VBE_DISPI_INDEX_YRES, height);
	vbeWriteReg(VBE_DISPI_INDEX_BPP, bpp);
	vbeWriteReg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
	// Verify mode latch.
	if((vbeReadReg(VBE_DISPI_INDEX_ENABLE) & VBE_DISPI_ENABLED) == 0) {
		vbeWriteReg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
		vga_graphics_enabled = 0;
		return 0;
	}
	if(vbeReadReg(VBE_DISPI_INDEX_XRES) != width ||
	   vbeReadReg(VBE_DISPI_INDEX_YRES) != height ||
	   vbeReadReg(VBE_DISPI_INDEX_BPP) != bpp) {
		vbeWriteReg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
		vga_graphics_enabled = 0;
		return 0;
	}
	vesa_width = width;
	vesa_height = height;
	vesa_bpp = bpp;
	vesa_pitch = vesa_width * 4;
	// QEMU -vga std typically exposes LFB at PCI BAR space (0xFD000000).
	vesa_active_lfb = vesa_lfb_alt;
	vga_graphics_enabled = 1;
	vgaClearGraphics(0);
	vesaRedrawTextScreen();
	return 1;
}

void vgaEnterGraphicsMode() {
	(void)vgaEnterVesaMode(VESA_DEFAULT_WIDTH, VESA_DEFAULT_HEIGHT, VESA_DEFAULT_BPP);
}

void vgaEnterTextMode() {
	vbeWriteReg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	vgaWriteRegisters(vga_mode_text_regs);
	vga_graphics_enabled = 0;
	desktop_enabled = 0;
	desktop_backbuffer_enabled = 0;
	vesa_width = 0;
	vesa_height = 0;
	vesa_bpp = 0;
	vesa_pitch = 0;
	TM_START = (char*)0xB8000;
	cls();
	setMonitorColor((char)currentColorAttr);
}

void vgaExitToShell() {
	char prompt[] = "OS0 > ";
	vgaEnterTextMode();
	printString("Returned to text mode.\n");
	printColorString(prompt, 0xaf);
}

char strcmp(char* sou , char* dest){
	int i = 0;
	while(*(sou + i) == *(dest + i)){
		if(*(sou + i) == 0 && *(dest + i) == 0)
			return 1;
		i++;
	}
	return 0;
}

char strcmpi(char* sou, char* dest) {
	int i = 0;
	while(1) {
		char a = *(sou + i);
		char b = *(dest + i);
		if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
		if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
		if(a != b) return 0;
		if(a == 0 && b == 0) return 1;
		i++;
	}
}

char startsWithi(char* text, char* prefix) {
	int i = 0;
	while(prefix[i] != '\0') {
		char a = text[i];
		char b = prefix[i];
		if(a == '\0') return 0;
		if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
		if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
		if(a != b) return 0;
		i++;
	}
	return 1;
}

void normalizeRepeat(char* text) {
	int len = 0;
	while(text[len] != '\0') len++;
	if(len < 2 || (len % 2) != 0) return;
	int half = len / 2;
	int i;
	for(i = 0; i < half; i++) {
		char a = text[i];
		char b = text[i + half];
		if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
		if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
		if(a != b) return;
	}
	text[half] = '\0';
}

// Parse color name and return VGA color value (0-15), or -1 if invalid
int parseColorName(char* name) {
	if(strcmp(name, "BLACK")) return 0x0;
	if(strcmp(name, "BLUE")) return 0x1;
	if(strcmp(name, "GREEN")) return 0x2;
	if(strcmp(name, "CYAN")) return 0x3;
	if(strcmp(name, "RED")) return 0x4;
	if(strcmp(name, "MAGENTA")) return 0x5;
	if(strcmp(name, "BROWN")) return 0x6;
	if(strcmp(name, "GRAY")) return 0x7;
	if(strcmp(name, "DARKGRAY")) return 0x8;
	if(strcmp(name, "LIGHTBLUE")) return 0x9;
	if(strcmp(name, "LIGHTGREEN")) return 0xa;
	if(strcmp(name, "LIGHTCYAN")) return 0xb;
	if(strcmp(name, "LIGHTRED")) return 0xc;
	if(strcmp(name, "PINK")) return 0xd;
	if(strcmp(name, "YELLOW")) return 0xe;
	if(strcmp(name, "WHITE")) return 0xf;
	return -1;  // Invalid color
}

// Set color and remember it
void setColorAndRemember(unsigned char color) {
	currentColorAttr = color;
	setMonitorColor(color);
}

// Audio: PC speaker control
void speakerOn(unsigned int freq) {
	if(freq < 20) freq = 20;
	unsigned int div = 1193180 / freq;
	outportb(0x43, 0xB6);
	outportb(0x42, (unsigned char)(div & 0xFF));
	outportb(0x42, (unsigned char)((div >> 8) & 0xFF));
	outportb(0x61, inportb(0x61) | 3);
}

void speakerOff() {
	outportb(0x61, inportb(0x61) & 0xFC);
}

void beep(unsigned int freq, unsigned int ms) {
	speakerOn(freq);
	volatile unsigned int i;
	while(ms--) {
		for(i = 0; i < 5000; i++);
	}
	speakerOff();
}

// PIT timer tick counter (defined in main.c, incremented by IRQ0 at 100 Hz)
extern volatile unsigned int pit_ticks;

// CPUID instruction wrapper
static void cpuid(unsigned int code, unsigned int* a, unsigned int* b, unsigned int* c, unsigned int* d) {
	__asm__ __volatile__(
		"cpuid"
		: "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
		: "a"(code)
	);
}

// Check if CPUID is supported
static int cpuid_supported() {
	unsigned int flags1, flags2;
	__asm__ __volatile__(
		"pushfl\n\t"
		"pushfl\n\t"
		"popl %0\n\t"
		"movl %0, %1\n\t"
		"xorl $0x200000, %0\n\t"
		"pushl %0\n\t"
		"popfl\n\t"
		"pushfl\n\t"
		"popl %0\n\t"
		"popfl\n\t"
		: "=r"(flags1), "=r"(flags2)
	);
	return ((flags1 ^ flags2) & 0x200000) != 0;
}

// Print a number in decimal
void printNumber(unsigned int num) {
	char buf[12];
	int i = 0;
	if(num == 0) {
		printChar('0');
		return;
	}
	while(num > 0) {
		buf[i++] = '0' + (num % 10);
		num /= 10;
	}
	while(i > 0) {
		printChar(buf[--i]);
	}
}

// Print a number in hex
void printHex(unsigned int num) {
	char hex[] = "0123456789ABCDEF";
	printString("0x");
	for(int i = 28; i >= 0; i -= 4) {
		printChar(hex[(num >> i) & 0xF]);
	}
}

void printHexByte(unsigned char v) {
	char hex[] = "0123456789ABCDEF";
	printChar(hex[(v >> 4) & 0xF]);
	printChar(hex[v & 0xF]);
}

// ============================================
// Networking (Ethernet + ARP + minimal IPv4)
// ============================================
#define ETH_TYPE_ARP   0x0806
#define ETH_TYPE_IPV4  0x0800
#define ARP_HTYPE_ETH  0x0001
#define ARP_PTYPE_IPV4 0x0800
#define ARP_HLEN_ETH   6
#define ARP_PLEN_IPV4  4
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2
#define ARP_CACHE_SIZE 8
#define IP_VERSION_IHL 0x45
#define IP_PROTO_TEST  0xFD
#define IP_PROTO_ICMP  1
#define IP_PROTO_UDP   17
#define IP_PROTO_TCP   6
#define IP_TTL_DEFAULT 64
#define UDP_SOCKET_MAX 4
#define UDP_QUEUE_MAX  8
#define UDP_DATA_MAX   512
#define DNS_PORT       53
#define DNS_SRC_PORT   53053
#define DNS_NAME_MAX   128
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_OPT_PAD     0
#define DHCP_OPT_SUBNET  1
#define DHCP_OPT_ROUTER  3
#define DHCP_OPT_DNS     6
#define DHCP_OPT_REQ_IP  50
#define DHCP_OPT_LEASE   51
#define DHCP_OPT_MSGTYPE 53
#define DHCP_OPT_SERVER  54
#define DHCP_OPT_PARAMRL 55
#define DHCP_OPT_CLIENT  61
#define DHCP_OPT_END     255
#define DHCP_MSG_DISCOVER 1
#define DHCP_MSG_OFFER    2
#define DHCP_MSG_REQUEST  3
#define DHCP_MSG_ACK      5
#define DHCP_MSG_NAK      6

struct arp_cache_entry {
	unsigned char valid;
	unsigned char ip[4];
	unsigned char mac[6];
	unsigned int last_seen_ticks;
};

static unsigned char net_local_ip[4] = {10, 0, 2, 15}; // Fallback/default if DHCP not available
static unsigned char net_subnet_mask[4] = {255, 255, 255, 0};
static unsigned char net_gateway_ip[4] = {10, 0, 2, 2};
static unsigned char net_dns_ip[4] = {10, 0, 2, 3};
static unsigned char net_dhcp_server_ip[4] = {0, 0, 0, 0};
static unsigned int net_dhcp_lease_seconds = 0;
static int net_dhcp_active = 0;
static struct arp_cache_entry arp_cache[ARP_CACHE_SIZE];
static unsigned short ip_ident_counter = 1;
static unsigned int dhcp_xid = 0;
static int dhcp_offer_ready = 0;
static int dhcp_ack_ready = 0;
static int dhcp_nak_seen = 0;
static unsigned char dhcp_offer_ip[4] = {0, 0, 0, 0};
static unsigned char dhcp_offer_server[4] = {0, 0, 0, 0};
static unsigned char dhcp_offer_subnet[4] = {255, 255, 255, 0};
static unsigned char dhcp_offer_router[4] = {0, 0, 0, 0};
static unsigned char dhcp_offer_dns[4] = {0, 0, 0, 0};
static unsigned int dhcp_offer_lease = 0;
static unsigned short ping_identifier = 0x4552;
static unsigned short ping_sequence = 0;
static int ping_waiting = 0;
static int ping_received = 0;
static unsigned int ping_sent_tick = 0;
static unsigned int ping_rtt_ticks = 0;
static unsigned char ping_target_ip[4] = {0, 0, 0, 0};
static unsigned char ping_reply_ip[4] = {0, 0, 0, 0};
static unsigned short dns_txid = 0;
static int dns_waiting = 0;
static int dns_done = 0;
static int dns_ok = 0;
static unsigned char dns_answer_ip[4] = {0, 0, 0, 0};
static unsigned char dns_server_ip_last[4] = {0, 0, 0, 0};
static char dns_query_name[DNS_NAME_MAX];

void handleIPv4Frame(const unsigned char* frame, unsigned short len, int verbose);
int dnsResolveA(const char* host, const unsigned char dns_server_ip[4], unsigned char out_ip[4], int verbose);

struct udp_socket_entry {
	unsigned char valid;
	unsigned short local_port;
};

struct udp_rx_entry {
	unsigned char valid;
	unsigned short dst_port;
	unsigned short src_port;
	unsigned short len;
	unsigned char src_ip[4];
	unsigned char data[UDP_DATA_MAX];
	unsigned int tick;
};

static struct udp_socket_entry udp_sockets[UDP_SOCKET_MAX];
static struct udp_rx_entry udp_rx_queue[UDP_QUEUE_MAX];

#define TCP_RX_MAX 1024
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_ST_CLOSED      0
#define TCP_ST_SYN_SENT    1
#define TCP_ST_ESTABLISHED 2
#define TCP_ST_FIN_WAIT1   3
#define TCP_ST_FIN_WAIT2   4
#define TCP_ST_CLOSE_WAIT  5
#define TCP_ST_LAST_ACK    6
#define TCP_ST_TIME_WAIT   7
#define TCP_RTO_TICKS      30   // 300 ms at 100 Hz
#define TCP_MAX_RETRY      5

struct tcp_conn {
	unsigned char valid;
	unsigned char state;
	unsigned short local_port;
	unsigned short remote_port;
	unsigned char remote_ip[4];
	unsigned int snd_una;   // oldest unacked seq
	unsigned int snd_nxt;   // next seq to send
	unsigned int rcv_nxt;   // next expected seq from peer
	unsigned short snd_wnd; // peer-advertised window
	unsigned int last_tick;
	unsigned int rto_tick;
	unsigned char retry_count;
	unsigned short tx_last_len;
	unsigned short tx_last_seq_advance;
	unsigned char tx_last[TCP_RX_MAX];
	unsigned short rx_len;
	unsigned char rx_buf[TCP_RX_MAX];
};

static struct tcp_conn tcp_client_conn;

int ip4Equal(const unsigned char* a, const unsigned char* b) {
	for(int i = 0; i < 4; i++) {
		if(a[i] != b[i]) return 0;
	}
	return 1;
}

int macEqual(const unsigned char* a, const unsigned char* b) {
	for(int i = 0; i < 6; i++) {
		if(a[i] != b[i]) return 0;
	}
	return 1;
}

void copyIp4(unsigned char* dst, const unsigned char* src) {
	for(int i = 0; i < 4; i++) dst[i] = src[i];
}

void copyMac(unsigned char* dst, const unsigned char* src) {
	for(int i = 0; i < 6; i++) dst[i] = src[i];
}

int ipIsZero(const unsigned char ip[4]) {
	return (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

int ipIsSameSubnet(const unsigned char a[4], const unsigned char b[4], const unsigned char mask[4]) {
	for(int i = 0; i < 4; i++) {
		if((a[i] & mask[i]) != (b[i] & mask[i])) return 0;
	}
	return 1;
}

unsigned int readU32BE(const unsigned char* p) {
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

void writeU32BE(unsigned char* p, unsigned int v) {
	p[0] = (unsigned char)((v >> 24) & 0xFF);
	p[1] = (unsigned char)((v >> 16) & 0xFF);
	p[2] = (unsigned char)((v >> 8) & 0xFF);
	p[3] = (unsigned char)(v & 0xFF);
}

void printIPv4(const unsigned char* ip) {
	for(int i = 0; i < 4; i++) {
		printNumber((unsigned int)ip[i]);
		if(i != 3) printChar('.');
	}
}

void printMac(const unsigned char* mac) {
	for(int i = 0; i < 6; i++) {
		printHexByte(mac[i]);
		if(i != 5) printChar(':');
	}
}

int parseIPv4(char* s, unsigned char out_ip[4]) {
	int part = 0;
	int value = 0;
	int have_digit = 0;
	for(int i = 0;; i++) {
		char c = s[i];
		if(c >= '0' && c <= '9') {
			value = (value * 10) + (c - '0');
			if(value > 255) return 0;
			have_digit = 1;
		} else if(c == '.' || c == '\0') {
			if(!have_digit || part >= 4) return 0;
			out_ip[part++] = (unsigned char)value;
			value = 0;
			have_digit = 0;
			if(c == '\0') break;
		} else if(c == ' ') {
			// Allow trailing spaces only.
			while(s[i] == ' ') i++;
			return (s[i] == '\0' && part == 4 && !have_digit) ? 1 : 0;
		} else {
			return 0;
		}
	}
	return part == 4;
}

int parseUnsignedDec(char* s, unsigned int* out) {
	unsigned int v = 0;
	int i = 0;
	if(s[0] == '\0') return 0;
	while(s[i] != '\0') {
		char c = s[i];
		if(c < '0' || c > '9') return 0;
		v = v * 10 + (unsigned int)(c - '0');
		i++;
	}
	*out = v;
	return 1;
}

char* skipSpaces(char* p) {
	while(*p == ' ') p++;
	return p;
}

int nextToken(char** p, char* out, int out_max) {
	int i = 0;
	char* s = skipSpaces(*p);
	if(*s == '\0') return 0;
	while(*s != '\0' && *s != ' ') {
		if(i < (out_max - 1)) out[i++] = *s;
		s++;
	}
	out[i] = '\0';
	*p = s;
	return 1;
}

int parsePort(char* s, unsigned short* out_port) {
	unsigned int v;
	if(!parseUnsignedDec(s, &v)) return 0;
	if(v > 65535) return 0;
	*out_port = (unsigned short)v;
	return 1;
}

int arpCacheFindByIp(const unsigned char ip[4]) {
	for(int i = 0; i < ARP_CACHE_SIZE; i++) {
		if(arp_cache[i].valid && ip4Equal(arp_cache[i].ip, ip)) {
			return i;
		}
	}
	return -1;
}

void arpCacheUpsert(const unsigned char ip[4], const unsigned char mac[6]) {
	int slot = -1;
	unsigned int oldest = 0xFFFFFFFF;
	int oldest_slot = 0;

	// Update existing entry first.
	for(int i = 0; i < ARP_CACHE_SIZE; i++) {
		if(arp_cache[i].valid && ip4Equal(arp_cache[i].ip, ip)) {
			copyMac(arp_cache[i].mac, mac);
			arp_cache[i].last_seen_ticks = pit_ticks;
			return;
		}
	}

	// Find first free slot.
	for(int i = 0; i < ARP_CACHE_SIZE; i++) {
		if(!arp_cache[i].valid) {
			slot = i;
			break;
		}
	}

	// If full, evict oldest.
	if(slot < 0) {
		for(int i = 0; i < ARP_CACHE_SIZE; i++) {
			if(arp_cache[i].last_seen_ticks < oldest) {
				oldest = arp_cache[i].last_seen_ticks;
				oldest_slot = i;
			}
		}
		slot = oldest_slot;
	}

	arp_cache[slot].valid = 1;
	copyIp4(arp_cache[slot].ip, ip);
	copyMac(arp_cache[slot].mac, mac);
	arp_cache[slot].last_seen_ticks = pit_ticks;
}

int arpCacheLookup(const unsigned char ip[4], unsigned char out_mac[6]) {
	int idx = arpCacheFindByIp(ip);
	if(idx < 0) return 0;
	copyMac(out_mac, arp_cache[idx].mac);
	arp_cache[idx].last_seen_ticks = pit_ticks;
	return 1;
}

void arpCacheShow() {
	int shown = 0;
	printString("\nARP cache entries:\n");
	for(int i = 0; i < ARP_CACHE_SIZE; i++) {
		if(!arp_cache[i].valid) continue;
		printString("  ");
		printIPv4(arp_cache[i].ip);
		printString(" -> ");
		printMac(arp_cache[i].mac);
		printString("  (age ticks: ");
		printNumber((unsigned int)(pit_ticks - arp_cache[i].last_seen_ticks));
		printString(")\n");
		shown++;
	}
	if(shown == 0) {
		printString("  (empty)\n");
	}
}

int netBuildAndSendArp(unsigned short op, const unsigned char target_mac[6], const unsigned char target_ip[4]) {
	unsigned char frame[60];
	unsigned char local_mac[6];
	unsigned char zero_mac[6] = {0, 0, 0, 0, 0, 0};
	const unsigned char* eth_dst = (op == ARP_OP_REQUEST) ? (const unsigned char*)"\xFF\xFF\xFF\xFF\xFF\xFF" : target_mac;
	const unsigned char* arp_tgt_mac = (op == ARP_OP_REQUEST) ? zero_mac : target_mac;

	if(!ne2k_is_ready()) return 0;
	if(op != ARP_OP_REQUEST && op != ARP_OP_REPLY) return 0;

	ne2k_get_mac(local_mac);

	// Ethernet header: dst(6), src(6), type(2=ARP)
	for(int i = 0; i < 6; i++) frame[i] = eth_dst[i];
	for(int i = 0; i < 6; i++) frame[6 + i] = local_mac[i];
	frame[12] = 0x08;
	frame[13] = 0x06;

	// ARP payload
	frame[14] = 0x00; frame[15] = 0x01; // Hardware type: Ethernet
	frame[16] = 0x08; frame[17] = 0x00; // Protocol type: IPv4
	frame[18] = 6;    frame[19] = 4;    // HLEN / PLEN
	frame[20] = (unsigned char)((op >> 8) & 0xFF);
	frame[21] = (unsigned char)(op & 0xFF);
	for(int i = 0; i < 6; i++) frame[22 + i] = local_mac[i];      // Sender MAC
	for(int i = 0; i < 4; i++) frame[28 + i] = net_local_ip[i];   // Sender IP
	for(int i = 0; i < 6; i++) frame[32 + i] = arp_tgt_mac[i];    // Target MAC
	for(int i = 0; i < 4; i++) frame[38 + i] = target_ip[i];      // Target IP

	for(int i = 42; i < 60; i++) frame[i] = 0; // Ethernet minimum frame size
	return ne2k_send_frame(frame, 60);
}

void handleArpFrame(const unsigned char* frame, unsigned short len, int verbose) {
	unsigned short htype, ptype, oper;
	const unsigned char* sender_mac;
	const unsigned char* sender_ip;
	const unsigned char* target_mac;
	const unsigned char* target_ip;

	if(len < 42) return;
	htype = (unsigned short)(((unsigned short)frame[14] << 8) | frame[15]);
	ptype = (unsigned short)(((unsigned short)frame[16] << 8) | frame[17]);
	oper  = (unsigned short)(((unsigned short)frame[20] << 8) | frame[21]);
	if(htype != ARP_HTYPE_ETH || ptype != ARP_PTYPE_IPV4) return;
	if(frame[18] != ARP_HLEN_ETH || frame[19] != ARP_PLEN_IPV4) return;

	sender_mac = &frame[22];
	sender_ip = &frame[28];
	target_mac = &frame[32];
	target_ip = &frame[38];

	// Cache sender mapping from both requests and replies.
	arpCacheUpsert(sender_ip, sender_mac);

	if(verbose) {
		printString("\nARP: ");
		if(oper == ARP_OP_REQUEST) printString("request ");
		else if(oper == ARP_OP_REPLY) printString("reply ");
		else printString("op ");
		printIPv4(sender_ip);
		printString(" is-at ");
		printMac(sender_mac);
		printChar('\n');
	}

	// Reply to ARP requests for our local IP.
	if(oper == ARP_OP_REQUEST && ip4Equal(target_ip, net_local_ip)) {
		int sent = netBuildAndSendArp(ARP_OP_REPLY, sender_mac, sender_ip);
		(void)target_mac; // Kept for clarity while parsing full ARP payload.
		if(verbose) {
			if(sent > 0) {
				printString("ARP: replied for ");
				printIPv4(net_local_ip);
				printChar('\n');
			} else {
				printString("ARP: reply send failed.\n");
			}
		}
	}
}

void processRxFrameProtocols(const unsigned char* frame, unsigned short len, int verbose) {
	unsigned short eth_type;
	if(len < 14) return;
	eth_type = (unsigned short)(((unsigned short)frame[12] << 8) | frame[13]);
	if(eth_type == ETH_TYPE_ARP) {
		handleArpFrame(frame, len, verbose);
	} else if(eth_type == ETH_TYPE_IPV4) {
		handleIPv4Frame(frame, len, verbose);
	}
}

int arpResolve(const unsigned char target_ip[4], unsigned char out_mac[6], unsigned int poll_budget) {
	unsigned char rx[1600];
	if(arpCacheLookup(target_ip, out_mac)) {
		return 1;
	}
	if(netBuildAndSendArp(ARP_OP_REQUEST, (const unsigned char*)"\x00\x00\x00\x00\x00\x00", target_ip) <= 0) {
		return 0;
	}
	while(poll_budget--) {
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) {
			processRxFrameProtocols(rx, (unsigned short)got, 0);
			if(arpCacheLookup(target_ip, out_mac)) return 1;
		}
	}
	return 0;
}

int udpSocketIsBound(unsigned short local_port) {
	for(int i = 0; i < UDP_SOCKET_MAX; i++) {
		if(udp_sockets[i].valid && udp_sockets[i].local_port == local_port) return 1;
	}
	return 0;
}

int udpSocketBind(unsigned short local_port) {
	for(int i = 0; i < UDP_SOCKET_MAX; i++) {
		if(udp_sockets[i].valid && udp_sockets[i].local_port == local_port) return 1;
	}
	for(int i = 0; i < UDP_SOCKET_MAX; i++) {
		if(!udp_sockets[i].valid) {
			udp_sockets[i].valid = 1;
			udp_sockets[i].local_port = local_port;
			return 1;
		}
	}
	return 0;
}

void udpSocketsShow() {
	int shown = 0;
	printString("\nUDP sockets:\n");
	for(int i = 0; i < UDP_SOCKET_MAX; i++) {
		if(!udp_sockets[i].valid) continue;
		printString("  port ");
		printNumber((unsigned int)udp_sockets[i].local_port);
		printChar('\n');
		shown++;
	}
	if(shown == 0) printString("  (none)\n");
}

void udpQueuePush(unsigned short dst_port, unsigned short src_port, const unsigned char src_ip[4], const unsigned char* data, unsigned short len) {
	int slot = -1;
	unsigned int oldest = 0xFFFFFFFF;
	int oldest_slot = 0;
	unsigned short copy_len = len;
	if(copy_len > UDP_DATA_MAX) copy_len = UDP_DATA_MAX;

	for(int i = 0; i < UDP_QUEUE_MAX; i++) {
		if(!udp_rx_queue[i].valid) {
			slot = i;
			break;
		}
	}
	if(slot < 0) {
		for(int i = 0; i < UDP_QUEUE_MAX; i++) {
			if(udp_rx_queue[i].tick < oldest) {
				oldest = udp_rx_queue[i].tick;
				oldest_slot = i;
			}
		}
		slot = oldest_slot;
	}

	udp_rx_queue[slot].valid = 1;
	udp_rx_queue[slot].dst_port = dst_port;
	udp_rx_queue[slot].src_port = src_port;
	udp_rx_queue[slot].len = copy_len;
	udp_rx_queue[slot].tick = pit_ticks;
	copyIp4(udp_rx_queue[slot].src_ip, src_ip);
	for(int i = 0; i < copy_len; i++) udp_rx_queue[slot].data[i] = data[i];
}

int udpQueuePop(unsigned short local_port, unsigned char out_src_ip[4], unsigned short* out_src_port, unsigned char* out_data, unsigned short max_len) {
	int idx = -1;
	unsigned int oldest = 0xFFFFFFFF;

	for(int i = 0; i < UDP_QUEUE_MAX; i++) {
		if(!udp_rx_queue[i].valid) continue;
		if(udp_rx_queue[i].dst_port != local_port) continue;
		if(udp_rx_queue[i].tick < oldest) {
			oldest = udp_rx_queue[i].tick;
			idx = i;
		}
	}
	if(idx < 0) return 0;

	{
		unsigned short copy_len = udp_rx_queue[idx].len;
		if(copy_len > max_len) copy_len = max_len;
		copyIp4(out_src_ip, udp_rx_queue[idx].src_ip);
		*out_src_port = udp_rx_queue[idx].src_port;
		for(int i = 0; i < copy_len; i++) out_data[i] = udp_rx_queue[idx].data[i];
		udp_rx_queue[idx].valid = 0;
		return (int)copy_len;
	}
}

unsigned short ipChecksum16(const unsigned char* data, unsigned int len) {
	unsigned int sum = 0;
	unsigned int i = 0;

	while(i + 1 < len) {
		unsigned short word = (unsigned short)(((unsigned short)data[i] << 8) | data[i + 1]);
		sum += (unsigned int)word;
		i += 2;
	}
	if(i < len) {
		sum += (unsigned int)((unsigned short)data[i] << 8);
	}
	while(sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return (unsigned short)(~sum);
}

void dhcpResetOfferState() {
	dhcp_offer_ready = 0;
	dhcp_ack_ready = 0;
	dhcp_nak_seen = 0;
	dhcp_offer_lease = 0;
	for(int i = 0; i < 4; i++) {
		dhcp_offer_ip[i] = 0;
		dhcp_offer_server[i] = 0;
		dhcp_offer_subnet[i] = 0;
		dhcp_offer_router[i] = 0;
		dhcp_offer_dns[i] = 0;
	}
}

int dhcpBuildAndSend(unsigned char msg_type, const unsigned char req_ip[4], const unsigned char server_ip[4]) {
	unsigned char frame[1518];
	unsigned char local_mac[6];
	unsigned short udp_len;
	unsigned short ip_len;
	unsigned short frame_len;
	unsigned short ck;
	unsigned short opt = 0;
	unsigned short payload_len;
	unsigned int xid;

	if(!ne2k_is_ready()) return 0;
	ne2k_get_mac(local_mac);
	xid = dhcp_xid;
	if(xid == 0) xid = 0x45524953; // "ERIS"

	// Ethernet header: broadcast
	for(int i = 0; i < 6; i++) frame[i] = 0xFF;
	for(int i = 0; i < 6; i++) frame[6 + i] = local_mac[i];
	frame[12] = 0x08;
	frame[13] = 0x00;

	// IPv4 header starts at 14, UDP header at 34, BOOTP starts at 42.
	// BOOTP fixed header (236 bytes) + magic cookie + options.
	for(int i = 42; i < 42 + 236 + 128; i++) frame[i] = 0;
	frame[42] = 1; // BOOTREQUEST
	frame[43] = 1; // Ethernet
	frame[44] = 6; // MAC length
	frame[45] = 0; // hops
	writeU32BE(&frame[46], xid);
	frame[50] = 0x00; frame[51] = 0x00; // secs
	frame[52] = 0x80; frame[53] = 0x00; // broadcast flag
	for(int i = 0; i < 6; i++) frame[70 + i] = local_mac[i]; // chaddr[16]

	// DHCP magic cookie
	frame[278] = 99; frame[279] = 130; frame[280] = 83; frame[281] = 99;
	opt = 282;

	// DHCP Message Type
	frame[opt++] = DHCP_OPT_MSGTYPE;
	frame[opt++] = 1;
	frame[opt++] = msg_type;

	// Client Identifier (type 1 + MAC)
	frame[opt++] = DHCP_OPT_CLIENT;
	frame[opt++] = 7;
	frame[opt++] = 1;
	for(int i = 0; i < 6; i++) frame[opt++] = local_mac[i];

	// Parameter Request List
	frame[opt++] = DHCP_OPT_PARAMRL;
	frame[opt++] = 5;
	frame[opt++] = DHCP_OPT_SUBNET;
	frame[opt++] = DHCP_OPT_ROUTER;
	frame[opt++] = DHCP_OPT_DNS;
	frame[opt++] = DHCP_OPT_LEASE;
	frame[opt++] = DHCP_OPT_SERVER;

	if(msg_type == DHCP_MSG_REQUEST && req_ip && server_ip) {
		frame[opt++] = DHCP_OPT_REQ_IP;
		frame[opt++] = 4;
		for(int i = 0; i < 4; i++) frame[opt++] = req_ip[i];

		frame[opt++] = DHCP_OPT_SERVER;
		frame[opt++] = 4;
		for(int i = 0; i < 4; i++) frame[opt++] = server_ip[i];
	}

	frame[opt++] = DHCP_OPT_END;

	payload_len = (unsigned short)(opt - 42);
	udp_len = (unsigned short)(8 + payload_len);
	ip_len = (unsigned short)(20 + udp_len);

	// IPv4 header (src 0.0.0.0, dst 255.255.255.255)
	frame[14] = IP_VERSION_IHL;
	frame[15] = 0x00;
	frame[16] = (unsigned char)((ip_len >> 8) & 0xFF);
	frame[17] = (unsigned char)(ip_len & 0xFF);
	frame[18] = (unsigned char)((ip_ident_counter >> 8) & 0xFF);
	frame[19] = (unsigned char)(ip_ident_counter & 0xFF);
	ip_ident_counter++;
	frame[20] = 0x00;
	frame[21] = 0x00;
	frame[22] = IP_TTL_DEFAULT;
	frame[23] = IP_PROTO_UDP;
	frame[24] = 0x00;
	frame[25] = 0x00;
	frame[26] = 0; frame[27] = 0; frame[28] = 0; frame[29] = 0;
	frame[30] = 255; frame[31] = 255; frame[32] = 255; frame[33] = 255;
	ck = ipChecksum16(&frame[14], 20);
	frame[24] = (unsigned char)((ck >> 8) & 0xFF);
	frame[25] = (unsigned char)(ck & 0xFF);

	// UDP header
	frame[34] = 0x00;
	frame[35] = DHCP_CLIENT_PORT;
	frame[36] = 0x00;
	frame[37] = DHCP_SERVER_PORT;
	frame[38] = (unsigned char)((udp_len >> 8) & 0xFF);
	frame[39] = (unsigned char)(udp_len & 0xFF);
	frame[40] = 0x00; // UDP checksum optional in IPv4
	frame[41] = 0x00;

	frame_len = (unsigned short)(14 + ip_len);
	return ne2k_send_frame(frame, frame_len);
}

void dhcpApplyOffer() {
	copyIp4(net_local_ip, dhcp_offer_ip);
	if(!(dhcp_offer_subnet[0] == 0 && dhcp_offer_subnet[1] == 0 &&
	     dhcp_offer_subnet[2] == 0 && dhcp_offer_subnet[3] == 0)) {
		copyIp4(net_subnet_mask, dhcp_offer_subnet);
	}
	if(!(dhcp_offer_router[0] == 0 && dhcp_offer_router[1] == 0 &&
	     dhcp_offer_router[2] == 0 && dhcp_offer_router[3] == 0)) {
		copyIp4(net_gateway_ip, dhcp_offer_router);
	}
	if(!(dhcp_offer_dns[0] == 0 && dhcp_offer_dns[1] == 0 &&
	     dhcp_offer_dns[2] == 0 && dhcp_offer_dns[3] == 0)) {
		copyIp4(net_dns_ip, dhcp_offer_dns);
	}
	copyIp4(net_dhcp_server_ip, dhcp_offer_server);
	net_dhcp_lease_seconds = dhcp_offer_lease;
	net_dhcp_active = 1;
}

int dhcpHandleUdpPayload(const unsigned char* data, unsigned short len, int verbose) {
	unsigned char local_mac[6];
	unsigned int xid;
	unsigned char msg_type = 0;
	unsigned int i;
	unsigned int lease_tmp = 0;
	unsigned char server_tmp[4] = {0, 0, 0, 0};
	unsigned char subnet_tmp[4] = {0, 0, 0, 0};
	unsigned char router_tmp[4] = {0, 0, 0, 0};
	unsigned char dns_tmp[4] = {0, 0, 0, 0};

	if(len < 240) return 0;
	if(data[0] != 2 || data[1] != 1 || data[2] != 6) return 0; // BOOTREPLY/Ethernet/MAC len

	ne2k_get_mac(local_mac);
	for(i = 0; i < 6; i++) {
		if(data[28 + i] != local_mac[i]) return 0;
	}

	xid = readU32BE(&data[4]);
	if(xid != dhcp_xid) return 0;

	if(!(data[236] == 99 && data[237] == 130 && data[238] == 83 && data[239] == 99)) return 0;

	i = 240;
	while(i < len) {
		unsigned char opt = data[i++];
		if(opt == DHCP_OPT_PAD) continue;
		if(opt == DHCP_OPT_END) break;
		if(i >= len) break;
		unsigned char olen = data[i++];
		if((unsigned int)i + (unsigned int)olen > (unsigned int)len) break;

		if(opt == DHCP_OPT_MSGTYPE && olen >= 1) {
			msg_type = data[i];
		} else if(opt == DHCP_OPT_SERVER && olen >= 4) {
			for(int k = 0; k < 4; k++) server_tmp[k] = data[i + k];
		} else if(opt == DHCP_OPT_SUBNET && olen >= 4) {
			for(int k = 0; k < 4; k++) subnet_tmp[k] = data[i + k];
		} else if(opt == DHCP_OPT_ROUTER && olen >= 4) {
			for(int k = 0; k < 4; k++) router_tmp[k] = data[i + k];
		} else if(opt == DHCP_OPT_DNS && olen >= 4) {
			for(int k = 0; k < 4; k++) dns_tmp[k] = data[i + k];
		} else if(opt == DHCP_OPT_LEASE && olen >= 4) {
			lease_tmp = readU32BE(&data[i]);
		}
		i += olen;
	}

	if(msg_type == DHCP_MSG_OFFER) {
		for(int k = 0; k < 4; k++) dhcp_offer_ip[k] = data[16 + k]; // yiaddr
		copyIp4(dhcp_offer_server, server_tmp);
		copyIp4(dhcp_offer_subnet, subnet_tmp);
		copyIp4(dhcp_offer_router, router_tmp);
		copyIp4(dhcp_offer_dns, dns_tmp);
		dhcp_offer_lease = lease_tmp;
		dhcp_offer_ready = 1;
		if(verbose) {
			printString("\nDHCP: OFFER ");
			printIPv4(dhcp_offer_ip);
			if(!(dhcp_offer_server[0] == 0 && dhcp_offer_server[1] == 0 && dhcp_offer_server[2] == 0 && dhcp_offer_server[3] == 0)) {
				printString(" from ");
				printIPv4(dhcp_offer_server);
			}
			printChar('\n');
		}
		return 1;
	}

	if(msg_type == DHCP_MSG_ACK) {
		for(int k = 0; k < 4; k++) dhcp_offer_ip[k] = data[16 + k];
		if(!(server_tmp[0] == 0 && server_tmp[1] == 0 && server_tmp[2] == 0 && server_tmp[3] == 0)) {
			copyIp4(dhcp_offer_server, server_tmp);
		}
		if(!(subnet_tmp[0] == 0 && subnet_tmp[1] == 0 && subnet_tmp[2] == 0 && subnet_tmp[3] == 0)) {
			copyIp4(dhcp_offer_subnet, subnet_tmp);
		}
		if(!(router_tmp[0] == 0 && router_tmp[1] == 0 && router_tmp[2] == 0 && router_tmp[3] == 0)) {
			copyIp4(dhcp_offer_router, router_tmp);
		}
		if(!(dns_tmp[0] == 0 && dns_tmp[1] == 0 && dns_tmp[2] == 0 && dns_tmp[3] == 0)) {
			copyIp4(dhcp_offer_dns, dns_tmp);
		}
		if(lease_tmp != 0) dhcp_offer_lease = lease_tmp;
		dhcp_ack_ready = 1;
		if(verbose) {
			printString("\nDHCP: ACK ");
			printIPv4(dhcp_offer_ip);
			printChar('\n');
		}
		return 1;
	}

	if(msg_type == DHCP_MSG_NAK) {
		dhcp_nak_seen = 1;
		if(verbose) printString("\nDHCP: NAK\n");
		return 1;
	}

	return 1;
}

int dhcpAcquireLease(int verbose) {
	unsigned char local_mac[6];
	unsigned int seed = pit_ticks;

	if(!ne2k_is_ready()) return 0;
	ne2k_get_mac(local_mac);

	seed ^= ((unsigned int)local_mac[2] << 24);
	seed ^= ((unsigned int)local_mac[3] << 16);
	seed ^= ((unsigned int)local_mac[4] << 8);
	seed ^= (unsigned int)local_mac[5];
	if(seed == 0) seed = 0x45524953;
	dhcp_xid = seed ^ 0xA5A55A5A;
	if(dhcp_xid == 0) dhcp_xid = 0x45524953;

	dhcpResetOfferState();
	if(verbose) printString("\nDHCP: sending DISCOVER...\n");
	if(dhcpBuildAndSend(DHCP_MSG_DISCOVER, 0, 0) <= 0) return 0;

	for(unsigned int spins = 0; spins < 700000; spins++) {
		unsigned char rx[1600];
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
		if(dhcp_offer_ready) break;
		if(dhcp_nak_seen) return 0;
	}
	if(!dhcp_offer_ready) return 0;

	if(verbose) {
		printString("DHCP: requesting ");
		printIPv4(dhcp_offer_ip);
		printString(" ...\n");
	}
	dhcp_ack_ready = 0;
	dhcp_nak_seen = 0;
	if(dhcpBuildAndSend(DHCP_MSG_REQUEST, dhcp_offer_ip, dhcp_offer_server) <= 0) return 0;

	for(unsigned int spins = 0; spins < 700000; spins++) {
		unsigned char rx[1600];
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
		if(dhcp_ack_ready) break;
		if(dhcp_nak_seen) return 0;
	}
	if(!dhcp_ack_ready) return 0;

	dhcpApplyOffer();
	return 1;
}

void dhcpCmd() {
	if(!ne2k_is_ready()) {
		printString("\nDHCP failed: NIC not initialized.\n");
		return;
	}
	if(dhcpAcquireLease(1)) {
		printString("DHCP: configured IP ");
		printIPv4(net_local_ip);
		printString(" / ");
		printIPv4(net_subnet_mask);
		printString(" gw ");
		printIPv4(net_gateway_ip);
		printString(" dns ");
		printIPv4(net_dns_ip);
		if(net_dhcp_lease_seconds) {
			printString(" lease=");
			printNumber(net_dhcp_lease_seconds);
			printString("s");
		}
		printChar('\n');
	} else {
		printString("DHCP: no lease, keeping current network configuration.\n");
	}
}

int netBuildAndSendIPv4(unsigned char proto, const unsigned char dst_ip[4], const unsigned char* payload, unsigned short payload_len) {
	unsigned char frame[1518];
	unsigned char local_mac[6];
	unsigned char dst_mac[6];
	unsigned char next_hop_ip[4];
	unsigned short ip_total_len;
	unsigned short frame_len;
	unsigned short hdr_ck;

	if(!ne2k_is_ready()) return 0;
	if(payload_len > (1500 - 20)) return 0; // MTU with no IPv4 options

	// Route selection:
	// - same subnet: ARP destination directly
	// - different subnet: ARP gateway
	if(ipIsSameSubnet(net_local_ip, dst_ip, net_subnet_mask)) {
		copyIp4(next_hop_ip, dst_ip);
	} else {
		if(ipIsZero(net_gateway_ip)) return 0;
		copyIp4(next_hop_ip, net_gateway_ip);
	}
	if(!arpResolve(next_hop_ip, dst_mac, 300000)) return 0;

	ne2k_get_mac(local_mac);

	// Ethernet header.
	for(int i = 0; i < 6; i++) frame[i] = dst_mac[i];
	for(int i = 0; i < 6; i++) frame[6 + i] = local_mac[i];
	frame[12] = 0x08;
	frame[13] = 0x00;

	// IPv4 header (20 bytes, no options).
	ip_total_len = (unsigned short)(20 + payload_len);
	frame[14] = IP_VERSION_IHL;
	frame[15] = 0x00;
	frame[16] = (unsigned char)((ip_total_len >> 8) & 0xFF);
	frame[17] = (unsigned char)(ip_total_len & 0xFF);
	frame[18] = (unsigned char)((ip_ident_counter >> 8) & 0xFF);
	frame[19] = (unsigned char)(ip_ident_counter & 0xFF);
	ip_ident_counter++;
	frame[20] = 0x40; // Don't fragment
	frame[21] = 0x00;
	frame[22] = IP_TTL_DEFAULT;
	frame[23] = proto;
	frame[24] = 0x00;
	frame[25] = 0x00;
	for(int i = 0; i < 4; i++) frame[26 + i] = net_local_ip[i];
	for(int i = 0; i < 4; i++) frame[30 + i] = dst_ip[i];

	hdr_ck = ipChecksum16(&frame[14], 20);
	frame[24] = (unsigned char)((hdr_ck >> 8) & 0xFF);
	frame[25] = (unsigned char)(hdr_ck & 0xFF);

	for(int i = 0; i < payload_len; i++) frame[34 + i] = payload[i];
	frame_len = (unsigned short)(14 + ip_total_len);
	return ne2k_send_frame(frame, frame_len);
}

unsigned short tcpChecksum(const unsigned char src_ip[4], const unsigned char dst_ip[4], const unsigned char* tcp, unsigned short tcp_len) {
	unsigned int sum = 0;
	unsigned int i = 0;

	// Pseudo-header
	sum += (unsigned int)(((unsigned short)src_ip[0] << 8) | src_ip[1]);
	sum += (unsigned int)(((unsigned short)src_ip[2] << 8) | src_ip[3]);
	sum += (unsigned int)(((unsigned short)dst_ip[0] << 8) | dst_ip[1]);
	sum += (unsigned int)(((unsigned short)dst_ip[2] << 8) | dst_ip[3]);
	sum += (unsigned int)IP_PROTO_TCP;
	sum += (unsigned int)tcp_len;

	while(i + 1 < tcp_len) {
		sum += (unsigned int)(((unsigned short)tcp[i] << 8) | tcp[i + 1]);
		i += 2;
	}
	if(i < tcp_len) {
		sum += (unsigned int)((unsigned short)tcp[i] << 8);
	}
	while(sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return (unsigned short)(~sum);
}

void tcpConnReset() {
	tcp_client_conn.valid = 0;
	tcp_client_conn.state = TCP_ST_CLOSED;
	tcp_client_conn.local_port = 0;
	tcp_client_conn.remote_port = 0;
	tcp_client_conn.snd_una = 0;
	tcp_client_conn.snd_nxt = 0;
	tcp_client_conn.rcv_nxt = 0;
	tcp_client_conn.snd_wnd = 0;
	tcp_client_conn.last_tick = 0;
	tcp_client_conn.rto_tick = 0;
	tcp_client_conn.retry_count = 0;
	tcp_client_conn.tx_last_len = 0;
	tcp_client_conn.tx_last_seq_advance = 0;
	tcp_client_conn.rx_len = 0;
	for(int i = 0; i < 4; i++) tcp_client_conn.remote_ip[i] = 0;
}

int tcpConnMatches(const unsigned char src_ip[4], unsigned short src_port, unsigned short dst_port) {
	if(!tcp_client_conn.valid) return 0;
	if(tcp_client_conn.local_port != dst_port) return 0;
	if(tcp_client_conn.remote_port != src_port) return 0;
	return ip4Equal(tcp_client_conn.remote_ip, src_ip);
}

int tcpSendSegmentRaw(unsigned char flags, const unsigned char* data, unsigned short len, unsigned int seq, unsigned int ack) {
	unsigned char seg[1500 - 20];
	unsigned short seg_len = (unsigned short)(20 + len);
	unsigned short ck;

	if(!tcp_client_conn.valid) return 0;
	if(seg_len > (1500 - 20)) return 0;

	seg[0] = (unsigned char)((tcp_client_conn.local_port >> 8) & 0xFF);
	seg[1] = (unsigned char)(tcp_client_conn.local_port & 0xFF);
	seg[2] = (unsigned char)((tcp_client_conn.remote_port >> 8) & 0xFF);
	seg[3] = (unsigned char)(tcp_client_conn.remote_port & 0xFF);
	seg[4] = (unsigned char)((seq >> 24) & 0xFF);
	seg[5] = (unsigned char)((seq >> 16) & 0xFF);
	seg[6] = (unsigned char)((seq >> 8) & 0xFF);
	seg[7] = (unsigned char)(seq & 0xFF);
	if(flags & TCP_FLAG_ACK) {
		seg[8]  = (unsigned char)((ack >> 24) & 0xFF);
		seg[9]  = (unsigned char)((ack >> 16) & 0xFF);
		seg[10] = (unsigned char)((ack >> 8) & 0xFF);
		seg[11] = (unsigned char)(ack & 0xFF);
	} else {
		seg[8] = seg[9] = seg[10] = seg[11] = 0;
	}
	seg[12] = 0x50; // data offset 5, no options
	seg[13] = flags;
	seg[14] = 0x04; // window 1024
	seg[15] = 0x00;
	seg[16] = 0x00;
	seg[17] = 0x00;
	seg[18] = 0x00; // urgent ptr
	seg[19] = 0x00;
	for(int i = 0; i < len; i++) seg[20 + i] = data[i];

	ck = tcpChecksum(net_local_ip, tcp_client_conn.remote_ip, seg, seg_len);
	seg[16] = (unsigned char)((ck >> 8) & 0xFF);
	seg[17] = (unsigned char)(ck & 0xFF);

	return netBuildAndSendIPv4(IP_PROTO_TCP, tcp_client_conn.remote_ip, seg, seg_len);
}

int tcpSendSegment(unsigned char flags, const unsigned char* data, unsigned short len, int track_retransmit) {
	unsigned int seq = tcp_client_conn.snd_nxt;
	unsigned int ack = tcp_client_conn.rcv_nxt;
	unsigned short adv = len;
	if(flags & TCP_FLAG_SYN) adv++;
	if(flags & TCP_FLAG_FIN) adv++;
	if(!tcpSendSegmentRaw(flags, data, len, seq, ack)) return 0;

	tcp_client_conn.snd_nxt += adv;
	tcp_client_conn.last_tick = pit_ticks;
	if(track_retransmit && adv > 0) {
		if(len > TCP_RX_MAX) len = TCP_RX_MAX;
		for(int i = 0; i < len; i++) tcp_client_conn.tx_last[i] = data[i];
		tcp_client_conn.tx_last_len = len;
		tcp_client_conn.tx_last_seq_advance = adv;
		tcp_client_conn.rto_tick = pit_ticks;
		tcp_client_conn.retry_count = 0;
	}
	return 1;
}

void tcpAckAdvance(unsigned int ack_num) {
	if(ack_num > tcp_client_conn.snd_una && ack_num <= tcp_client_conn.snd_nxt) {
		tcp_client_conn.snd_una = ack_num;
		tcp_client_conn.retry_count = 0;
		tcp_client_conn.rto_tick = pit_ticks;
		if(tcp_client_conn.snd_una == tcp_client_conn.snd_nxt) {
			tcp_client_conn.tx_last_len = 0;
			tcp_client_conn.tx_last_seq_advance = 0;
		}
	}
}

void tcpRetransmitIfNeeded() {
	unsigned int now = pit_ticks;
	unsigned int unacked = tcp_client_conn.snd_nxt - tcp_client_conn.snd_una;
	if(!tcp_client_conn.valid) return;
	if(unacked == 0) return;
	if((now - tcp_client_conn.rto_tick) < TCP_RTO_TICKS) return;

	if(tcp_client_conn.retry_count >= TCP_MAX_RETRY) {
		tcpConnReset();
		return;
	}

	// Re-send the last tracked segment from snd_una.
	{
		unsigned char flags = TCP_FLAG_ACK;
		unsigned short len = tcp_client_conn.tx_last_len;
		unsigned int seq = tcp_client_conn.snd_una;
		unsigned int ack = tcp_client_conn.rcv_nxt;

		if(tcp_client_conn.state == TCP_ST_SYN_SENT) {
			flags = TCP_FLAG_SYN;
			len = 0;
		} else if(tcp_client_conn.state == TCP_ST_FIN_WAIT1 || tcp_client_conn.state == TCP_ST_LAST_ACK) {
			flags = (unsigned char)(TCP_FLAG_ACK | TCP_FLAG_FIN);
			len = 0;
		} else if(len > 0) {
			flags = (unsigned char)(TCP_FLAG_ACK | TCP_FLAG_PSH);
		}

		(void)tcpSendSegmentRaw(flags, tcp_client_conn.tx_last, len, seq, ack);
	}
	tcp_client_conn.retry_count++;
	tcp_client_conn.rto_tick = now;
}

void tcpHandlePayload(const unsigned char* ip_payload, unsigned short ip_payload_len, const unsigned char* src_ip, int verbose) {
	unsigned short src_port;
	unsigned short dst_port;
	unsigned int seq;
	unsigned int ack;
	unsigned char data_off_words;
	unsigned short hdr_len;
	unsigned char flags;
	unsigned short data_len;
	const unsigned char* data;
	unsigned short wnd;

	if(ip_payload_len < 20) return;
	src_port = (unsigned short)(((unsigned short)ip_payload[0] << 8) | ip_payload[1]);
	dst_port = (unsigned short)(((unsigned short)ip_payload[2] << 8) | ip_payload[3]);
	seq = ((unsigned int)ip_payload[4] << 24) | ((unsigned int)ip_payload[5] << 16) | ((unsigned int)ip_payload[6] << 8) | (unsigned int)ip_payload[7];
	ack = ((unsigned int)ip_payload[8] << 24) | ((unsigned int)ip_payload[9] << 16) | ((unsigned int)ip_payload[10] << 8) | (unsigned int)ip_payload[11];
	data_off_words = (unsigned char)((ip_payload[12] >> 4) & 0x0F);
	hdr_len = (unsigned short)(data_off_words * 4);
	if(hdr_len < 20 || hdr_len > ip_payload_len) return;
	flags = ip_payload[13];
	wnd = (unsigned short)(((unsigned short)ip_payload[14] << 8) | ip_payload[15]);
	data_len = (unsigned short)(ip_payload_len - hdr_len);
	data = &ip_payload[hdr_len];

	if(!tcpConnMatches(src_ip, src_port, dst_port)) return;
	tcp_client_conn.snd_wnd = wnd;

	if(flags & TCP_FLAG_RST) {
		tcpConnReset();
		return;
	}

	if(flags & TCP_FLAG_ACK) {
		tcpAckAdvance(ack);
		if(tcp_client_conn.state == TCP_ST_LAST_ACK && tcp_client_conn.snd_una == tcp_client_conn.snd_nxt) {
			tcpConnReset();
			return;
		}
	}

	if(tcp_client_conn.state == TCP_ST_SYN_SENT) {
		if((flags & TCP_FLAG_SYN) && (flags & TCP_FLAG_ACK) && ack == tcp_client_conn.snd_nxt) {
			tcp_client_conn.rcv_nxt = seq + 1;
			tcp_client_conn.state = TCP_ST_ESTABLISHED;
			(void)tcpSendSegment(TCP_FLAG_ACK, 0, 0, 0);
			if(verbose) printString("TCP: established.\n");
		}
		return;
	}

	if(tcp_client_conn.state == TCP_ST_FIN_WAIT1) {
		if((flags & TCP_FLAG_ACK) && ack >= tcp_client_conn.snd_nxt) {
			tcp_client_conn.state = TCP_ST_FIN_WAIT2;
		}
	}

	if(data_len > 0) {
		if(seq == tcp_client_conn.rcv_nxt) {
			unsigned short can_copy = data_len;
			if(can_copy > (unsigned short)(TCP_RX_MAX - tcp_client_conn.rx_len)) {
				can_copy = (unsigned short)(TCP_RX_MAX - tcp_client_conn.rx_len);
			}
			for(int i = 0; i < can_copy; i++) {
				tcp_client_conn.rx_buf[tcp_client_conn.rx_len + i] = data[i];
			}
			tcp_client_conn.rx_len = (unsigned short)(tcp_client_conn.rx_len + can_copy);
			tcp_client_conn.rcv_nxt = seq + data_len;
		}
		(void)tcpSendSegment(TCP_FLAG_ACK, 0, 0, 0);
	}

	if(flags & TCP_FLAG_FIN) {
		unsigned int fin_seq_end = seq + data_len + 1;
		if(fin_seq_end > tcp_client_conn.rcv_nxt) tcp_client_conn.rcv_nxt = fin_seq_end;
		(void)tcpSendSegment(TCP_FLAG_ACK, 0, 0, 0);
		if(tcp_client_conn.state == TCP_ST_ESTABLISHED) {
			tcp_client_conn.state = TCP_ST_CLOSE_WAIT;
		} else if(tcp_client_conn.state == TCP_ST_FIN_WAIT2) {
			tcp_client_conn.state = TCP_ST_TIME_WAIT;
			tcp_client_conn.last_tick = pit_ticks;
		}
	}

	if(tcp_client_conn.state == TCP_ST_TIME_WAIT) {
		if((pit_ticks - tcp_client_conn.last_tick) > 200) { // ~2s
			tcpConnReset();
			return;
		}
	}
}

int tcpClientConnect(const unsigned char dst_ip[4], unsigned short dst_port, unsigned int timeout_spins) {
	unsigned char rx[1600];
	unsigned int seed = pit_ticks;

	tcpConnReset();
	tcp_client_conn.valid = 1;
	tcp_client_conn.state = TCP_ST_SYN_SENT;
	tcp_client_conn.local_port = (unsigned short)(40000 + (seed % 2000));
	tcp_client_conn.remote_port = dst_port;
	copyIp4(tcp_client_conn.remote_ip, dst_ip);
	tcp_client_conn.snd_una = 0x10000000u + (seed & 0x0000FFFFu);
	tcp_client_conn.snd_nxt = tcp_client_conn.snd_una;
	tcp_client_conn.rcv_nxt = 0;
	tcp_client_conn.snd_wnd = 1024;
	tcp_client_conn.rx_len = 0;

	if(!tcpSendSegment(TCP_FLAG_SYN, 0, 0, 1)) {
		tcpConnReset();
		return 0;
	}

	while(timeout_spins--) {
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
		tcpRetransmitIfNeeded();
		if(tcp_client_conn.valid && tcp_client_conn.state == TCP_ST_ESTABLISHED) return 1;
	}

	tcpConnReset();
	return 0;
}

int tcpClientSend(const unsigned char* data, unsigned short len) {
	if(!tcp_client_conn.valid || tcp_client_conn.state != TCP_ST_ESTABLISHED) return 0;
	if(tcp_client_conn.snd_wnd == 0) return 0;
	if(len > tcp_client_conn.snd_wnd) len = tcp_client_conn.snd_wnd;
	return tcpSendSegment((unsigned char)(TCP_FLAG_ACK | TCP_FLAG_PSH), data, len, 1);
}

void tcpClientClose() {
	unsigned char rx[1600];
	if(tcp_client_conn.valid && tcp_client_conn.state == TCP_ST_ESTABLISHED) {
		tcp_client_conn.state = TCP_ST_FIN_WAIT1;
		(void)tcpSendSegment((unsigned char)(TCP_FLAG_ACK | TCP_FLAG_FIN), 0, 0, 1);
		for(unsigned int spins = 0; spins < 400000 && tcp_client_conn.valid; spins++) {
			int got = ne2k_poll_receive(rx, 1600);
			if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
			tcpRetransmitIfNeeded();
			if(tcp_client_conn.state == TCP_ST_TIME_WAIT) break;
		}
	} else if(tcp_client_conn.valid && tcp_client_conn.state == TCP_ST_CLOSE_WAIT) {
		tcp_client_conn.state = TCP_ST_LAST_ACK;
		(void)tcpSendSegment((unsigned char)(TCP_FLAG_ACK | TCP_FLAG_FIN), 0, 0, 1);
		for(unsigned int spins = 0; spins < 200000 && tcp_client_conn.valid; spins++) {
			int got = ne2k_poll_receive(rx, 1600);
			if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
			tcpRetransmitIfNeeded();
			if(tcp_client_conn.snd_una == tcp_client_conn.snd_nxt) break;
		}
	}
	tcpConnReset();
}

int udpSendTo(unsigned short local_port, const unsigned char dst_ip[4], unsigned short dst_port, const unsigned char* data, unsigned short len) {
	unsigned char udp[1500 - 20];
	unsigned short udp_len;

	if(!udpSocketIsBound(local_port)) return 0;
	if(len > (unsigned short)(sizeof(udp) - 8)) return 0;

	udp_len = (unsigned short)(8 + len);
	udp[0] = (unsigned char)((local_port >> 8) & 0xFF);
	udp[1] = (unsigned char)(local_port & 0xFF);
	udp[2] = (unsigned char)((dst_port >> 8) & 0xFF);
	udp[3] = (unsigned char)(dst_port & 0xFF);
	udp[4] = (unsigned char)((udp_len >> 8) & 0xFF);
	udp[5] = (unsigned char)(udp_len & 0xFF);
	udp[6] = 0x00; // checksum optional for IPv4
	udp[7] = 0x00;
	for(int i = 0; i < len; i++) udp[8 + i] = data[i];

	return netBuildAndSendIPv4(IP_PROTO_UDP, dst_ip, udp, udp_len);
}

int dnsNameEncode(const char* host, unsigned char* out, unsigned short out_max, unsigned short* out_len) {
	unsigned short pos = 0;
	unsigned short label_len = 0;
	unsigned short label_start = 0;
	unsigned short i = 0;

	if(!host || host[0] == '\0') return 0;
	while(1) {
		char c = host[i];
		if(c == '.' || c == '\0') {
			if(label_len == 0 || label_len > 63) return 0;
			if((unsigned short)(label_start + label_len) >= out_max) return 0;
			out[label_start] = (unsigned char)label_len;
			if(c == '\0') break;
			label_start = pos;
			label_len = 0;
			if(pos >= out_max) return 0;
			out[pos++] = 0; // placeholder for next label len
			i++;
			continue;
		}
		// Lower-case normalization; DNS names are case-insensitive.
		if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
		if(pos >= out_max) return 0;
		if(label_len == 0 && pos == 0) {
			out[pos++] = 0; // first label length placeholder
			label_start = 0;
		}
		out[pos++] = (unsigned char)c;
		label_len++;
		i++;
	}
	if(pos >= out_max) return 0;
	out[pos++] = 0; // root terminator
	*out_len = pos;
	return 1;
}

int dnsBuildQuery(const char* host, unsigned char* out, unsigned short out_max, unsigned short txid, unsigned short* out_len) {
	unsigned short qname_len = 0;
	unsigned short p = 0;
	if(out_max < 18) return 0;
	for(int i = 0; i < out_max; i++) out[i] = 0;

	// DNS header
	out[p++] = (unsigned char)((txid >> 8) & 0xFF);
	out[p++] = (unsigned char)(txid & 0xFF);
	out[p++] = 0x01; out[p++] = 0x00; // RD=1
	out[p++] = 0x00; out[p++] = 0x01; // QDCOUNT=1
	out[p++] = 0x00; out[p++] = 0x00; // ANCOUNT=0
	out[p++] = 0x00; out[p++] = 0x00; // NSCOUNT=0
	out[p++] = 0x00; out[p++] = 0x00; // ARCOUNT=0

	if(!dnsNameEncode(host, &out[p], (unsigned short)(out_max - p), &qname_len)) return 0;
	p = (unsigned short)(p + qname_len);
	if((unsigned short)(p + 4) > out_max) return 0;
	out[p++] = 0x00; out[p++] = 0x01; // QTYPE A
	out[p++] = 0x00; out[p++] = 0x01; // QCLASS IN

	*out_len = p;
	return 1;
}

int dnsSkipName(const unsigned char* msg, unsigned short msg_len, unsigned short* io_off) {
	unsigned short off = *io_off;
	unsigned short guard = 0;
	while(off < msg_len && guard < 256) {
		unsigned char c = msg[off++];
		guard++;
		if(c == 0) {
			*io_off = off;
			return 1;
		}
		if((c & 0xC0) == 0xC0) {
			// Compression pointer: one extra byte and stop.
			if(off >= msg_len) return 0;
			off++;
			*io_off = off;
			return 1;
		}
		if(c > 63) return 0;
		if((unsigned short)(off + c) > msg_len) return 0;
		off = (unsigned short)(off + c);
	}
	return 0;
}

void dnsHandleUdpResponse(const unsigned char* data, unsigned short len, int verbose) {
	unsigned short off;
	unsigned short qdcount;
	unsigned short ancount;
	unsigned short i;
	unsigned short rcode;
	unsigned char qr;

	if(!dns_waiting) return;
	if(len < 12) return;

	if((((unsigned short)data[0] << 8) | data[1]) != dns_txid) return;
	qr = (unsigned char)((data[2] >> 7) & 1);
	if(!qr) return;
	rcode = (unsigned short)(data[3] & 0x0F);
	qdcount = (unsigned short)(((unsigned short)data[4] << 8) | data[5]);
	ancount = (unsigned short)(((unsigned short)data[6] << 8) | data[7]);
	if(rcode != 0) {
		dns_done = 1;
		dns_ok = 0;
		return;
	}

	off = 12;
	for(i = 0; i < qdcount; i++) {
		if(!dnsSkipName(data, len, &off)) return;
		if((unsigned short)(off + 4) > len) return;
		off = (unsigned short)(off + 4); // qtype + qclass
	}

	for(i = 0; i < ancount; i++) {
		unsigned short atype, aclass, rdlen;
		if(!dnsSkipName(data, len, &off)) return;
		if((unsigned short)(off + 10) > len) return;
		atype = (unsigned short)(((unsigned short)data[off] << 8) | data[off + 1]);
		aclass = (unsigned short)(((unsigned short)data[off + 2] << 8) | data[off + 3]);
		rdlen = (unsigned short)(((unsigned short)data[off + 8] << 8) | data[off + 9]);
		off = (unsigned short)(off + 10);
		if((unsigned short)(off + rdlen) > len) return;

		if(atype == 1 && aclass == 1 && rdlen == 4) {
			for(int k = 0; k < 4; k++) dns_answer_ip[k] = data[off + k];
			dns_ok = 1;
			dns_done = 1;
			if(verbose) {
				printString("DNS: answer ");
				printIPv4(dns_answer_ip);
				printChar('\n');
			}
			return;
		}
		off = (unsigned short)(off + rdlen);
	}

	// Valid response but no A answer.
	dns_done = 1;
	dns_ok = 0;
}

int pingSendEchoRequest(const unsigned char dst_ip[4], unsigned short id, unsigned short seq) {
	unsigned char icmp[64];
	unsigned short icmp_len = 24;
	unsigned short ck;

	for(int i = 0; i < (int)icmp_len; i++) icmp[i] = 0;
	icmp[0] = 8; // Echo request
	icmp[1] = 0;
	icmp[2] = 0;
	icmp[3] = 0;
	icmp[4] = (unsigned char)((id >> 8) & 0xFF);
	icmp[5] = (unsigned char)(id & 0xFF);
	icmp[6] = (unsigned char)((seq >> 8) & 0xFF);
	icmp[7] = (unsigned char)(seq & 0xFF);
	icmp[8]  = (unsigned char)((pit_ticks >> 24) & 0xFF);
	icmp[9]  = (unsigned char)((pit_ticks >> 16) & 0xFF);
	icmp[10] = (unsigned char)((pit_ticks >> 8) & 0xFF);
	icmp[11] = (unsigned char)(pit_ticks & 0xFF);
	for(int i = 12; i < (int)icmp_len; i++) icmp[i] = (unsigned char)i;

	ck = ipChecksum16(icmp, icmp_len);
	icmp[2] = (unsigned char)((ck >> 8) & 0xFF);
	icmp[3] = (unsigned char)(ck & 0xFF);
	return netBuildAndSendIPv4(IP_PROTO_ICMP, dst_ip, icmp, icmp_len);
}

void handleIcmpPayload(const unsigned char* ip_payload, unsigned short ip_payload_len, const unsigned char* src_ip, const unsigned char* dst_ip, int verbose) {
	unsigned char type;
	unsigned char code;
	unsigned short id;
	unsigned short seq;

	if(ip_payload_len < 8) return;
	if(ipChecksum16(ip_payload, ip_payload_len) != 0) return;

	type = ip_payload[0];
	code = ip_payload[1];

	// Echo request to us: send echo reply.
	if(type == 8 && code == 0 && ip4Equal(dst_ip, net_local_ip)) {
		unsigned char reply[1500 - 20];
		unsigned short ck;
		if(ip_payload_len > sizeof(reply)) return;
		for(int i = 0; i < ip_payload_len; i++) reply[i] = ip_payload[i];
		reply[0] = 0; // Echo reply
		reply[2] = 0;
		reply[3] = 0;
		ck = ipChecksum16(reply, ip_payload_len);
		reply[2] = (unsigned char)((ck >> 8) & 0xFF);
		reply[3] = (unsigned char)(ck & 0xFF);
		(void)netBuildAndSendIPv4(IP_PROTO_ICMP, src_ip, reply, ip_payload_len);
		if(verbose) {
			printString("ICMP: echo request from ");
			printIPv4(src_ip);
			printString(" (replied)\n");
		}
		return;
	}

	// Echo reply for a pending ping command.
	if(type == 0 && code == 0 && ip_payload_len >= 8) {
		id = (unsigned short)(((unsigned short)ip_payload[4] << 8) | ip_payload[5]);
		seq = (unsigned short)(((unsigned short)ip_payload[6] << 8) | ip_payload[7]);
		if(ping_waiting && id == ping_identifier && seq == ping_sequence && ip4Equal(src_ip, ping_target_ip)) {
			ping_waiting = 0;
			ping_received = 1;
			ping_rtt_ticks = pit_ticks - ping_sent_tick;
			copyIp4(ping_reply_ip, src_ip);
		}
		if(verbose) {
			printString("ICMP: echo reply from ");
			printIPv4(src_ip);
			printChar('\n');
		}
	}
}

void handleUdpPayload(const unsigned char* ip_payload, unsigned short ip_payload_len, const unsigned char* src_ip, int verbose) {
	unsigned short src_port;
	unsigned short dst_port;
	unsigned short udp_len;
	unsigned short data_len;
	const unsigned char* udp_data;

	if(ip_payload_len < 8) return;
	src_port = (unsigned short)(((unsigned short)ip_payload[0] << 8) | ip_payload[1]);
	dst_port = (unsigned short)(((unsigned short)ip_payload[2] << 8) | ip_payload[3]);
	udp_len = (unsigned short)(((unsigned short)ip_payload[4] << 8) | ip_payload[5]);
	if(udp_len < 8) return;
	if(udp_len > ip_payload_len) return;

	data_len = (unsigned short)(udp_len - 8);
	udp_data = &ip_payload[8];

	// DHCP server->client traffic: UDP 67 -> 68
	if(src_port == DHCP_SERVER_PORT && dst_port == DHCP_CLIENT_PORT) {
		(void)dhcpHandleUdpPayload(udp_data, data_len, verbose);
		return;
	}

	// DNS response path for synchronous DNS command.
	if(dns_waiting && src_port == DNS_PORT && dst_port == DNS_SRC_PORT) {
		dnsHandleUdpResponse(udp_data, data_len, verbose);
		return;
	}

	if(udpSocketIsBound(dst_port)) {
		udpQueuePush(dst_port, src_port, src_ip, udp_data, data_len);
		if(verbose) {
			printString("UDP: queued dst=");
			printNumber((unsigned int)dst_port);
			printString(" src=");
			printNumber((unsigned int)src_port);
			printString(" len=");
			printNumber((unsigned int)data_len);
			printChar('\n');
		}
	} else if(verbose) {
		printString("UDP: dropped (no socket on port ");
		printNumber((unsigned int)dst_port);
		printString(")\n");
	}
}

void handleIPv4Frame(const unsigned char* frame, unsigned short len, int verbose) {
	unsigned char version = (unsigned char)(frame[14] >> 4);
	unsigned char ihl_words = (unsigned char)(frame[14] & 0x0F);
	unsigned short ihl_bytes;
	unsigned short total_len;
	unsigned char proto;
	unsigned char ttl;
	const unsigned char* src_ip;
	const unsigned char* dst_ip;
	unsigned short rx_ck;
	unsigned short calc_ck;
	unsigned char hdr[60];

	if(len < (14 + 20)) return;
	if(version != 4 || ihl_words < 5) return;

	ihl_bytes = (unsigned short)(ihl_words * 4);
	if(len < (unsigned short)(14 + ihl_bytes)) return;
	if(ihl_bytes > sizeof(hdr)) return;

	total_len = (unsigned short)(((unsigned short)frame[16] << 8) | frame[17]);
	if(total_len < ihl_bytes) return;
	if(len < (unsigned short)(14 + total_len)) return;

	ttl = frame[22];
	proto = frame[23];
	src_ip = &frame[26];
	dst_ip = &frame[30];

	// Learn sender mapping from observed IPv4 traffic.
	arpCacheUpsert(src_ip, &frame[6]);

	rx_ck = (unsigned short)(((unsigned short)frame[24] << 8) | frame[25]);
	for(int i = 0; i < ihl_bytes; i++) hdr[i] = frame[14 + i];
	hdr[10] = 0;
	hdr[11] = 0;
	calc_ck = ipChecksum16(hdr, ihl_bytes);

	if(verbose) {
		printString("\nIPv4: ");
		printIPv4(src_ip);
		printString(" -> ");
		printIPv4(dst_ip);
		printString(" proto=");
		printNumber((unsigned int)proto);
		printString(" ttl=");
		printNumber((unsigned int)ttl);
		printString(" len=");
		printNumber((unsigned int)total_len);
		if(calc_ck == rx_ck) printString(" cksum=ok");
		else printString(" cksum=bad");
		printChar('\n');
	}

	if(calc_ck != rx_ck) return;

	if(proto == IP_PROTO_UDP) {
		unsigned short ip_payload_len = (unsigned short)(total_len - ihl_bytes);
		const unsigned char* ip_payload = &frame[14 + ihl_bytes];
		handleUdpPayload(ip_payload, ip_payload_len, src_ip, verbose);
	} else if(proto == IP_PROTO_ICMP) {
		unsigned short ip_payload_len = (unsigned short)(total_len - ihl_bytes);
		const unsigned char* ip_payload = &frame[14 + ihl_bytes];
		handleIcmpPayload(ip_payload, ip_payload_len, src_ip, dst_ip, verbose);
	} else if(proto == IP_PROTO_TCP) {
		unsigned short ip_payload_len = (unsigned short)(total_len - ihl_bytes);
		const unsigned char* ip_payload = &frame[14 + ihl_bytes];
		tcpHandlePayload(ip_payload, ip_payload_len, src_ip, verbose);
	}
}

void showNetInfo() {
	if(!ne2k_is_ready()) {
		printString("\nNET: RTL8029/NE2000 not initialized.\n");
		printString("Reboot with QEMU option: -device ne2k_pci,netdev=net0\n");
		return;
	}

	unsigned char mac[6];
	ne2k_get_mac(mac);
	printString("\nNET: RTL8029 (NE2000-compatible)\n");
	printString("I/O Base: ");
	printHex(ne2k_get_io_base());
	printString("\nIRQ: ");
	printNumber(ne2k_get_irq_line());
	printString("\nMAC: ");
	printMac(mac);
	printString("\nIP: ");
	printIPv4(net_local_ip);
	printString("\nMask: ");
	printIPv4(net_subnet_mask);
	printString("\nGateway: ");
	printIPv4(net_gateway_ip);
	printString("\nDNS: ");
	printIPv4(net_dns_ip);
	printString("\nDHCP: ");
	if(net_dhcp_active) {
		printString("enabled");
		if(net_dhcp_lease_seconds) {
			printString(" (lease ");
			printNumber(net_dhcp_lease_seconds);
			printString("s)");
		}
	} else {
		printString("fallback/static");
	}
	printString("\n");
}

void sendNetTestFrame() {
	if(!ne2k_is_ready()) {
		printString("\nNETTX failed: NIC not initialized.\n");
		return;
	}

	unsigned char frame[60];
	unsigned char mac[6];
	ne2k_get_mac(mac);

	// Ethernet II broadcast test frame, custom EtherType 0x88B5.
	for(int i = 0; i < 6; i++) frame[i] = 0xFF;       // Destination: broadcast
	for(int i = 0; i < 6; i++) frame[6 + i] = mac[i]; // Source: local MAC
	frame[12] = 0x88;
	frame[13] = 0xB5;

	char payload[] = "ErisOS NETTX test";
	int payload_len = 0;
	while(payload[payload_len] != '\0') payload_len++;
	for(int i = 0; i < payload_len && (14 + i) < 60; i++) {
		frame[14 + i] = (unsigned char)payload[i];
	}
	for(int i = 14 + payload_len; i < 60; i++) {
		frame[i] = 0;
	}

	int sent = ne2k_send_frame(frame, 60);
	if(sent > 0) {
		printString("\nNETTX: sent ");
		printNumber((unsigned int)sent);
		printString(" bytes.\n");
	} else {
		printString("\nNETTX failed.\n");
	}
}

void pollNetRx() {
	if(!ne2k_is_ready()) {
		printString("\nNETRX failed: NIC not initialized.\n");
		return;
	}

	unsigned char rx[1600];
	int got = ne2k_poll_receive(rx, 1600);
	if(got <= 0) {
		printString("\nNETRX: no packet.\n");
		return;
	}

	printString("\nNETRX: received ");
	printNumber((unsigned int)got);
	printString(" bytes. First 32 bytes:\n");
	int shown = got < 32 ? got : 32;
	for(int i = 0; i < shown; i++) {
		printHexByte(rx[i]);
		printChar(' ');
	}
	printChar('\n');

	processRxFrameProtocols(rx, (unsigned short)got, 1);
}

void arpWho(char* argument) {
	unsigned char target_ip[4];
	unsigned char mac[6];

	if(!ne2k_is_ready()) {
		printString("\nARPWHO failed: NIC not initialized.\n");
		return;
	}
	if(argument[0] == '\0') {
		printString("\nUsage: ARPWHO <IPv4>\n");
		printString("Example: ARPWHO 10.0.2.2\n");
		return;
	}
	if(!parseIPv4(argument, target_ip)) {
		printString("\nARPWHO: invalid IPv4 address.\n");
		return;
	}

	printString("\nARP resolving ");
	printIPv4(target_ip);
	printString(" ...\n");
	if(arpResolve(target_ip, mac, 300000)) {
		printString("ARP: ");
		printIPv4(target_ip);
		printString(" -> ");
		printMac(mac);
		printString("\n");
	} else {
		printString("ARP resolve timeout.\n");
	}
}

void ipTx(char* argument) {
	unsigned char target_ip[4];
	char payload[] = "ErisOS IPv4 test";
	unsigned short payload_len = 0;
	int sent;

	if(!ne2k_is_ready()) {
		printString("\nIPTX failed: NIC not initialized.\n");
		return;
	}
	if(argument[0] == '\0') {
		printString("\nUsage: IPTX <IPv4>\n");
		printString("Example: IPTX 10.0.2.2\n");
		return;
	}
	if(!parseIPv4(argument, target_ip)) {
		printString("\nIPTX: invalid IPv4 address.\n");
		return;
	}

	while(payload[payload_len] != '\0') payload_len++;

	printString("\nIPTX sending IPv4 packet to ");
	printIPv4(target_ip);
	printString(" ...\n");
	sent = netBuildAndSendIPv4(IP_PROTO_TEST, target_ip, (const unsigned char*)payload, payload_len);
	if(sent > 0) {
		printString("IPTX: sent ");
		printNumber((unsigned int)sent);
		printString(" bytes.\n");
	} else {
		printString("IPTX failed (ARP unresolved or TX error).\n");
	}
}

void pingCmd(char* argument) {
	unsigned char target_ip[4];
	unsigned char rx[1600];

	if(!ne2k_is_ready()) {
		printString("\nPING failed: NIC not initialized.\n");
		return;
	}
	if(argument[0] == '\0') {
		printString("\nUsage: PING <IPv4>\n");
		printString("Example: PING 10.0.2.2\n");
		return;
	}
	if(!parseIPv4(argument, target_ip)) {
		printString("\nPING: invalid IPv4 address.\n");
		return;
	}

	ping_sequence++;
	copyIp4(ping_target_ip, target_ip);
	ping_waiting = 1;
	ping_received = 0;
	ping_sent_tick = pit_ticks;
	ping_rtt_ticks = 0;

	printString("\nPING ");
	printIPv4(target_ip);
	printString(" ...\n");
	if(pingSendEchoRequest(target_ip, ping_identifier, ping_sequence) <= 0) {
		ping_waiting = 0;
		printString("PING send failed (ARP unresolved or TX error).\n");
		return;
	}

	for(unsigned int spins = 0; spins < 600000; spins++) {
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
		if(ping_received) break;
	}

	if(ping_received) {
		printString("Reply from ");
		printIPv4(ping_reply_ip);
		printString(": seq=");
		printNumber((unsigned int)ping_sequence);
		printString(" time=");
		printNumber((unsigned int)(ping_rtt_ticks * 10));
		printString("ms\n");
	} else {
		ping_waiting = 0;
		printString("Request timed out.\n");
	}
}

void dnsCmd(char* argument) {
	char tok_host[DNS_NAME_MAX];
	char tok_dns[32];
	char* p = argument;
	unsigned char out_ip[4];

	if(!nextToken(&p, tok_host, sizeof(tok_host))) {
		printString("\nUsage: DNS <hostname> [dns_server_ip]\n");
		printString("Example: DNS example.com\n");
		printString("Example: DNS example.com 8.8.8.8\n");
		return;
	}
	if(nextToken(&p, tok_dns, sizeof(tok_dns))) {
		unsigned char server_ip[4];
		if(!parseIPv4(tok_dns, server_ip)) {
			printString("\nDNS: invalid DNS server IP.\n");
			return;
		}
		if(dnsResolveA(tok_host, server_ip, out_ip, 1)) {
			printString("DNS: ");
			printString(tok_host);
			printString(" -> ");
			printIPv4(out_ip);
			printChar('\n');
		} else {
			printString("DNS: no A record/timeout.\n");
		}
		return;
	}
	if(dnsResolveA(tok_host, net_dns_ip, out_ip, 1)) {
		printString("DNS: ");
		printString(tok_host);
		printString(" -> ");
		printIPv4(out_ip);
		printChar('\n');
	} else {
		printString("DNS: no A record/timeout.\n");
	}
}

int dnsResolveA(const char* host, const unsigned char dns_server_ip[4], unsigned char out_ip[4], int verbose) {
	char tok_host[DNS_NAME_MAX];
	unsigned char server_ip[4];
	unsigned char payload[512];
	unsigned short payload_len = 0;
	unsigned char rx[1600];

	if(!ne2k_is_ready()) {
		if(verbose) printString("\nDNS failed: NIC not initialized.\n");
		return 0;
	}
	if(!host || host[0] == '\0') return 0;
	for(int i = 0; i < DNS_NAME_MAX; i++) {
		tok_host[i] = host[i];
		if(host[i] == '\0') break;
	}
	if(ipIsZero(dns_server_ip)) return 0;
	copyIp4(server_ip, dns_server_ip);

	if(!udpSocketBind(DNS_SRC_PORT)) {
		if(verbose) printString("\nDNS: cannot bind local UDP source port.\n");
		return 0;
	}

	dns_txid = (unsigned short)((pit_ticks & 0xFFFF) ^ 0xA55A);
	if(dns_txid == 0) dns_txid = 1;
	if(!dnsBuildQuery(tok_host, payload, sizeof(payload), dns_txid, &payload_len)) {
		if(verbose) printString("\nDNS: invalid hostname format.\n");
		return 0;
	}

	dns_waiting = 1;
	dns_done = 0;
	dns_ok = 0;
	copyIp4(dns_server_ip_last, server_ip);
	for(int i = 0; i < DNS_NAME_MAX; i++) {
		dns_query_name[i] = tok_host[i];
		if(tok_host[i] == '\0') break;
	}

	if(verbose) {
		printString("\nDNS query ");
		printString(tok_host);
		printString(" @ ");
		printIPv4(server_ip);
		printString(" ...\n");
	}

	if(!udpSendTo(DNS_SRC_PORT, server_ip, DNS_PORT, payload, payload_len)) {
		dns_waiting = 0;
		if(verbose) printString("DNS send failed.\n");
		return 0;
	}

	for(unsigned int spins = 0; spins < 900000; spins++) {
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
		if(dns_done) break;
	}
	dns_waiting = 0;

	if(dns_done && dns_ok) {
		copyIp4(out_ip, dns_answer_ip);
		return 1;
	} else {
		return 0;
	}
}

void httpGetCmd(char* argument) {
	char tok_host[DNS_NAME_MAX];
	char tok2[128];
	char tok3[16];
	char* p = argument;
	unsigned short port = 80;
	char path[128];
	unsigned char dst_ip[4];
	unsigned char req[512];
	unsigned short req_len = 0;
	unsigned short i;
	unsigned short initial_rx;
	unsigned int stable = 0;

	if(!ne2k_is_ready()) {
		printString("\nHTTPGET failed: NIC not initialized.\n");
		return;
	}
	if(!nextToken(&p, tok_host, sizeof(tok_host))) {
		printString("\nUsage: HTTPGET <host_or_ip> [path] [port]\n");
		printString("Example: HTTPGET example.com /\n");
		printString("Example: HTTPGET 93.184.216.34 / 80\n");
		return;
	}

	path[0] = '/';
	path[1] = '\0';
	if(nextToken(&p, tok2, sizeof(tok2))) {
		unsigned short maybe_port;
		if(parsePort(tok2, &maybe_port)) {
			port = maybe_port;
		} else {
			for(i = 0; i < (unsigned short)(sizeof(path) - 1) && tok2[i] != '\0'; i++) path[i] = tok2[i];
			path[i] = '\0';
			if(nextToken(&p, tok3, sizeof(tok3))) {
				unsigned short parsed;
				if(parsePort(tok3, &parsed)) port = parsed;
			}
		}
	}

	if(!parseIPv4(tok_host, dst_ip)) {
		if(ipIsZero(net_dns_ip)) {
			printString("\nHTTPGET: hostname requires DNS, but no DNS server is configured.\n");
			return;
		}
		if(!dnsResolveA(tok_host, net_dns_ip, dst_ip, 1)) {
			printString("HTTPGET: DNS resolution failed.\n");
			return;
		}
	}

	printString("\nHTTPGET connecting to ");
	printIPv4(dst_ip);
	printString(":");
	printNumber((unsigned int)port);
	printString(" ...\n");
	if(!tcpClientConnect(dst_ip, port, 900000)) {
		printString("HTTPGET: connect timeout/failure.\n");
		return;
	}

	{
		char pre1[] = "GET ";
		char pre2[] = " HTTP/1.0\r\nHost: ";
		char pre3[] = "\r\nUser-Agent: ErisOS/0.1\r\nConnection: close\r\n\r\n";
		for(i = 0; pre1[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)pre1[i];
		for(i = 0; path[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)path[i];
		for(i = 0; pre2[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)pre2[i];
		for(i = 0; tok_host[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)tok_host[i];
		for(i = 0; pre3[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)pre3[i];
	}
	if(!tcpClientSend(req, req_len)) {
		printString("HTTPGET: send failed.\n");
		tcpClientClose();
		return;
	}

	initial_rx = tcp_client_conn.rx_len;
	for(unsigned int spins = 0; spins < 1800000; spins++) {
		unsigned char rx[1600];
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);
		tcpRetransmitIfNeeded();

		if(tcp_client_conn.rx_len != initial_rx) {
			initial_rx = tcp_client_conn.rx_len;
			stable = 0;
		} else {
			stable++;
		}
		if((tcp_client_conn.state == TCP_ST_CLOSE_WAIT || tcp_client_conn.state == TCP_ST_TIME_WAIT) && stable > 50000) break;
	}

	printString("HTTPGET: received ");
	printNumber((unsigned int)tcp_client_conn.rx_len);
	printString(" bytes.\n");
	for(i = 0; i < tcp_client_conn.rx_len; i++) {
		char c = (char)tcp_client_conn.rx_buf[i];
		if(c >= 9 && c <= 126) printChar(c);
		else if(c == '\n' || c == '\r' || c == '\t') printChar(c);
		else printChar('.');
	}
	printChar('\n');
	tcpClientClose();
}

void udpBindCmd(char* argument) {
	char tok[16];
	char* p = argument;
	unsigned short port;

	if(!ne2k_is_ready()) {
		printString("\nUDPBIND failed: NIC not initialized.\n");
		return;
	}
	if(!nextToken(&p, tok, sizeof(tok)) || !parsePort(tok, &port)) {
		printString("\nUsage: UDPBIND <port>\n");
		printString("Example: UDPBIND 5000\n");
		return;
	}
	if(udpSocketBind(port)) {
		printString("\nUDPBIND: bound port ");
		printNumber((unsigned int)port);
		printString(".\n");
	} else {
		printString("\nUDPBIND failed: socket table full.\n");
	}
}

void udpSendCmd(char* argument) {
	char tok_src[16];
	char tok_ip[32];
	char tok_dst[16];
	char* p = argument;
	unsigned short src_port;
	unsigned short dst_port;
	unsigned char dst_ip[4];
	unsigned char payload[UDP_DATA_MAX];
	unsigned short payload_len = 0;
	int sent;
	char* msg;

	if(!ne2k_is_ready()) {
		printString("\nUDPSEND failed: NIC not initialized.\n");
		return;
	}
	if(!nextToken(&p, tok_src, sizeof(tok_src)) ||
	   !nextToken(&p, tok_ip, sizeof(tok_ip)) ||
	   !nextToken(&p, tok_dst, sizeof(tok_dst))) {
		printString("\nUsage: UDPSEND <src_port> <dst_ip> <dst_port> <message>\n");
		printString("Example: UDPSEND 5000 10.0.2.2 5001 hello\n");
		return;
	}
	if(!parsePort(tok_src, &src_port) || !parsePort(tok_dst, &dst_port) || !parseIPv4(tok_ip, dst_ip)) {
		printString("\nUDPSEND: invalid src_port, dst_ip, or dst_port.\n");
		return;
	}

	msg = skipSpaces(p);
	while(msg[payload_len] != '\0' && payload_len < UDP_DATA_MAX) {
		payload[payload_len] = (unsigned char)msg[payload_len];
		payload_len++;
	}

	sent = udpSendTo(src_port, dst_ip, dst_port, payload, payload_len);
	if(sent > 0) {
		printString("\nUDPSEND: sent ");
		printNumber((unsigned int)sent);
		printString(" bytes to ");
		printIPv4(dst_ip);
		printString(":");
		printNumber((unsigned int)dst_port);
		printString(".\n");
	} else {
		printString("\nUDPSEND failed (bind missing, ARP unresolved, or TX error).\n");
	}
}

void udpRecvCmd(char* argument) {
	char tok[16];
	char* p = argument;
	unsigned short port;
	unsigned char src_ip[4];
	unsigned short src_port = 0;
	unsigned char data[UDP_DATA_MAX];
	int got;

	if(!ne2k_is_ready()) {
		printString("\nUDPRECV failed: NIC not initialized.\n");
		return;
	}
	if(!nextToken(&p, tok, sizeof(tok)) || !parsePort(tok, &port)) {
		printString("\nUsage: UDPRECV <port>\n");
		printString("Example: UDPRECV 5000\n");
		return;
	}
	if(!udpSocketIsBound(port)) {
		printString("\nUDPRECV: port not bound. Use UDPBIND first.\n");
		return;
	}

	// Poll briefly so incoming frames can be parsed and queued.
	for(unsigned int spins = 0; spins < 20000; spins++) {
		unsigned char rx[1600];
		int r = ne2k_poll_receive(rx, 1600);
		if(r > 0) processRxFrameProtocols(rx, (unsigned short)r, 0);
	}

	got = udpQueuePop(port, src_ip, &src_port, data, UDP_DATA_MAX);
	if(got <= 0) {
		printString("\nUDPRECV: no datagram.\n");
		return;
	}

	printString("\nUDPRECV: ");
	printNumber((unsigned int)got);
	printString(" bytes from ");
	printIPv4(src_ip);
	printString(":");
	printNumber((unsigned int)src_port);
	printString("\nData: ");
	for(int i = 0; i < got; i++) {
		char c = (char)data[i];
		if(c >= 32 && c <= 126) printChar(c);
		else printChar('.');
	}
	printChar('\n');
}

void ipv4ToString(const unsigned char ip[4], char* out, int out_max) {
	unsigned int idx = 0;
	for(int part = 0; part < 4; part++) {
		unsigned int v = ip[part];
		char tmp[4];
		int t = 0;
		if(v == 0) {
			tmp[t++] = '0';
		} else {
			char rev[4];
			int r = 0;
			while(v > 0 && r < 3) {
				rev[r++] = (char)('0' + (v % 10));
				v /= 10;
			}
			while(r > 0) tmp[t++] = rev[--r];
		}
		for(int i = 0; i < t && idx + 1 < (unsigned int)out_max; i++) out[idx++] = tmp[i];
		if(part != 3 && idx + 1 < (unsigned int)out_max) out[idx++] = '.';
	}
	out[idx < (unsigned int)out_max ? idx : (unsigned int)(out_max - 1)] = '\0';
}

void tcpGetCmd(char* argument) {
	char tok_ip[32];
	char tok_port[16];
	char tok_path[96];
	char* p = argument;
	unsigned char dst_ip[4];
	unsigned short dst_port;
	char path[96];
	char ip_text[24];
	unsigned char req[320];
	unsigned short req_len = 0;
	unsigned short initial_rx;
	unsigned int stable = 0;
	unsigned short i;

	if(!ne2k_is_ready()) {
		printString("\nTCPGET failed: NIC not initialized.\n");
		return;
	}
	if(!nextToken(&p, tok_ip, sizeof(tok_ip)) || !nextToken(&p, tok_port, sizeof(tok_port))) {
		printString("\nUsage: TCPGET <dst_ip> <dst_port> [path]\n");
		printString("Example: TCPGET 93.184.216.34 80 /\n");
		return;
	}
	if(!parseIPv4(tok_ip, dst_ip) || !parsePort(tok_port, &dst_port)) {
		printString("\nTCPGET: invalid IP or port.\n");
		return;
	}
	if(nextToken(&p, tok_path, sizeof(tok_path))) {
		for(i = 0; i < sizeof(path) - 1 && tok_path[i] != '\0'; i++) path[i] = tok_path[i];
		path[i] = '\0';
	} else {
		path[0] = '/';
		path[1] = '\0';
	}

	printString("\nTCPGET: connecting to ");
	printIPv4(dst_ip);
	printString(":");
	printNumber((unsigned int)dst_port);
	printString(" ...\n");
	if(!tcpClientConnect(dst_ip, dst_port, 900000)) {
		printString("TCPGET: connect timeout/failure.\n");
		return;
	}

	ipv4ToString(dst_ip, ip_text, sizeof(ip_text));
	{
		char pre1[] = "GET ";
		char pre2[] = " HTTP/1.0\r\nHost: ";
		char pre3[] = "\r\nConnection: close\r\n\r\n";
		for(i = 0; pre1[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)pre1[i];
		for(i = 0; path[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)path[i];
		for(i = 0; pre2[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)pre2[i];
		for(i = 0; ip_text[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)ip_text[i];
		for(i = 0; pre3[i] != '\0' && req_len < sizeof(req); i++) req[req_len++] = (unsigned char)pre3[i];
	}
	if(!tcpClientSend(req, req_len)) {
		printString("TCPGET: send failed.\n");
		tcpClientClose();
		return;
	}

	initial_rx = tcp_client_conn.rx_len;
	for(unsigned int spins = 0; spins < 1600000; spins++) {
		unsigned char rx[1600];
		int got = ne2k_poll_receive(rx, 1600);
		if(got > 0) processRxFrameProtocols(rx, (unsigned short)got, 0);

		if(tcp_client_conn.rx_len != initial_rx) {
			initial_rx = tcp_client_conn.rx_len;
			stable = 0;
		} else {
			stable++;
		}
		if((tcp_client_conn.state == TCP_ST_CLOSE_WAIT || tcp_client_conn.state == TCP_ST_TIME_WAIT) && stable > 50000) break;
	}

	printString("TCPGET: received ");
	printNumber((unsigned int)tcp_client_conn.rx_len);
	printString(" bytes.\n");
	for(i = 0; i < tcp_client_conn.rx_len; i++) {
		char c = (char)tcp_client_conn.rx_buf[i];
		if(c >= 9 && c <= 126) printChar(c);
		else if(c == '\n' || c == '\r') printChar(c);
		else printChar('.');
	}
	printChar('\n');
	tcpClientClose();
}

void tcpStatePrint(unsigned char st) {
	if(st == TCP_ST_CLOSED) printString("CLOSED");
	else if(st == TCP_ST_SYN_SENT) printString("SYN_SENT");
	else if(st == TCP_ST_ESTABLISHED) printString("ESTABLISHED");
	else if(st == TCP_ST_FIN_WAIT1) printString("FIN_WAIT1");
	else if(st == TCP_ST_FIN_WAIT2) printString("FIN_WAIT2");
	else if(st == TCP_ST_CLOSE_WAIT) printString("CLOSE_WAIT");
	else if(st == TCP_ST_LAST_ACK) printString("LAST_ACK");
	else if(st == TCP_ST_TIME_WAIT) printString("TIME_WAIT");
	else printString("UNKNOWN");
}

void tcpStatCmd() {
	printString("\nTCP state: ");
	tcpStatePrint(tcp_client_conn.state);
	if(!tcp_client_conn.valid) {
		printString("\n");
		return;
	}
	printString("\nRemote: ");
	printIPv4(tcp_client_conn.remote_ip);
	printString(":");
	printNumber((unsigned int)tcp_client_conn.remote_port);
	printString("\nLocal port: ");
	printNumber((unsigned int)tcp_client_conn.local_port);
	printString("\nSND.UNA=");
	printHex(tcp_client_conn.snd_una);
	printString(" SND.NXT=");
	printHex(tcp_client_conn.snd_nxt);
	printString("\nRCV.NXT=");
	printHex(tcp_client_conn.rcv_nxt);
	printString(" WIN=");
	printNumber((unsigned int)tcp_client_conn.snd_wnd);
	printString("\nRX buffered: ");
	printNumber((unsigned int)tcp_client_conn.rx_len);
	printString(" bytes\n");
}

void tcpConnCmd(char* argument) {
	char tok_ip[32];
	char tok_port[16];
	char* p = argument;
	unsigned char dst_ip[4];
	unsigned short dst_port;
	if(!ne2k_is_ready()) {
		printString("\nTCPCONN failed: NIC not initialized.\n");
		return;
	}
	if(!nextToken(&p, tok_ip, sizeof(tok_ip)) || !nextToken(&p, tok_port, sizeof(tok_port))) {
		printString("\nUsage: TCPCONN <dst_ip> <dst_port>\n");
		printString("Example: TCPCONN 10.0.2.2 80\n");
		return;
	}
	if(!parseIPv4(tok_ip, dst_ip) || !parsePort(tok_port, &dst_port)) {
		printString("\nTCPCONN: invalid IP or port.\n");
		return;
	}
	printString("\nTCPCONN: connecting to ");
	printIPv4(dst_ip);
	printString(":");
	printNumber((unsigned int)dst_port);
	printString(" ...\n");
	if(tcpClientConnect(dst_ip, dst_port, 900000)) {
		printString("TCPCONN: established.\n");
	} else {
		printString("TCPCONN: failed.\n");
	}
}

void tcpSendCmd(char* argument) {
	unsigned char payload[TCP_RX_MAX];
	unsigned short len = 0;
	char* p = skipSpaces(argument);
	if(!tcp_client_conn.valid || tcp_client_conn.state != TCP_ST_ESTABLISHED) {
		printString("\nTCPSEND: no established connection.\n");
		return;
	}
	while(p[len] != '\0' && len < TCP_RX_MAX) {
		payload[len] = (unsigned char)p[len];
		len++;
	}
	if(len == 0) {
		printString("\nUsage: TCPSEND <text>\n");
		return;
	}
	if(tcpClientSend(payload, len)) {
		printString("\nTCPSEND: sent ");
		printNumber((unsigned int)len);
		printString(" bytes.\n");
	} else {
		printString("\nTCPSEND failed.\n");
	}
}

void tcpRecvCmd() {
	if(!tcp_client_conn.valid) {
		printString("\nTCPRECV: no connection.\n");
		return;
	}
	if(tcp_client_conn.rx_len == 0) {
		printString("\nTCPRECV: no data.\n");
		return;
	}
	printString("\nTCPRECV: ");
	printNumber((unsigned int)tcp_client_conn.rx_len);
	printString(" bytes\n");
	for(unsigned short i = 0; i < tcp_client_conn.rx_len; i++) {
		char c = (char)tcp_client_conn.rx_buf[i];
		if(c >= 32 && c <= 126) printChar(c);
		else if(c == '\n' || c == '\r' || c == '\t') printChar(c);
		else printChar('.');
	}
	printChar('\n');
	tcp_client_conn.rx_len = 0;
}

void tcpCloseCmd() {
	if(!tcp_client_conn.valid) {
		printString("\nTCPCLOSE: no connection.\n");
		return;
	}
	tcpClientClose();
	printString("\nTCPCLOSE: closed.\n");
}

// Get CPU vendor string
void getCpuVendor(char* vendor) {
	unsigned int a, b, c, d;
	cpuid(0, &a, &b, &c, &d);
	// Vendor string is in EBX, EDX, ECX (in that order)
	*((unsigned int*)&vendor[0]) = b;
	*((unsigned int*)&vendor[4]) = d;
	*((unsigned int*)&vendor[8]) = c;
	vendor[12] = '\0';
}

// Get CPU brand string (if supported)
void getCpuBrand(char* brand) {
	unsigned int a, b, c, d;
	cpuid(0x80000000, &a, &b, &c, &d);
	if(a < 0x80000004) {
		brand[0] = '\0';
		return;
	}
	// Brand string is in 0x80000002, 0x80000003, 0x80000004
	cpuid(0x80000002, &a, &b, &c, &d);
	*((unsigned int*)&brand[0]) = a;
	*((unsigned int*)&brand[4]) = b;
	*((unsigned int*)&brand[8]) = c;
	*((unsigned int*)&brand[12]) = d;
	cpuid(0x80000003, &a, &b, &c, &d);
	*((unsigned int*)&brand[16]) = a;
	*((unsigned int*)&brand[20]) = b;
	*((unsigned int*)&brand[24]) = c;
	*((unsigned int*)&brand[28]) = d;
	cpuid(0x80000004, &a, &b, &c, &d);
	*((unsigned int*)&brand[32]) = a;
	*((unsigned int*)&brand[36]) = b;
	*((unsigned int*)&brand[40]) = c;
	*((unsigned int*)&brand[44]) = d;
	brand[48] = '\0';
}

// Display CPU info
void showCpuInfo() {
	printString("\n=== CPU Information ===\n");
	
	if(!cpuid_supported()) {
		printString("CPUID not supported on this CPU.\n");
		return;
	}
	
	char vendor[13];
	getCpuVendor(vendor);
	printString("Vendor: ");
	printString(vendor);
	printString("\n");
	
	char brand[49];
	getCpuBrand(brand);
	if(brand[0] != '\0') {
		printString("Brand:  ");
		// Skip leading spaces
		char* b = brand;
		while(*b == ' ') b++;
		printString(b);
		printString("\n");
	}
	
	// Get CPU features
	unsigned int a, b, c, d;
	cpuid(1, &a, &b, &c, &d);
	
	unsigned int stepping = a & 0xF;
	unsigned int model = (a >> 4) & 0xF;
	unsigned int family = (a >> 8) & 0xF;
	
	printString("Family: ");
	printNumber(family);
	printString("  Model: ");
	printNumber(model);
	printString("  Stepping: ");
	printNumber(stepping);
	printString("\n");
	
	printString("Features: ");
	if(d & (1 << 0)) printString("FPU ");
	if(d & (1 << 4)) printString("TSC ");
	if(d & (1 << 5)) printString("MSR ");
	if(d & (1 << 15)) printString("CMOV ");
	if(d & (1 << 23)) printString("MMX ");
	if(d & (1 << 25)) printString("SSE ");
	if(d & (1 << 26)) printString("SSE2 ");
	if(c & (1 << 0)) printString("SSE3 ");
	if(c & (1 << 9)) printString("SSSE3 ");
	if(c & (1 << 19)) printString("SSE4.1 ");
	if(c & (1 << 20)) printString("SSE4.2 ");
	if(c & (1 << 28)) printString("AVX ");
	printString("\n");
}

// ============================================
// HEAP DECLARATIONS (used by showMemInfo below)
// ============================================
#define HEAP_START  0x60000
#define HEAP_END    0x90000
#define HEAP_SIZE   (HEAP_END - HEAP_START)

struct heap_block {
	unsigned int size;   // Size of usable data area (excludes this header)
	unsigned int used;   // 0 = free, 1 = allocated
};

#define HEAP_HEADER_SIZE sizeof(struct heap_block)
#define HEAP_ALIGN 4  // Align all allocations to 4 bytes

static int heap_initialized = 0;

// Display memory info (basic - just show what we know)
void showMemInfo() {
	printString("\n=== Memory Information ===\n");
	printString("Kernel loaded at: 0x10000\n");
	printString("Page tables at:   0x30000\n");
	printString("User programs at: 0x50000\n");
	printString("Heap at:          0x60000-0x8FFFF\n");
	printString("Stack at:         0x90000\n");
	printString("Video memory:     0xB8000\n");
	printString("HDD sectors:      10MB allocated\n");
	
	printString("\nMemory layout:\n");
	printString("  0x00000-0x0FFFF: Low memory / bootloader\n");
	printString("  0x10000-0x2FFFF: Kernel (128 KB)\n");
	printString("  0x30000-0x31FFF: Page tables (8 KB)\n");
	printString("  0x32000-0x4FFFF: Free (120 KB)\n");
	printString("  0x50000-0x5FFFF: User programs (64 KB)\n");
	printString("  0x60000-0x8FFFF: Heap (192 KB)\n");
	printString("  0x90000-0x9FFFF: Stack (64 KB)\n");
	printString("  0xA0000-0xBFFFF: Video RAM\n");
	printString("  0xC0000-0xFFFFF: ROM/Reserved\n");

	// Show heap usage summary
	if(heap_initialized) {
		printString("\nHeap status:\n");
		// Quick walk to get used/free totals
		unsigned int total_free = 0, total_used = 0;
		struct heap_block* cur = (struct heap_block*)HEAP_START;
		while((unsigned int)cur < HEAP_END) {
			if(cur->used) total_used += cur->size;
			else total_free += cur->size;
			cur = (struct heap_block*)((char*)cur + HEAP_HEADER_SIZE + cur->size);
		}
		printString("  Used: ");
		printNumber(total_used);
		printString(" bytes  Free: ");
		printNumber(total_free);
		printString(" bytes\n");
		printString("  Type HEAP for detailed heap info.\n");
	}
}

// ============================================
// MEMORY PAGING
// ============================================

// Page structures are kernel globals aligned to 4 KB.
// This avoids hardcoded physical addresses that can overlap as the kernel grows.
#define PAGE_PRESENT    0x01
#define PAGE_WRITABLE   0x02
#define PAGE_USER       0x04

static int paging_enabled = 0;
static unsigned int paging_dir_phys = 0;
static unsigned int paging_table_phys = 0;
static unsigned int paging_table_vesa_phys = 0;
static unsigned int paging_table_vesa_alt_phys = 0;
static unsigned int paging_dir[1024] __attribute__((aligned(4096)));
static unsigned int paging_table0[1024] __attribute__((aligned(4096)));
static unsigned int paging_table_vesa[1024] __attribute__((aligned(4096)));
static unsigned int paging_table_vesa_alt[1024] __attribute__((aligned(4096)));

// Initialize identity paging: virtual address == physical address for first 4 MB
void paging_init() {
	unsigned int* page_dir = paging_dir;
	unsigned int* page_table_low = paging_table0;
	unsigned int* page_table_vesa_lfb = paging_table_vesa;
	unsigned int* page_table_vesa_lfb_alt = paging_table_vesa_alt;
	int i;

	// 1. Clear the entire page directory (1024 entries)
	for(i = 0; i < 1024; i++) {
		page_dir[i] = 0;
	}

	// 2. Fill page table 0 with identity-mapped entries for 0-4 MB
	//    Each entry maps a 4 KB page: entry = (physical_addr | flags)
	for(i = 0; i < 1024; i++) {
		page_table_low[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_WRITABLE;
	}

	// 3. Map the VESA LFB window (0xE0000000-0xE03FFFFF), identity-mapped.
	//    This gives graphics code access to the Bochs/QEMU linear framebuffer.
	for(i = 0; i < 1024; i++) {
		page_table_vesa_lfb[i] = (VESA_LFB_BASE + (i * 0x1000u)) | PAGE_PRESENT | PAGE_WRITABLE;
		page_table_vesa_lfb_alt[i] = (VESA_LFB_BASE_ALT + (i * 0x1000u)) | PAGE_PRESENT | PAGE_WRITABLE;
	}

	// 4. Point page directory entries to our page tables.
	page_dir[0] = ((unsigned int)page_table_low) | PAGE_PRESENT | PAGE_WRITABLE;
	page_dir[VESA_LFB_BASE >> 22] = ((unsigned int)page_table_vesa_lfb) | PAGE_PRESENT | PAGE_WRITABLE;
	page_dir[VESA_LFB_BASE_ALT >> 22] = ((unsigned int)page_table_vesa_lfb_alt) | PAGE_PRESENT | PAGE_WRITABLE;
	paging_dir_phys = (unsigned int)page_dir;
	paging_table_phys = (unsigned int)page_table_low;
	paging_table_vesa_phys = (unsigned int)page_table_vesa_lfb;
	paging_table_vesa_alt_phys = (unsigned int)page_table_vesa_lfb_alt;

	// 5. Load CR3 with the page directory physical address
	__asm__ __volatile__(
		"mov %0, %%cr3"
		:
		: "r"(paging_dir_phys)
	);

	// 6. Enable paging by setting CR0 bit 31 (PG)
	unsigned int cr0;
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= 0x80000000;  // Set PG bit
	__asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0));

	paging_enabled = 1;
}

// Page fault handler -- called from ISR14 stub in IDT.asm
// Reads CR2 (faulting address), prints diagnostic, and halts.
void isr14_Handler() {
	unsigned int faulting_addr;
	__asm__ __volatile__("mov %%cr2, %0" : "=r"(faulting_addr));

	printString("\n\n!!! PAGE FAULT !!!\n");
	printString("Faulting address: ");
	printHex(faulting_addr);
	printString("\n");

	// The error code was pushed by the CPU onto the stack.
	// Our ISR stub pushad'd, so the error code is at [ESP + 32].
	// However, since we're called via CALL (not directly from the stub's
	// stack frame), we just print a generic message here.
	printString("The CPU tried to access a page that is not mapped.\n");
	printString("System halted.\n");

	// Halt the CPU -- this fault is unrecoverable in ErisOS
	__asm__ __volatile__("cli; hlt");
}

// Display paging information (PGINFO command)
void showPagingInfo() {
	printString("\n=== Paging Information ===\n");

	if(!paging_enabled) {
		printString("Status: DISABLED\n");
		printString("Paging has not been initialized.\n");
		return;
	}

	printString("Status: ENABLED (identity + VESA LFB paging)\n\n");

	// Read and display CR0
	unsigned int cr0;
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
	printString("CR0: ");
	printHex(cr0);
	printString("\n");
	printString("  PE (Protection Enable): ");
	printString((cr0 & 0x01) ? "Yes" : "No");
	printString("\n  PG (Paging Enable):     ");
	printString((cr0 & 0x80000000) ? "Yes" : "No");
	printString("\n");

	// Read and display CR3
	unsigned int cr3;
	__asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
	printString("CR3 (Page Directory): ");
	printHex(cr3);
	printString("\n\n");

	printString("Page Directory:  ");
	printHex(paging_dir_phys);
	printString(" (4 KB)\n");
	printString("Page Table 0:    ");
	printHex(paging_table_phys);
	printString(" (4 KB)\n\n");
	printString("VESA Page Table: ");
	printHex(paging_table_vesa_phys);
	printString(" (4 KB)\n\n");

	printString("Mapped region:   0x00000000 - 0x003FFFFF\n");
	printString("VESA LFB map:    0xE0000000 - 0xE03FFFFF\n");
	printString("Mapping size:    8 MB total (2 x 4 MB tables)\n");
	printString("Mapping type:    Identity (virtual == physical)\n\n");

	// Show a few page table entries as examples
	unsigned int* pt = paging_table0;
	printString("Sample page table entries:\n");

	printString("  Page 0x00000 -> ");
	printHex(pt[0] & 0xFFFFF000);
	printString(" [");
	printString((pt[0] & PAGE_PRESENT) ? "P" : "-");
	printString((pt[0] & PAGE_WRITABLE) ? "W" : "R");
	printString("]\n");

	// Kernel page (0x10000 / 0x1000 = entry 16)
	printString("  Page 0x10000 -> ");
	printHex(pt[16] & 0xFFFFF000);
	printString(" [");
	printString((pt[16] & PAGE_PRESENT) ? "P" : "-");
	printString((pt[16] & PAGE_WRITABLE) ? "W" : "R");
	printString("]\n");

	// User program page (0x50000 / 0x1000 = entry 80)
	printString("  Page 0x50000 -> ");
	printHex(pt[80] & 0xFFFFF000);
	printString(" [");
	printString((pt[80] & PAGE_PRESENT) ? "P" : "-");
	printString((pt[80] & PAGE_WRITABLE) ? "W" : "R");
	printString("]\n");

	// VGA page (0xB8000 / 0x1000 = entry 184)
	printString("  Page 0xB8000 -> ");
	printHex(pt[184] & 0xFFFFF000);
	printString(" [");
	printString((pt[184] & PAGE_PRESENT) ? "P" : "-");
	printString((pt[184] & PAGE_WRITABLE) ? "W" : "R");
	printString("]\n");

	// VESA LFB first page (page directory index 896, table index 0)
	unsigned int* pt_vesa = paging_table_vesa;
	printString("  Page 0xE0000000 -> ");
	printHex(pt_vesa[0] & 0xFFFFF000);
	printString(" [");
	printString((pt_vesa[0] & PAGE_PRESENT) ? "P" : "-");
	printString((pt_vesa[0] & PAGE_WRITABLE) ? "W" : "R");
	printString("]\n");
}

// ============================================
// DYNAMIC MEMORY ALLOCATION (HEAP)
// ============================================
// Constants, struct, and heap_initialized are declared above showMemInfo().

// Initialize the heap with one giant free block
void heap_init() {
	struct heap_block* first = (struct heap_block*)HEAP_START;
	first->size = HEAP_SIZE - HEAP_HEADER_SIZE;
	first->used = 0;
	heap_initialized = 1;
}

// Allocate 'size' bytes from the heap (first-fit).
// Returns a pointer to the usable memory, or 0 (NULL) on failure.
void* kmalloc(unsigned int size) {
	if(!heap_initialized || size == 0) return (void*)0;

	// Align size up to HEAP_ALIGN boundary
	if(size & (HEAP_ALIGN - 1)) {
		size = (size + HEAP_ALIGN) & ~(HEAP_ALIGN - 1);
	}

	// Minimum allocation to avoid tiny unusable fragments
	if(size < HEAP_ALIGN) size = HEAP_ALIGN;

	struct heap_block* current = (struct heap_block*)HEAP_START;
	unsigned int heap_end = HEAP_END;

	while((unsigned int)current < heap_end) {
		if(!current->used && current->size >= size) {
			// Found a free block large enough

			// Split the block if there is enough room left for a new block + some data
			unsigned int remaining = current->size - size;
			if(remaining > HEAP_HEADER_SIZE + HEAP_ALIGN) {
				// Create a new free block after this allocation
				struct heap_block* next = (struct heap_block*)((char*)current + HEAP_HEADER_SIZE + size);
				next->size = remaining - HEAP_HEADER_SIZE;
				next->used = 0;
				current->size = size;
			}

			current->used = 1;
			// Return pointer just past the header
			return (void*)((char*)current + HEAP_HEADER_SIZE);
		}

		// Advance to the next block
		current = (struct heap_block*)((char*)current + HEAP_HEADER_SIZE + current->size);
	}

	return (void*)0;  // No block large enough found
}

// Free a previously allocated block and coalesce adjacent free blocks.
void kfree(void* ptr) {
	if(!heap_initialized || ptr == (void*)0) return;

	// The header sits immediately before the pointer the user received
	struct heap_block* block = (struct heap_block*)((char*)ptr - HEAP_HEADER_SIZE);

	// Basic sanity check: is this pointer within our heap?
	if((unsigned int)block < HEAP_START || (unsigned int)block >= HEAP_END) return;

	block->used = 0;

	// Coalesce adjacent free blocks (walk the entire list and merge neighbours)
	struct heap_block* current = (struct heap_block*)HEAP_START;
	unsigned int heap_end = HEAP_END;

	while((unsigned int)current < heap_end) {
		if(!current->used) {
			// Look at the next block and merge if it is also free
			struct heap_block* next = (struct heap_block*)((char*)current + HEAP_HEADER_SIZE + current->size);
			while((unsigned int)next < heap_end && !next->used) {
				// Absorb next block (its header + data) into current
				current->size += HEAP_HEADER_SIZE + next->size;
				next = (struct heap_block*)((char*)current + HEAP_HEADER_SIZE + current->size);
			}
		}
		current = (struct heap_block*)((char*)current + HEAP_HEADER_SIZE + current->size);
	}
}

// Display heap statistics (HEAP command)
void showHeapInfo() {
	printString("\n=== Heap Information ===\n");

	if(!heap_initialized) {
		printString("Status: NOT INITIALIZED\n");
		return;
	}

	printString("Status: ACTIVE\n\n");

	printString("Heap start:  ");
	printHex(HEAP_START);
	printString("\n");
	printString("Heap end:    ");
	printHex(HEAP_END);
	printString("\n");
	printString("Total size:  ");
	printNumber(HEAP_SIZE / 1024);
	printString(" KB (");
	printNumber(HEAP_SIZE);
	printString(" bytes)\n\n");

	// Walk the block list and collect statistics
	unsigned int total_free = 0;
	unsigned int total_used = 0;
	unsigned int block_count = 0;
	unsigned int free_blocks = 0;
	unsigned int used_blocks = 0;
	unsigned int largest_free = 0;

	struct heap_block* current = (struct heap_block*)HEAP_START;
	unsigned int heap_end = HEAP_END;

	while((unsigned int)current < heap_end) {
		block_count++;
		if(current->used) {
			used_blocks++;
			total_used += current->size;
		} else {
			free_blocks++;
			total_free += current->size;
			if(current->size > largest_free) {
				largest_free = current->size;
			}
		}
		current = (struct heap_block*)((char*)current + HEAP_HEADER_SIZE + current->size);
	}

	printString("Blocks:      ");
	printNumber(block_count);
	printString(" total (");
	printNumber(used_blocks);
	printString(" used, ");
	printNumber(free_blocks);
	printString(" free)\n");

	printString("Used memory: ");
	printNumber(total_used);
	printString(" bytes");
	if(total_used >= 1024) {
		printString(" (");
		printNumber(total_used / 1024);
		printString(" KB)");
	}
	printString("\n");

	printString("Free memory: ");
	printNumber(total_free);
	printString(" bytes");
	if(total_free >= 1024) {
		printString(" (");
		printNumber(total_free / 1024);
		printString(" KB)");
	}
	printString("\n");

	printString("Largest free block: ");
	printNumber(largest_free);
	printString(" bytes");
	if(largest_free >= 1024) {
		printString(" (");
		printNumber(largest_free / 1024);
		printString(" KB)");
	}
	printString("\n");

	printString("Overhead:    ");
	printNumber(block_count * HEAP_HEADER_SIZE);
	printString(" bytes (");
	printNumber(block_count);
	printString(" headers x ");
	printNumber(HEAP_HEADER_SIZE);
	printString(" bytes)\n");

	// Show first few blocks for debugging
	printString("\nBlock list (first 8):\n");
	current = (struct heap_block*)HEAP_START;
	int shown = 0;
	while((unsigned int)current < heap_end && shown < 8) {
		printString("  ");
		printHex((unsigned int)current);
		printString(": ");
		printNumber(current->size);
		printString(" bytes [");
		printString(current->used ? "USED" : "FREE");
		printString("]\n");
		current = (struct heap_block*)((char*)current + HEAP_HEADER_SIZE + current->size);
		shown++;
	}
	if((unsigned int)current < heap_end) {
		printString("  ... (");
		printNumber(block_count - shown);
		printString(" more blocks)\n");
	}
}

// Display system info
void showSysInfo() {
	printString("\n========================================\n");
	printString("         ErisOS System Information\n");
	printString("========================================\n\n");
	
	printString("OS Version:    ErisOS 1.0\n");
	printString("Architecture:  x86 (32-bit Protected Mode)\n");
	printString("Display:       VGA Text 80x25 + VESA LFB 800x600x32\n");
	printString("Input:         PS/2 Keyboard + Mouse\n");
	printString("Storage:       ATA PIO Hard Disk\n");
	printString("Audio:         PC Speaker (PIT Channel 2)\n");
	
	// CPU vendor quick check
	if(cpuid_supported()) {
		char vendor[13];
		getCpuVendor(vendor);
		printString("CPU Vendor:    ");
		printString(vendor);
		printString("\n");
	}
	
	printString("Paging:        ");
	printString(paging_enabled ? "Enabled (identity + VESA LFB map)" : "Disabled");
	printString("\n");
	printString("Heap:          ");
	printString(heap_initialized ? "Active (192 KB at 0x60000)" : "Not initialized");
	printString("\n");
	printString("Uptime:        ");
	{
		unsigned int secs = pit_ticks / 100;
		unsigned int h = secs / 3600;
		unsigned int m = (secs % 3600) / 60;
		unsigned int s = secs % 60;
		if(h > 0) { printNumber(h); printString("h "); }
		printNumber(m);
		printString("m ");
		printNumber(s);
		printString("s\n");
	}
	
	printString("\nType CPUINFO for detailed CPU info.\n");
	printString("Type MEMINFO for memory information.\n");
	printString("Type PGINFO for paging details.\n");
	printString("Type HEAP for heap memory info.\n");
}

// Display uptime (computed from PIT ticks at 100 Hz)
void showUptime() {
	unsigned int total_seconds = pit_ticks / 100;
	unsigned int hours = total_seconds / 3600;
	unsigned int minutes = (total_seconds % 3600) / 60;
	unsigned int seconds = total_seconds % 60;
	unsigned int ticks = pit_ticks % 100;  // Sub-second ticks

	printString("\n=== System Uptime ===\n");

	// Print HH:MM:SS format
	if(hours > 0) {
		printNumber(hours);
		printString("h ");
	}
	printNumber(minutes);
	printString("m ");
	printNumber(seconds);
	printString(".");
	// Print centiseconds with leading zero
	if(ticks < 10) printString("0");
	printNumber(ticks);
	printString("s\n");

	printString("Total ticks: ");
	printNumber(pit_ticks);
	printString(" (100 Hz PIT)\n");
}

// Help output helper
void helpPrint(char* s) {
	printString(s);
}

// ============================================
// FULL-SCREEN TEXT EDITOR
// ============================================

// Editor mode flag (defined in main.c)
extern int editor_mode;

// VGA cursor functions (defined in main.c)
void setCursor(int x, int y);
void enableCursor();
void disableCursor();
void printCharAt(char c, int x, int y, char color);
char getCharAt(int x, int y);

// Editor constants
#define EDITOR_MAX_LINES 50
#define EDITOR_LINE_LEN 80
#define EDITOR_VIEW_LINES 24  // Lines 0-23 for editing, line 24 for status

// Editor state
char editor_buffer[EDITOR_MAX_LINES][EDITOR_LINE_LEN];
int editor_num_lines;           // Total lines in buffer
int editor_cx, editor_cy;       // Cursor position in buffer
int editor_scroll_y;            // First visible line (for scrolling)
char editor_filename[16];       // Current filename
int editor_modified;            // Has file been modified?
unsigned char editor_color;     // Editor color scheme

// Initialize editor state
void editorInit(char* filename) {
	int i, j;
	// Clear buffer
	for(i = 0; i < EDITOR_MAX_LINES; i++) {
		for(j = 0; j < EDITOR_LINE_LEN; j++) {
			editor_buffer[i][j] = 0;
		}
	}
	editor_num_lines = 1;  // Start with one empty line
	editor_cx = 0;
	editor_cy = 0;
	editor_scroll_y = 0;
	editor_modified = 0;
	editor_color = 0x1f;  // White on blue (like DOS EDIT)
	
	// Copy filename
	for(i = 0; i < 15 && filename[i] != '\0'; i++) {
		editor_filename[i] = filename[i];
	}
	editor_filename[i] = '\0';
}

// Get line length (find null or end)
int editorLineLen(int line) {
	if(line < 0 || line >= EDITOR_MAX_LINES) return 0;
	int len = 0;
	while(len < EDITOR_LINE_LEN - 1 && editor_buffer[line][len] != 0) {
		len++;
	}
	return len;
}

// Draw a single line on screen
void editorDrawLine(int screen_y, int buffer_y) {
	int x;
	char c;
	for(x = 0; x < 80; x++) {
		if(buffer_y < editor_num_lines && x < EDITOR_LINE_LEN) {
			c = editor_buffer[buffer_y][x];
			if(c == 0) c = ' ';
		} else {
			c = ' ';
		}
		printCharAt(c, x, screen_y, editor_color);
	}
}

// Draw status bar at bottom of screen
void editorDrawStatusBar() {
	int x;
	unsigned char status_color = 0x70;  // Black on gray
	
	// Clear status line
	for(x = 0; x < 80; x++) {
		printCharAt(' ', x, 24, status_color);
	}
	
	// Draw status info
	char* label = " EDIT: ";
	x = 0;
	while(*label && x < 7) {
		printCharAt(*label++, x++, 24, status_color);
	}
	
	// Filename
	int i = 0;
	while(editor_filename[i] && x < 20) {
		printCharAt(editor_filename[i++], x++, 24, status_color);
	}
	
	// Modified indicator
	if(editor_modified) {
		printCharAt('*', x++, 24, status_color);
	}
	
	// Line info
	x = 25;
	char* line_label = " Line ";
	while(*line_label && x < 32) {
		printCharAt(*line_label++, x++, 24, status_color);
	}
	
	// Current line number
	char num[8];
	int pos = 0;
	int val = editor_cy + 1;
	if(val == 0) num[pos++] = '0';
	else {
		while(val > 0 && pos < 7) {
			num[pos++] = '0' + (val % 10);
			val /= 10;
		}
	}
	// Reverse
	int s = 0, e = pos - 1;
	while(s < e) { char t = num[s]; num[s] = num[e]; num[e] = t; s++; e--; }
	num[pos] = 0;
	i = 0;
	while(num[i] && x < 40) {
		printCharAt(num[i++], x++, 24, status_color);
	}
	
	printCharAt('/', x++, 24, status_color);
	
	// Total lines
	pos = 0;
	val = editor_num_lines;
	if(val == 0) num[pos++] = '0';
	else {
		while(val > 0 && pos < 7) {
			num[pos++] = '0' + (val % 10);
			val /= 10;
		}
	}
	s = 0; e = pos - 1;
	while(s < e) { char t = num[s]; num[s] = num[e]; num[e] = t; s++; e--; }
	num[pos] = 0;
	i = 0;
	while(num[i] && x < 50) {
		printCharAt(num[i++], x++, 24, status_color);
	}
	
	// Help text
	x = 55;
	char* help = "F2=Save ESC=Exit";
	while(*help && x < 80) {
		printCharAt(*help++, x++, 24, status_color);
	}
}

// Redraw entire editor screen
void editorRefresh() {
	int y;
	for(y = 0; y < EDITOR_VIEW_LINES; y++) {
		int buffer_line = editor_scroll_y + y;
		editorDrawLine(y, buffer_line);
	}
	editorDrawStatusBar();
	
	// Position hardware cursor
	int screen_y = editor_cy - editor_scroll_y;
	if(screen_y >= 0 && screen_y < EDITOR_VIEW_LINES) {
		setCursor(editor_cx, screen_y);
	}
}

// Ensure cursor is visible (adjust scroll if needed)
void editorEnsureCursorVisible() {
	if(editor_cy < editor_scroll_y) {
		editor_scroll_y = editor_cy;
	}
	if(editor_cy >= editor_scroll_y + EDITOR_VIEW_LINES) {
		editor_scroll_y = editor_cy - EDITOR_VIEW_LINES + 1;
	}
}

// Insert character at cursor position
void editorInsertChar(char c) {
	if(editor_cy >= EDITOR_MAX_LINES) return;
	
	int line_len = editorLineLen(editor_cy);
	if(line_len >= EDITOR_LINE_LEN - 2) return;  // Line full
	
	// Shift characters right from cursor
	int i;
	for(i = line_len; i > editor_cx; i--) {
		editor_buffer[editor_cy][i] = editor_buffer[editor_cy][i-1];
	}
	
	// Insert character
	editor_buffer[editor_cy][editor_cx] = c;
	editor_cx++;
	editor_modified = 1;
}

// Delete character before cursor (backspace)
void editorBackspace() {
	if(editor_cx > 0) {
		// Delete character before cursor
		int line_len = editorLineLen(editor_cy);
		int i;
		for(i = editor_cx - 1; i < line_len; i++) {
			editor_buffer[editor_cy][i] = editor_buffer[editor_cy][i+1];
		}
		editor_cx--;
		editor_modified = 1;
	}
	else if(editor_cy > 0) {
		// Join with previous line
		int prev_len = editorLineLen(editor_cy - 1);
		int curr_len = editorLineLen(editor_cy);
		
		if(prev_len + curr_len < EDITOR_LINE_LEN - 1) {
			// Append current line to previous
			int i;
			for(i = 0; i < curr_len; i++) {
				editor_buffer[editor_cy - 1][prev_len + i] = editor_buffer[editor_cy][i];
			}
			editor_buffer[editor_cy - 1][prev_len + curr_len] = 0;
			
			// Shift all lines up
			for(i = editor_cy; i < editor_num_lines - 1; i++) {
				int j;
				for(j = 0; j < EDITOR_LINE_LEN; j++) {
					editor_buffer[i][j] = editor_buffer[i+1][j];
				}
			}
			// Clear last line
			for(i = 0; i < EDITOR_LINE_LEN; i++) {
				editor_buffer[editor_num_lines - 1][i] = 0;
			}
			
			editor_num_lines--;
			editor_cy--;
			editor_cx = prev_len;
			editor_modified = 1;
		}
	}
}

// Insert new line at cursor (Enter key)
void editorNewLine() {
	if(editor_num_lines >= EDITOR_MAX_LINES) return;
	
	// Shift lines down
	int i, j;
	for(i = editor_num_lines; i > editor_cy + 1; i--) {
		for(j = 0; j < EDITOR_LINE_LEN; j++) {
			editor_buffer[i][j] = editor_buffer[i-1][j];
		}
	}
	
	// Split current line at cursor
	int curr_len = editorLineLen(editor_cy);
	for(i = editor_cx; i <= curr_len; i++) {
		editor_buffer[editor_cy + 1][i - editor_cx] = editor_buffer[editor_cy][i];
	}
	// Truncate current line
	for(i = editor_cx; i < EDITOR_LINE_LEN; i++) {
		editor_buffer[editor_cy][i] = 0;
	}
	
	editor_num_lines++;
	editor_cy++;
	editor_cx = 0;
	editor_modified = 1;
	editorEnsureCursorVisible();
}

// Cursor movement
void editorMoveUp() {
	if(editor_cy > 0) {
		editor_cy--;
		int line_len = editorLineLen(editor_cy);
		if(editor_cx > line_len) editor_cx = line_len;
		editorEnsureCursorVisible();
	}
}

void editorMoveDown() {
	if(editor_cy < editor_num_lines - 1) {
		editor_cy++;
		int line_len = editorLineLen(editor_cy);
		if(editor_cx > line_len) editor_cx = line_len;
		editorEnsureCursorVisible();
	}
}

void editorMoveLeft() {
	if(editor_cx > 0) {
		editor_cx--;
	} else if(editor_cy > 0) {
		editor_cy--;
		editor_cx = editorLineLen(editor_cy);
		editorEnsureCursorVisible();
	}
}

void editorMoveRight() {
	int line_len = editorLineLen(editor_cy);
	if(editor_cx < line_len) {
		editor_cx++;
	} else if(editor_cy < editor_num_lines - 1) {
		editor_cy++;
		editor_cx = 0;
		editorEnsureCursorVisible();
	}
}

// Save file to filesystem
void editorSave() {
	// Calculate total size
	int total_size = 0;
	int i, j;
	for(i = 0; i < editor_num_lines; i++) {
		total_size += editorLineLen(i) + 1;  // +1 for newline
	}
	
	// Build output buffer (reuse a static buffer)
	static char save_buf[4096];
	int pos = 0;
	
	for(i = 0; i < editor_num_lines && pos < 4090; i++) {
		int len = editorLineLen(i);
		for(j = 0; j < len && pos < 4090; j++) {
			save_buf[pos++] = editor_buffer[i][j];
		}
		save_buf[pos++] = '\n';
	}
	save_buf[pos] = 0;
	
	// Write to filesystem
	if(fs_write_file(editor_filename, save_buf, pos)) {
		editor_modified = 0;
		editorDrawStatusBar();
	}
}

// Load file into editor
void editorLoad(char* filename) {
	static char load_buf[4096];
	int bytes = fs_read_file(filename, load_buf, 4095);
	
	if(bytes <= 0) return;  // File not found or empty, start fresh
	
	load_buf[bytes] = 0;
	
	// Parse into lines
	int line = 0;
	int col = 0;
	int i;
	
	for(i = 0; i < bytes && line < EDITOR_MAX_LINES; i++) {
		if(load_buf[i] == '\n' || load_buf[i] == '\r') {
			editor_buffer[line][col] = 0;
			line++;
			col = 0;
			// Skip \r\n combo
			if(load_buf[i] == '\r' && load_buf[i+1] == '\n') i++;
		} else if(col < EDITOR_LINE_LEN - 1) {
			editor_buffer[line][col++] = load_buf[i];
		}
	}
	// Handle last line without newline
	if(col > 0) {
		editor_buffer[line][col] = 0;
		line++;
	}
	
	editor_num_lines = (line > 0) ? line : 1;
}

// Exit editor
void editorExit() {
	editor_mode = 0;
	cls();
	printString("Exited editor.\n\nOS0 > ");
}

// Handle key input in editor mode
void editorHandleKey(int code, int is_extended, int shift) {
	// Normal (unshifted) scancode lookup - lowercase letters
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
	
	// Shifted scancode lookup - uppercase letters + symbols
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
	
	// Handle extended keys (arrow keys)
	if(is_extended) {
		switch(code) {
			case 0x48: editorMoveUp(); break;    // Up arrow
			case 0x50: editorMoveDown(); break;  // Down arrow
			case 0x4B: editorMoveLeft(); break;  // Left arrow
			case 0x4D: editorMoveRight(); break; // Right arrow
		}
		editorRefresh();
		return;
	}
	
	// Handle special keys
	switch(code) {
		case 0x01:  // Escape - exit
			editorExit();
			return;
		case 0x3C:  // F2 - save
			editorSave();
			editorRefresh();
			return;
		case 0x1C:  // Enter
			editorNewLine();
			editorRefresh();
			return;
		case 0x0E:  // Backspace
			editorBackspace();
			editorRefresh();
			return;
		case 0x0F:  // Tab - insert spaces
			editorInsertChar(' ');
			editorInsertChar(' ');
			editorInsertChar(' ');
			editorInsertChar(' ');
			editorRefresh();
			return;
	}
	
	// Handle printable characters
	if(code < 0x3A) {
		char c;
		if(shift) {
			c = ShiftedScancode[code];
		} else {
			c = Scancode[code];
		}
		if(c != 0) {
			editorInsertChar(c);
			editorRefresh();
		}
	}
}

// Start the editor
void editorStart(char* filename) {
	editorInit(filename);
	editorLoad(filename);
	editor_mode = 1;
	enableCursor();
	editorRefresh();
}

// ============================================
// SIMPLE x86 ASSEMBLER
// ============================================

// Assembler output buffer
static unsigned char asm_output[4096];
static int asm_output_pos;

// Label storage for two-pass assembly
#define ASM_MAX_LABELS 32
static char asm_labels[ASM_MAX_LABELS][16];
static int asm_label_addrs[ASM_MAX_LABELS];
static int asm_num_labels;

// Forward reference storage (for absolute addresses like PUSH label)
#define ASM_MAX_REFS 32
static char asm_refs[ASM_MAX_REFS][16];
static int asm_ref_addrs[ASM_MAX_REFS];
static int asm_num_refs;

// Relative jump reference storage (for JMP, JE, JNE, etc.)
#define ASM_MAX_JMPREFS 32
static char asm_jmprefs[ASM_MAX_JMPREFS][16];
static int asm_jmpref_addrs[ASM_MAX_JMPREFS];    // Address where offset is stored
static int asm_jmpref_sizes[ASM_MAX_JMPREFS];    // 1 for rel8, 4 for rel32
static int asm_num_jmprefs;

// Skip whitespace
char* asmSkipWS(char* p) {
	while(*p == ' ' || *p == '\t') p++;
	return p;
}

// Check if char is alphanumeric or underscore
int asmIsAlnum(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
	       (c >= '0' && c <= '9') || c == '_';
}

// Parse identifier (returns pointer after identifier, copies to buf)
char* asmParseIdent(char* p, char* buf, int maxlen) {
	int i = 0;
	p = asmSkipWS(p);
	while(asmIsAlnum(*p) && i < maxlen - 1) {
		buf[i++] = *p++;
	}
	buf[i] = 0;
	return p;
}

// Convert string to uppercase in place
void asmToUpper(char* s) {
	while(*s) {
		if(*s >= 'a' && *s <= 'z') *s = *s - 'a' + 'A';
		s++;
	}
}

// Parse register name, return register number (0-7) or -1 if not a register
// eax=0, ecx=1, edx=2, ebx=3, esp=4, ebp=5, esi=6, edi=7
int asmParseReg(char* s) {
	asmToUpper(s);
	if(strcmp(s, "EAX")) return 0;
	if(strcmp(s, "ECX")) return 1;
	if(strcmp(s, "EDX")) return 2;
	if(strcmp(s, "EBX")) return 3;
	if(strcmp(s, "ESP")) return 4;
	if(strcmp(s, "EBP")) return 5;
	if(strcmp(s, "ESI")) return 6;
	if(strcmp(s, "EDI")) return 7;
	return -1;
}

// Parse number (decimal or hex with 0x prefix)
int asmParseNum(char* p, int* val) {
	*val = 0;
	p = asmSkipWS(p);
	
	// Check for hex
	if(*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
		p += 2;
		while(1) {
			if(*p >= '0' && *p <= '9') {
				*val = (*val << 4) | (*p - '0');
			} else if(*p >= 'A' && *p <= 'F') {
				*val = (*val << 4) | (*p - 'A' + 10);
			} else if(*p >= 'a' && *p <= 'f') {
				*val = (*val << 4) | (*p - 'a' + 10);
			} else {
				break;
			}
			p++;
		}
		return 1;
	}
	
	// Decimal
	if(*p >= '0' && *p <= '9') {
		while(*p >= '0' && *p <= '9') {
			*val = *val * 10 + (*p - '0');
			p++;
		}
		return 1;
	}
	
	return 0;  // Not a number
}

// Emit byte to output
void asmEmit(unsigned char b) {
	if(asm_output_pos < 4096) {
		asm_output[asm_output_pos++] = b;
	}
}

// Emit 32-bit value (little endian)
void asmEmit32(unsigned int val) {
	asmEmit(val & 0xFF);
	asmEmit((val >> 8) & 0xFF);
	asmEmit((val >> 16) & 0xFF);
	asmEmit((val >> 24) & 0xFF);
}

// Add a label
void asmAddLabel(char* name, int addr) {
	if(asm_num_labels < ASM_MAX_LABELS) {
		int i;
		for(i = 0; i < 15 && name[i]; i++) {
			asm_labels[asm_num_labels][i] = name[i];
		}
		asm_labels[asm_num_labels][i] = 0;
		asmToUpper(asm_labels[asm_num_labels]);
		asm_label_addrs[asm_num_labels] = addr;
		asm_num_labels++;
	}
}

// Find label address
int asmFindLabel(char* name) {
	char upper[16];
	int i;
	for(i = 0; i < 15 && name[i]; i++) {
		upper[i] = name[i];
	}
	upper[i] = 0;
	asmToUpper(upper);
	
	for(i = 0; i < asm_num_labels; i++) {
		if(strcmp(asm_labels[i], upper)) {
			return asm_label_addrs[i];
		}
	}
	return -1;
}

// Add forward reference (for absolute addresses)
void asmAddRef(char* name, int addr) {
	if(asm_num_refs < ASM_MAX_REFS) {
		int i;
		for(i = 0; i < 15 && name[i]; i++) {
			asm_refs[asm_num_refs][i] = name[i];
		}
		asm_refs[asm_num_refs][i] = 0;
		asmToUpper(asm_refs[asm_num_refs]);
		asm_ref_addrs[asm_num_refs] = addr;
		asm_num_refs++;
	}
}

// Add jump reference (for relative offsets)
void asmAddJmpRef(char* name, int addr, int size) {
	if(asm_num_jmprefs < ASM_MAX_JMPREFS) {
		int i;
		for(i = 0; i < 15 && name[i]; i++) {
			asm_jmprefs[asm_num_jmprefs][i] = name[i];
		}
		asm_jmprefs[asm_num_jmprefs][i] = 0;
		asmToUpper(asm_jmprefs[asm_num_jmprefs]);
		asm_jmpref_addrs[asm_num_jmprefs] = addr;
		asm_jmpref_sizes[asm_num_jmprefs] = size;
		asm_num_jmprefs++;
	}
}

// Resolve forward references (absolute addresses)
void asmResolveRefs(int base_addr) {
	int i;
	for(i = 0; i < asm_num_refs; i++) {
		int label_addr = asmFindLabel(asm_refs[i]);
		if(label_addr >= 0) {
			// Patch the address (absolute address = base + label offset)
			int abs_addr = base_addr + label_addr;
			int patch_pos = asm_ref_addrs[i];
			asm_output[patch_pos] = abs_addr & 0xFF;
			asm_output[patch_pos + 1] = (abs_addr >> 8) & 0xFF;
			asm_output[patch_pos + 2] = (abs_addr >> 16) & 0xFF;
			asm_output[patch_pos + 3] = (abs_addr >> 24) & 0xFF;
		}
	}
}

// Resolve jump references (relative offsets)
int asmResolveJmpRefs() {
	int i;
	for(i = 0; i < asm_num_jmprefs; i++) {
		int label_addr = asmFindLabel(asm_jmprefs[i]);
		if(label_addr < 0) {
			printString("\n[ERROR] Undefined label: ");
			printString(asm_jmprefs[i]);
			printString("\n");
			return 0;
		}
		
		int patch_pos = asm_jmpref_addrs[i];
		int size = asm_jmpref_sizes[i];
		
		// Calculate relative offset from end of instruction
		// offset = target - (patch_pos + size)
		int offset = label_addr - (patch_pos + size);
		
		if(size == 1) {
			// rel8: offset must fit in signed byte
			if(offset < -128 || offset > 127) {
				printString("\n[ERROR] Jump too far for rel8: ");
				printString(asm_jmprefs[i]);
				printString("\n");
				return 0;
			}
			asm_output[patch_pos] = (unsigned char)(offset & 0xFF);
		} else {
			// rel32
			asm_output[patch_pos] = offset & 0xFF;
			asm_output[patch_pos + 1] = (offset >> 8) & 0xFF;
			asm_output[patch_pos + 2] = (offset >> 16) & 0xFF;
			asm_output[patch_pos + 3] = (offset >> 24) & 0xFF;
		}
	}
	return 1;
}

// Assemble a single line
// Returns 1 on success, 0 on error
int asmAssembleLine(char* line) {
	char instr[16];
	char op1[32];
	char op2[32];
	int val;
	
	// Skip leading whitespace
	line = asmSkipWS(line);
	
	// Skip empty lines and comments
	if(*line == 0 || *line == '\n' || *line == ';') return 1;
	
	// Check for label (ends with :)
	// Scan the whole line for a colon that's part of a label definition
	char* colon = line;
	char* label_end = line;
	// Find first identifier
	while(*label_end && asmIsAlnum(*label_end)) label_end++;
	// Skip optional whitespace before colon
	char* maybe_colon = label_end;
	while(*maybe_colon == ' ' || *maybe_colon == '\t') maybe_colon++;
	// Check if there's a colon
	if(*maybe_colon == ':' && label_end > line) {
		// This is a label
		char label[16];
		int i;
		for(i = 0; i < 15 && line + i < label_end; i++) {
			label[i] = line[i];
		}
		label[i] = 0;
		asmAddLabel(label, asm_output_pos);
		line = maybe_colon + 1;
		line = asmSkipWS(line);
		if(*line == 0 || *line == '\n' || *line == ';') return 1;
	}
	
	// Parse instruction
	line = asmParseIdent(line, instr, 16);
	asmToUpper(instr);
	
	// Save full arguments (for DB directive which can have many values)
	static char full_args[128];
	line = asmSkipWS(line);
	int fa_i = 0;
	char* fa_start = line;
	while(*fa_start && *fa_start != ';' && *fa_start != '\n' && fa_i < 127) {
		full_args[fa_i++] = *fa_start++;
	}
	full_args[fa_i] = 0;
	// Trim trailing whitespace from full_args
	while(fa_i > 0 && (full_args[fa_i-1] == ' ' || full_args[fa_i-1] == '\t')) {
		full_args[--fa_i] = 0;
	}
	
	// Parse operands (for normal instructions)
	op1[0] = 0;
	op2[0] = 0;
	
	if(*line && *line != ';' && *line != '\n') {
		// Get first operand
		int i = 0;
		while(*line && *line != ',' && *line != ';' && *line != '\n' && i < 31) {
			op1[i++] = *line++;
		}
		op1[i] = 0;
		// Trim trailing whitespace
		while(i > 0 && (op1[i-1] == ' ' || op1[i-1] == '\t')) op1[--i] = 0;
		
		// Check for second operand
		if(*line == ',') {
			line++;
			line = asmSkipWS(line);
			i = 0;
			while(*line && *line != ';' && *line != '\n' && i < 31) {
				op2[i++] = *line++;
			}
			op2[i] = 0;
			while(i > 0 && (op2[i-1] == ' ' || op2[i-1] == '\t')) op2[--i] = 0;
		}
	}
	
	// Assemble instruction
	if(strcmp(instr, "NOP")) {
		asmEmit(0x90);
	}
	else if(strcmp(instr, "RET")) {
		asmEmit(0xC3);
	}
	else if(strcmp(instr, "PUSH")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// push reg: 50+r
			asmEmit(0x50 + reg);
		} else if(asmParseNum(op1, &val)) {
			// push imm32: 68 + imm32
			asmEmit(0x68);
			asmEmit32(val);
		} else {
			// Push label address
			asmEmit(0x68);
			asmAddRef(op1, asm_output_pos);
			asmEmit32(0);  // Placeholder
		}
	}
	else if(strcmp(instr, "POP")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// pop reg: 58+r
			asmEmit(0x58 + reg);
		}
	}
	else if(strcmp(instr, "MOV")) {
		asmToUpper(op1);
		
		// Check for mov reg, imm32
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// mov reg, ...
			if(op2[0] == '[') {
				// mov reg, [mem]
				// Parse memory operand
				char memop[32];
				int i = 0, j = 1;
				while(op2[j] && op2[j] != ']' && i < 31) {
					memop[i++] = op2[j++];
				}
				memop[i] = 0;
				asmToUpper(memop);
				
				// Check for [reg] or [reg+offset]
				int base_reg = asmParseReg(memop);
				if(base_reg >= 0) {
					// mov reg, [base_reg]
					asmEmit(0x8B);
					if(base_reg == 5) {  // EBP needs special encoding
						asmEmit((reg << 3) | 0x45);
						asmEmit(0x00);  // [ebp+0]
					} else {
						asmEmit((reg << 3) | base_reg);
					}
				}
				// Check for [reg+imm]
				char* plus = memop;
				while(*plus && *plus != '+') plus++;
				if(*plus == '+') {
					*plus = 0;
					plus++;
					base_reg = asmParseReg(memop);
					int offset;
					asmParseNum(plus, &offset);
					asmEmit(0x8B);
					if(offset >= -128 && offset <= 127) {
						asmEmit(0x40 | (reg << 3) | base_reg);
						asmEmit((char)offset);
					} else {
						asmEmit(0x80 | (reg << 3) | base_reg);
						asmEmit32(offset);
					}
				}
			} else {
				// mov reg, imm32 or mov reg, reg2
				int reg2 = asmParseReg(op2);
				if(reg2 >= 0) {
					// mov reg, reg2: 89 /r
					asmEmit(0x89);
					asmEmit(0xC0 | (reg2 << 3) | reg);
				} else if(asmParseNum(op2, &val)) {
					// mov reg, imm32: B8+r + imm32
					asmEmit(0xB8 + reg);
					asmEmit32(val);
				} else {
					// mov reg, label
					asmEmit(0xB8 + reg);
					asmAddRef(op2, asm_output_pos);
					asmEmit32(0);
				}
			}
		}
	}
	else if(strcmp(instr, "ADD")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			int val;
			if(asmParseNum(op2, &val)) {
				if(val >= -128 && val <= 127) {
					// add reg, imm8: 83 /0 ib
					asmEmit(0x83);
					asmEmit(0xC0 | reg);
					asmEmit((char)val);
				} else {
					// add reg, imm32: 81 /0 id
					asmEmit(0x81);
					asmEmit(0xC0 | reg);
					asmEmit32(val);
				}
			}
		}
	}
	else if(strcmp(instr, "SUB")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			int val;
			if(asmParseNum(op2, &val)) {
				if(val >= -128 && val <= 127) {
					// sub reg, imm8: 83 /5 ib
					asmEmit(0x83);
					asmEmit(0xE8 | reg);
					asmEmit((char)val);
				}
			}
		}
	}
	else if(strcmp(instr, "XOR")) {
		asmToUpper(op1);
		asmToUpper(op2);
		int reg1 = asmParseReg(op1);
		int reg2 = asmParseReg(op2);
		if(reg1 >= 0 && reg2 >= 0) {
			// xor reg1, reg2: 31 /r
			asmEmit(0x31);
			asmEmit(0xC0 | (reg2 << 3) | reg1);
		}
	}
	else if(strcmp(instr, "AND")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			int reg2 = asmParseReg(op2);
			if(reg2 >= 0) {
				// and reg, reg2: 21 /r
				asmEmit(0x21);
				asmEmit(0xC0 | (reg2 << 3) | reg);
			} else {
				int val;
				if(asmParseNum(op2, &val)) {
					if(val >= -128 && val <= 127) {
						// and reg, imm8: 83 /4 ib
						asmEmit(0x83);
						asmEmit(0xE0 | reg);
						asmEmit((char)val);
					} else {
						// and reg, imm32: 81 /4 id
						asmEmit(0x81);
						asmEmit(0xE0 | reg);
						asmEmit32(val);
					}
				}
			}
		}
	}
	else if(strcmp(instr, "OR")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			int reg2 = asmParseReg(op2);
			if(reg2 >= 0) {
				// or reg, reg2: 09 /r
				asmEmit(0x09);
				asmEmit(0xC0 | (reg2 << 3) | reg);
			} else {
				int val;
				if(asmParseNum(op2, &val)) {
					if(val >= -128 && val <= 127) {
						// or reg, imm8: 83 /1 ib
						asmEmit(0x83);
						asmEmit(0xC8 | reg);
						asmEmit((char)val);
					} else {
						// or reg, imm32: 81 /1 id
						asmEmit(0x81);
						asmEmit(0xC8 | reg);
						asmEmit32(val);
					}
				}
			}
		}
	}
	else if(strcmp(instr, "NOT")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// not reg: F7 /2
			asmEmit(0xF7);
			asmEmit(0xD0 | reg);
		}
	}
	else if(strcmp(instr, "NEG")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// neg reg: F7 /3
			asmEmit(0xF7);
			asmEmit(0xD8 | reg);
		}
	}
	else if(strcmp(instr, "INC")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// inc reg: 40+r
			asmEmit(0x40 + reg);
		}
	}
	else if(strcmp(instr, "DEC")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// dec reg: 48+r
			asmEmit(0x48 + reg);
		}
	}
	else if(strcmp(instr, "CMP")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			int reg2 = asmParseReg(op2);
			if(reg2 >= 0) {
				// cmp reg, reg2: 39 /r
				asmEmit(0x39);
				asmEmit(0xC0 | (reg2 << 3) | reg);
			} else {
				int val;
				if(asmParseNum(op2, &val)) {
					if(val >= -128 && val <= 127) {
						// cmp reg, imm8: 83 /7 ib
						asmEmit(0x83);
						asmEmit(0xF8 | reg);
						asmEmit((char)val);
					} else {
						// cmp reg, imm32: 81 /7 id
						asmEmit(0x81);
						asmEmit(0xF8 | reg);
						asmEmit32(val);
					}
				}
			}
		}
	}
	else if(strcmp(instr, "TEST")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			int reg2 = asmParseReg(op2);
			if(reg2 >= 0) {
				// test reg, reg2: 85 /r
				asmEmit(0x85);
				asmEmit(0xC0 | (reg2 << 3) | reg);
			} else {
				int val;
				if(asmParseNum(op2, &val)) {
					// test reg, imm32: F7 /0 id
					asmEmit(0xF7);
					asmEmit(0xC0 | reg);
					asmEmit32(val);
				}
			}
		}
	}
	else if(strcmp(instr, "JMP")) {
		// Unconditional jump
		// First try to find if label is already defined (backward jump)
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			// Backward jump - calculate offset
			int offset = label_addr - (asm_output_pos + 2);  // +2 for short jump
			if(offset >= -128 && offset <= 127) {
				// Short jump: EB + rel8
				asmEmit(0xEB);
				asmEmit((char)offset);
			} else {
				// Near jump: E9 + rel32
				offset = label_addr - (asm_output_pos + 5);  // +5 for near jump
				asmEmit(0xE9);
				asmEmit32(offset);
			}
		} else {
			// Forward jump - use near jump and add reference
			asmEmit(0xE9);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);  // Placeholder
		}
	}
	else if(strcmp(instr, "JE") || strcmp(instr, "JZ")) {
		// Jump if equal/zero: 74 rel8 or 0F 84 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x74);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x84);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x84);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JNE") || strcmp(instr, "JNZ")) {
		// Jump if not equal/not zero: 75 rel8 or 0F 85 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x75);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x85);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x85);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JG") || strcmp(instr, "JNLE")) {
		// Jump if greater (signed): 7F rel8 or 0F 8F rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x7F);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x8F);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x8F);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JGE") || strcmp(instr, "JNL")) {
		// Jump if greater or equal (signed): 7D rel8 or 0F 8D rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x7D);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x8D);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x8D);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JL") || strcmp(instr, "JNGE")) {
		// Jump if less (signed): 7C rel8 or 0F 8C rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x7C);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x8C);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x8C);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JLE") || strcmp(instr, "JNG")) {
		// Jump if less or equal (signed): 7E rel8 or 0F 8E rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x7E);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x8E);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x8E);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JA") || strcmp(instr, "JNBE")) {
		// Jump if above (unsigned): 77 rel8 or 0F 87 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x77);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x87);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x87);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JAE") || strcmp(instr, "JNB") || strcmp(instr, "JNC")) {
		// Jump if above or equal (unsigned): 73 rel8 or 0F 83 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x73);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x83);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x83);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JB") || strcmp(instr, "JNAE") || strcmp(instr, "JC")) {
		// Jump if below (unsigned): 72 rel8 or 0F 82 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x72);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x82);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x82);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JBE") || strcmp(instr, "JNA")) {
		// Jump if below or equal (unsigned): 76 rel8 or 0F 86 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x76);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x86);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x86);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JS")) {
		// Jump if sign (negative): 78 rel8 or 0F 88 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x78);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x88);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x88);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "JNS")) {
		// Jump if not sign (positive): 79 rel8 or 0F 89 rel32
		int label_addr = asmFindLabel(op1);
		if(label_addr >= 0) {
			int offset = label_addr - (asm_output_pos + 2);
			if(offset >= -128 && offset <= 127) {
				asmEmit(0x79);
				asmEmit((char)offset);
			} else {
				offset = label_addr - (asm_output_pos + 6);
				asmEmit(0x0F);
				asmEmit(0x89);
				asmEmit32(offset);
			}
		} else {
			asmEmit(0x0F);
			asmEmit(0x89);
			asmAddJmpRef(op1, asm_output_pos, 4);
			asmEmit32(0);
		}
	}
	else if(strcmp(instr, "CALL")) {
		asmToUpper(op1);
		int reg = asmParseReg(op1);
		if(reg >= 0) {
			// call reg: FF /2
			asmEmit(0xFF);
			asmEmit(0xD0 | reg);
		}
	}
	else if(strcmp(instr, "DB")) {
		// Data byte directive - use full_args to handle multiple values
		char* p = full_args;
		while(*p) {
			p = asmSkipWS(p);
			if(*p == '"' || *p == '\'') {
				// String literal
				char quote = *p++;
				while(*p && *p != quote) {
					asmEmit(*p++);
				}
				if(*p == quote) p++;
			} else if(*p >= '0' && *p <= '9') {
				// Parse number (decimal or hex)
				int num_val = 0;
				if(*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
					// Hex number
					p += 2;
					while(1) {
						if(*p >= '0' && *p <= '9') {
							num_val = (num_val << 4) | (*p - '0');
						} else if(*p >= 'A' && *p <= 'F') {
							num_val = (num_val << 4) | (*p - 'A' + 10);
						} else if(*p >= 'a' && *p <= 'f') {
							num_val = (num_val << 4) | (*p - 'a' + 10);
						} else {
							break;
						}
						p++;
					}
				} else {
					// Decimal number
					while(*p >= '0' && *p <= '9') {
						num_val = num_val * 10 + (*p - '0');
						p++;
					}
				}
				asmEmit((unsigned char)num_val);
			} else {
				// Skip unknown character
				p++;
			}
			p = asmSkipWS(p);
			if(*p == ',') p++;
		}
	}
	else if(instr[0] != 0) {
		// Unknown instruction - print debug info
		printString("\n[DEBUG] Unknown: '");
		printString(instr);
		printString("'\n");
		return 0;
	}
	
	return 1;
}

// Main assembler function
// Reads source file, assembles it, writes output binary
int asmAssemble(char* src_file, char* out_file) {
	// Read source file
	static char src_buf[4096];
	int bytes = fs_read_file(src_file, src_buf, 4095);
	if(bytes <= 0) {
		printString("\n[ERROR] Cannot read source file: ");
		printString(src_file);
		printString("\n");
		return 0;
	}
	src_buf[bytes] = 0;
	
	// Initialize assembler state
	asm_output_pos = 0;
	asm_num_labels = 0;
	asm_num_refs = 0;
	asm_num_jmprefs = 0;
	
	// Two-pass assembly
	// Pass 1: Find all labels
	char* p = src_buf;
	while(*p) {
		// Find end of line
		char* line_start = p;
		while(*p && *p != '\n') p++;
		char saved = *p;
		if(*p) *p = 0;
		
		// Check for label
		char* colon = line_start;
		while(*colon && *colon != ':' && *colon != ' ' && *colon != '\t' && *colon != ';') colon++;
		if(*colon == ':') {
			char label[16];
			int i;
			for(i = 0; i < 15 && line_start + i < colon; i++) {
				label[i] = line_start[i];
			}
			label[i] = 0;
			// Don't add yet - we'll add during assembly
		}
		
		if(saved) {
			*p = saved;
			p++;
		}
	}
	
	// Pass 2: Assemble
	p = src_buf;
	int line_num = 1;
	while(*p) {
		char* line_start = p;
		while(*p && *p != '\n') p++;
		char saved = *p;
		if(*p) *p = 0;
		
		if(!asmAssembleLine(line_start)) {
			printString("\n[ERROR] Line ");
			printNumber(line_num);
			printString(": ");
			printString(line_start);
			printString("\n");
			return 0;
		}
		
		if(saved) {
			*p = saved;
			p++;
		}
		line_num++;
	}
	
	// Resolve forward references (programs load at 0x50000)
	asmResolveRefs(0x50000);
	
	// Resolve jump references (relative offsets)
	if(!asmResolveJmpRefs()) {
		return 0;
	}
	
	// Write output file
	if(asm_output_pos > 0) {
		if(fs_write_file(out_file, (char*)asm_output, asm_output_pos)) {
			fs_mark_executable(out_file);
			printString("\n[SUCCESS] Assembled ");
			printNumber(asm_output_pos);
			printString(" bytes to ");
			printString(out_file);
			printString("\n");
			return 1;
		}
	}
	
	printString("\n[ERROR] Failed to write output file\n");
	return 0;
}

// Helper function to extract command and argument
void parseCommand(char* cmd, char* command, char* argument) {
	int i = 0;
	int j = 0;
	
	// Skip leading spaces
	while(cmd[i] == ' ') i++;
	
	// Extract command (first word)
	while(cmd[i] != '\0' && cmd[i] != ' ' && j < 63) {
		command[j++] = cmd[i++];
	}
	command[j] = '\0';
	
	// Skip spaces after command
	while(cmd[i] == ' ') i++;
	
	// Extract everything after command as argument
	j = 0;
	while(cmd[i] != '\0' && j < 191) {
		argument[j++] = cmd[i++];
	}
	argument[j] = '\0';
}

// Helper function to parse WRITE command: WRITE <filename> <data>
// Returns 1 if successful, 0 if error
int parseWriteCommand(char* argument, char* filename, char* data, int* data_len) {
	int i = 0;
	int j = 0;
	
	// Skip leading spaces
	while(argument[i] == ' ') i++;
	
	// Extract filename (first word in argument)
	j = 0;
	while(argument[i] != '\0' && argument[i] != ' ' && j < 63) {
		filename[j++] = argument[i++];
	}
	filename[j] = '\0';
	
	// Check if filename is empty
	if(j == 0) {
		return 0; // No filename provided
	}
	
	// Validate filename length (max 12 characters)
	if(j > 12) {
		return 0; // Filename too long
	}
	
	// Skip spaces after filename
	while(argument[i] == ' ') i++;
	
	// Everything after filename is the data
	if(argument[i] == '\0') {
		// No data provided
		*data_len = 0;
		data[0] = '\0';
		return 1; // Still valid, just empty data
	}
	
	// Copy data
	j = 0;
	int max_data = 2000; // Large buffer for data (can write to multiple sectors)
	while(argument[i] != '\0' && j < max_data - 1) {
		data[j++] = argument[i++];
	}
	data[j] = '\0';
	*data_len = j;
	
	return 1; // Success
}

void strEval(char* CMD){
	char command[64];   // Increased size for longer command names
	char argument[192]; // Increased size for longer arguments (256 - 64 = 192)
	parseCommand(CMD, command, argument);
	normalizeRepeat(command);
	
	// Convert command to uppercase for case-insensitive comparison
	int cmd_idx = 0;
	while(command[cmd_idx] != '\0') {
		if(command[cmd_idx] >= 'a' && command[cmd_idx] <= 'z') {
			command[cmd_idx] = command[cmd_idx] - 'a' + 'A';
		}
		cmd_idx++;
	}
	
	char cmd1[] = "CLS";
	char cmd2[] = "COLORA";
	char cmd3[] = "COLORB";
	char cmd4[] = "COLORC";
	char cmd5[] = "COLORDEF";
	char cmd6[] = "VID";
	char cmd7[] = "HI";
	char cmd8[] = "PUT";
	char cmd9[] = "GET";
	char cmdColor[] = "COLOR";
	char cmdColorBG[] = "COLORBG";
	char cmdColorFG[] = "COLORFG";
	
	// System info commands
	char cmdSysInfo[] = "SYSINFO";
	char cmdCpuInfo[] = "CPUINFO";
	char cmdMemInfo[] = "MEMINFO";
	char cmdUptime[] = "UPTIME";
	char cmdPgInfo[] = "PGINFO";
	char cmdHeap[] = "HEAP";
	
	// File system commands
	char cmd10[] = "CREATE";
	char cmd11[] = "DELETE";
	char cmd12[] = "READ";
	char cmd13[] = "WRITE";
	char cmd14[] = "LIST";
	char cmd15[] = "EXEC";
	char cmd16[] = "SIZE";
	char cmd17[] = "CHMODX";
	char cmd18[] = "SECTOR";
	char cmd19[] = "HELP";
	char cmd20[] = "BEEP";
	char cmd21[] = "MUTE";
	char cmd22[] = "MOUSE";
	char cmd23[] = "INSTALL";
	char cmd24[] = "EDIT";
	char cmd25[] = "ASM";
	char cmd26[] = "NETINFO";
	char cmd27[] = "NETTX";
	char cmd28[] = "NETRX";
	char cmd29[] = "ARPWHO";
	char cmd30[] = "ARPCACHE";
	char cmd31[] = "IPTX";
	char cmd32[] = "UDPBIND";
	char cmd33[] = "UDPSEND";
	char cmd34[] = "UDPRECV";
	char cmd35[] = "UDPSOCK";
	char cmd36[] = "DHCP";
	char cmd37[] = "TCPGET";
	char cmd38[] = "PING";
	char cmd39[] = "TCPCONN";
	char cmd40[] = "TCPSEND";
	char cmd41[] = "TCPRECV";
	char cmd42[] = "TCPCLOSE";
	char cmd43[] = "TCPSTAT";
	char cmd44[] = "DNS";
	char cmd45[] = "HTTPGET";
	char cmd46[] = "VGAMODE";
	char cmd47[] = "DESKTOP";

	char msg1[] = "\nHELLO , Welcome to ERISos\n";

	if(strcmp(command , cmd1))
		cls();

	else if(strcmp(command , cmd2))
		setMonitorColor(0x3c);

	else if(strcmp(command , cmd3))
		setMonitorColor(0x5a);

	else if(strcmp(command , cmd4))
		setMonitorColor(0x2a);

	else if(strcmp(command , cmd5))
		setMonitorColor(0xa5);	

	else if(strcmp(command , cmdColor)) {
		// COLOR command with argument
		if(argument[0] == '\0') {
			// No argument - show available colors
			printString("\nAvailable color schemes:\n");
			printString("  Classic:  MATRIX TERMINAL AMBER DOS SNOW\n");
			printString("  Dark:     MIDNIGHT OCEAN FOREST VOLCANO\n");
			printString("  Light:    PAPER SKY MINT SUNSET\n");
			printString("  High-Contrast: INVERSE BLUEPRINT FIRE ICE\n");
			printString("\nUsage: COLOR <name>\n");
			printString("Legacy: COLORA, COLORB, COLORC, COLORDEF\n");
		}
		// Classic schemes
		else if(strcmp(argument, "MATRIX")) {
			setMonitorColor(0x0a);  // Black bg, Light Green fg
			printString("\nMatrix theme activated.\n");
		}
		else if(strcmp(argument, "TERMINAL")) {
			setMonitorColor(0x0f);  // Black bg, White fg
			printString("\nTerminal theme activated.\n");
		}
		else if(strcmp(argument, "AMBER")) {
			setMonitorColor(0x06);  // Black bg, Brown/Orange fg
			printString("\nAmber theme activated.\n");
		}
		else if(strcmp(argument, "DOS")) {
			setMonitorColor(0x07);  // Black bg, Light Gray fg
			printString("\nDOS theme activated.\n");
		}
		else if(strcmp(argument, "SNOW")) {
			setMonitorColor(0xf0);  // White bg, Black fg
			printString("\nSnow theme activated.\n");
		}
		// Dark schemes
		else if(strcmp(argument, "MIDNIGHT")) {
			setMonitorColor(0x19);  // Blue bg, Light Blue fg
			printString("\nMidnight theme activated.\n");
		}
		else if(strcmp(argument, "OCEAN")) {
			setMonitorColor(0x1b);  // Blue bg, Light Cyan fg
			printString("\nOcean theme activated.\n");
		}
		else if(strcmp(argument, "FOREST")) {
			setMonitorColor(0x2a);  // Green bg, Light Green fg
			printString("\nForest theme activated.\n");
		}
		else if(strcmp(argument, "VOLCANO")) {
			setMonitorColor(0x4e);  // Red bg, Yellow fg
			printString("\nVolcano theme activated.\n");
		}
		// Light schemes
		else if(strcmp(argument, "PAPER")) {
			setMonitorColor(0x70);  // Light Gray bg, Black fg
			printString("\nPaper theme activated.\n");
		}
		else if(strcmp(argument, "SKY")) {
			setMonitorColor(0xb1);  // Light Cyan bg, Blue fg
			printString("\nSky theme activated.\n");
		}
		else if(strcmp(argument, "MINT")) {
			setMonitorColor(0xa0);  // Light Green bg, Black fg
			printString("\nMint theme activated.\n");
		}
		else if(strcmp(argument, "SUNSET")) {
			setMonitorColor(0xe4);  // Yellow bg, Red fg
			printString("\nSunset theme activated.\n");
		}
		// High contrast schemes
		else if(strcmp(argument, "INVERSE")) {
			setMonitorColor(0xf0);  // White bg, Black fg
			printString("\nInverse theme activated.\n");
		}
		else if(strcmp(argument, "BLUEPRINT")) {
			setMonitorColor(0x1f);  // Blue bg, White fg
			printString("\nBlueprint theme activated.\n");
		}
		else if(strcmp(argument, "FIRE")) {
			setMonitorColor(0x4f);  // Red bg, White fg
			printString("\nFire theme activated.\n");
		}
		else if(strcmp(argument, "ICE")) {
			setMonitorColor(0x3f);  // Cyan bg, White fg
			printString("\nIce theme activated.\n");
		}
		else {
			printString("\nUnknown color scheme: ");
			printString(argument);
			printString("\nType COLOR for list of available schemes.\n");
		}
	}

	else if(strcmp(command , cmdColorBG)) {
		// COLORBG - Set background color
		if(argument[0] == '\0') {
			printString("\nSet background color.\n");
			printString("Usage: COLORBG <color>\n");
			printString("Colors: BLACK BLUE GREEN CYAN RED MAGENTA BROWN GRAY\n");
			printString("        DARKGRAY LIGHTBLUE LIGHTGREEN LIGHTCYAN\n");
			printString("        LIGHTRED PINK YELLOW WHITE\n");
		}
		else {
			int color = parseColorName(argument);
			if(color >= 0) {
				// Keep foreground, change background
				unsigned char newColor = (color << 4) | (currentColorAttr & 0x0f);
				setColorAndRemember(newColor);
				printString("\nBackground set to ");
				printString(argument);
				printString(".\n");
			}
			else {
				printString("\nUnknown color: ");
				printString(argument);
				printString("\nType COLORBG for list of colors.\n");
			}
		}
	}

	else if(strcmp(command , cmdColorFG)) {
		// COLORFG - Set foreground color
		if(argument[0] == '\0') {
			printString("\nSet foreground (text) color.\n");
			printString("Usage: COLORFG <color>\n");
			printString("Colors: BLACK BLUE GREEN CYAN RED MAGENTA BROWN GRAY\n");
			printString("        DARKGRAY LIGHTBLUE LIGHTGREEN LIGHTCYAN\n");
			printString("        LIGHTRED PINK YELLOW WHITE\n");
		}
		else {
			int color = parseColorName(argument);
			if(color >= 0) {
				// Keep background, change foreground
				unsigned char newColor = (currentColorAttr & 0xf0) | color;
				setColorAndRemember(newColor);
				printString("\nForeground set to ");
				printString(argument);
				printString(".\n");
			}
			else {
				printString("\nUnknown color: ");
				printString(argument);
				printString("\nType COLORFG for list of colors.\n");
			}
		}
	}
	else if(strcmp(command , cmd46)) { // VGAMODE
		char mode_opt[32];
		int mi = 0;
		int mj = 0;
		while(argument[mi] == ' ') mi++;
		while(argument[mi] != '\0' && argument[mi] != ' ' && mj < 31) {
			mode_opt[mj++] = argument[mi++];
		}
		mode_opt[mj] = '\0';

		if(mode_opt[0] == '\0') {
			printString("\nGraphics mode control.\n");
			printString("Usage: VGAMODE <VESA|TEST|TEXT>\n");
			printString("  VESA - Enter 800x600x32 graphics mode\n");
			printString("  TEST - Enter VESA mode and draw demo pattern\n");
			printString("  TEXT - Return to 80x25 text mode\n");
		}
		else if(startsWithi(mode_opt, "VESA") || strcmp(mode_opt, "13")) {
			if(vgaEnterVesaMode(VESA_DEFAULT_WIDTH, VESA_DEFAULT_HEIGHT, VESA_DEFAULT_BPP)) {
				printString("\nEntered VESA 800x600x32 mode. Press ESC to return.\n");
			} else {
				printString("\nVESA mode unavailable on this hardware/emulator.\n");
			}
		}
		else if(startsWithi(mode_opt, "TEST")) {
			if(vgaEnterVesaMode(VESA_DEFAULT_WIDTH, VESA_DEFAULT_HEIGHT, VESA_DEFAULT_BPP)) {
				printString("\nEntered VESA test mode. Press ESC to return.\n");
				vgaDrawDemoPattern();
			} else {
				printString("\nVESA mode unavailable on this hardware/emulator.\n");
			}
		}
		else if(startsWithi(mode_opt, "TEXT")) {
			vgaEnterTextMode();
			printString("Text mode enabled.\n");
		}
		else {
			printString("\nUnknown VGAMODE option: ");
			printString(mode_opt);
			printString("\nUsage: VGAMODE <VESA|TEST|TEXT>\n");
		}
	}
	else if(strcmp(command , cmd47)) { // DESKTOP
		char desktop_opt[32];
		int di = 0;
		int dj = 0;
		while(argument[di] == ' ') di++;
		while(argument[di] != '\0' && argument[di] != ' ' && dj < 31) {
			desktop_opt[dj++] = argument[di++];
		}
		desktop_opt[dj] = '\0';
		desktopLaunch(desktop_opt);
	}

	else if(strcmp(command , cmdSysInfo)) {
		showSysInfo();
	}

	else if(strcmp(command , cmdCpuInfo)) {
		showCpuInfo();
	}

	else if(strcmp(command , cmdMemInfo)) {
		showMemInfo();
	}

	else if(strcmp(command , cmdUptime)) {
		showUptime();
	}

	else if(strcmp(command , cmdPgInfo)) {
		showPagingInfo();
	}

	else if(strcmp(command , cmdHeap)) {
		showHeapInfo();
	}

	else if(strcmp(command , cmd6))
		vid();
	else if(strcmp(command , cmd7))
		printString(msg1);
	else if(strcmp(command , cmd8)){
		blockAddr = 0;
		int i = 0;
		
		while(i < 511){
			At[i] = 'J'; // Fill with J
			i++;
		}
		At[i] = 0; // Null character

		put(); // Writes to Hard disk
		printString("\nTest data written to sector 0.\n");
		
		i = 0;
		while(i < 511){
			At[i] = 0;  // Clears the content
			i++;
		}
	}		
	else if(strcmp(command , cmd9)){
		blockAddr = 0;
		get();
		printString("\nSector 0 contents:\n");
		printString(At);
		printString("\n");
	}
	// File system commands
	else if(strcmp(command , cmd10)){ // CREATE
		if(argument[0] == '\0') {
			char msg[] = "Usage: CREATE <filename>\n";
			printString(msg);
		} else {
			// Check filename length first
			int len = 0;
			while(argument[len] != '\0' && len < 13) len++;
			if(argument[len] != '\0') {
				char msg[] = "Filename too long (max 12 characters): ";
				printString(msg);
				printString(argument);
				char msg2[] = "\n";
				printString(msg2);
			} else if(fs_create_file(argument, 0)) {
				char msg[] = "\n[SUCCESS] File created: ";
				printString(msg);
				printString(argument);
				printString("\n");
			} else {
				char msg[] = "\n[ERROR] Failed to create file (may already exist or filename invalid): ";
				printString(msg);
				printString(argument);
				printString("\n");
			}
		}
	}
	else if(strcmp(command , cmd11)){ // DELETE
		if(argument[0] == '\0') {
			char msg[] = "Usage: DELETE <filename>\n";
			printString(msg);
		} else {
			if(fs_delete_file(argument)) {
				char msg[] = "\n[SUCCESS] File deleted: ";
				printString(msg);
				printString(argument);
				printString("\n");
			} else {
				char msg[] = "\n[ERROR] Failed to delete file: ";
				printString(msg);
				printString(argument);
				printString("\n");
			}
		}
	}
	else if(strcmp(command , cmd12)){ // READ
		if(argument[0] == '\0') {
			char msg[] = "Usage: READ <filename>\n";
			printString(msg);
		} else {
			char buffer[4096];
			int bytes_read = fs_read_file(argument, buffer, 4096);
			if(bytes_read > 0) {
				buffer[bytes_read] = '\0';
				char header[] = "\n--- Content of ";
				printString(header);
				printString(argument);
				char header2[] = " ---\n";
				printString(header2);
				printString(buffer);
				char footer[] = "\n--- End of file ---\n";
				printString(footer);
			} else {
				char msg[] = "\n[ERROR] Failed to read file: ";
				printString(msg);
				printString(argument);
				char msg2[] = " (file not found or empty)\n";
				printString(msg2);
			}
		}
	}
	else if(strcmp(command , cmd13)){ // WRITE
		if(argument[0] == '\0') {
			char msg[] = "Usage: WRITE <filename> <data>\n";
			printString(msg);
		} else {
			// Parse WRITE command: separate filename and data
			char filename[64];
			char data[2000];  // Large buffer for data
			int data_len = 0;
			
			if(!parseWriteCommand(argument, filename, data, &data_len)) {
				char msg[] = "Error: Invalid filename (max 12 characters)\n";
				printString(msg);
			} else if(data_len == 0) {
				char msg[] = "Warning: No data provided, creating empty file\n";
				printString(msg);
				// Create empty file
				if(fs_write_file(filename, "", 0)) {
					char msg2[] = "Empty file created: ";
					printString(msg2);
					printString(filename);
					printString("\n");
				} else {
					char msg2[] = "Failed to create file: ";
					printString(msg2);
					printString(filename);
					printString("\n");
				}
			} else {
				// Write data to file (data is stored in data sectors)
				if(fs_write_file(filename, data, data_len)) {
					char msg[] = "\n[SUCCESS] Data written to: ";
					printString(msg);
					printString(filename);
					char msg2[] = "\n         Size: ";
					printString(msg2);
					// Print data length
					char len_str[16];
					int pos = 0;
					int temp = data_len;
					if(temp == 0) {
						len_str[pos++] = '0';
					} else {
						while(temp > 0 && pos < 15) {
							len_str[pos++] = '0' + (temp % 10);
							temp /= 10;
						}
						int start = 0, end = pos - 1;
						while(start < end) {
							char t = len_str[start];
							len_str[start] = len_str[end];
							len_str[end] = t;
							start++;
							end--;
						}
					}
					len_str[pos] = '\0';
					printString(len_str);
					char msg3[] = " bytes\n";
					printString(msg3);
				} else {
					char msg[] = "\n[ERROR] Failed to write file: ";
					printString(msg);
					printString(filename);
					printString("\n");
				}
			}
		}
	}
	else if(strcmp(command , cmd14)){ // LIST
		fs_list_files();
	}
	else if(strcmp(command , cmd15)){ // EXEC
		if(argument[0] == '\0') {
			char msg[] = "Usage: EXEC <filename>\n";
			printString(msg);
		} else {
			fs_execute_file(argument);
		}
	}
	else if(strcmp(command , cmd16)){ // SIZE
		if(argument[0] == '\0') {
			char msg[] = "Usage: SIZE <filename>\n";
			printString(msg);
		} else {
			int size = fs_get_file_size(argument);
			if(size >= 0) {
				char msg[] = "\nFile: ";
				printString(msg);
				printString(argument);
				char msg2[] = "\nSize: ";
				printString(msg2);
				// Simple size display
				char size_chars[16];
				int pos = 0;
				if(size == 0) {
					size_chars[pos++] = '0';
				} else {
					int temp = size;
					while(temp > 0 && pos < 15) {
						size_chars[pos++] = '0' + (temp % 10);
						temp /= 10;
					}
					// Reverse
					int start = 0, end = pos - 1;
					while(start < end) {
						char t = size_chars[start];
						size_chars[start] = size_chars[end];
						size_chars[end] = t;
						start++;
						end--;
					}
				}
				size_chars[pos] = '\0';
				printString(size_chars);
				char bytes[] = " bytes\n";
				printString(bytes);
			} else {
				char msg[] = "\n[ERROR] File not found: ";
				printString(msg);
				printString(argument);
				printString("\n");
			}
		}
	}
	else if(strcmp(command , cmd17)){ // CHMODX (mark as executable)
		if(argument[0] == '\0') {
			char msg[] = "Usage: CHMODX <filename>\n";
			printString(msg);
		} else {
			if(fs_mark_executable(argument)) {
				char msg[] = "File marked as executable: ";
				printString(msg);
				printString(argument);
				printString("\n");
			} else {
				char msg[] = "Failed to mark file as executable: ";
				printString(msg);
				printString(argument);
				printString("\n");
			}
		}
	}
	else if(strcmp(command , cmd18)){ // SECTOR (show current sector)
		char msg[] = "\nCurrent sector: ";
		printString(msg);
		
		// Convert sector number to string
		int sector = blockAddr;
		char sector_str[16];
		int pos = 0;
		
		if(sector == 0) {
			sector_str[pos++] = '0';
		} else {
			// Convert to string (reverse order first)
			int temp = sector;
			while(temp > 0 && pos < 15) {
				sector_str[pos++] = '0' + (temp % 10);
				temp /= 10;
			}
			// Reverse the string
			int start = 0, end = pos - 1;
			while(start < end) {
				char t = sector_str[start];
				sector_str[start] = sector_str[end];
				sector_str[end] = t;
				start++;
				end--;
			}
		}
		sector_str[pos] = '\0';
		printString(sector_str);
		
		// Add sector description
		if(sector == 0) {
			char desc[] = " (OS Boot Sector - PROTECTED)";
			printString(desc);
		} else if(sector == 1) {
			char desc[] = " (File Allocation Table)";
			printString(desc);
		} else if(sector == 2) {
			char desc[] = " (Directory Table)";
			printString(desc);
		} else if(sector >= 3) {
			char desc[] = " (Data Sector)";
			printString(desc);
		}
		printString("\n");
	}
	else if(strcmp(command , cmd19)){ // HELP
		// Check if argument is empty
		if(argument[0] == '\0') {
			// List all commands (short literals)
			helpPrint("\nAvailable commands:\n");
			helpPrint("Display   : CLS COLOR COLORBG COLORFG VGAMODE\n");
			helpPrint("Display   : DESKTOP\n");
			helpPrint("Display   : COLORA COLORB COLORC COLORDEF\n");
			helpPrint("System    : SYSINFO CPUINFO MEMINFO PGINFO HEAP\n");
			helpPrint("System    : UPTIME VID HI PUT GET\n");
			helpPrint("File      : CREATE DELETE READ WRITE LIST\n");
			helpPrint("File      : EXEC SIZE CHMODX SECTOR INSTALL\n");
			helpPrint("Network   : NETINFO NETTX NETRX ARPWHO ARPCACHE IPTX DHCP\n");
			helpPrint("Network   : UDPBIND UDPSEND UDPRECV UDPSOCK TCPGET PING\n");
			helpPrint("Network   : TCPCONN TCPSEND TCPRECV TCPCLOSE TCPSTAT DNS HTTPGET\n");
			helpPrint("Audio     : BEEP MUTE \n");
			helpPrint("Edit      : EDIT ASM\n");
			helpPrint("Misalenius: MOUSE HELP\n");
			helpPrint("\nType HELP <command> for details.\n");
		} else {
			// Explain specific command - extract to small buffer and compare
			char help_cmd[16];  // Small buffer for command name
			int i = 0;
			int j = 0;
			// Skip leading spaces
			while(argument[i] == ' ' && argument[i] != '\0') i++;
			// Extract command name and convert to uppercase
			while(argument[i] != '\0' && argument[i] != ' ' && j < 15) {
				if(argument[i] >= 'a' && argument[i] <= 'z') {
					help_cmd[j++] = argument[i++] - 'a' + 'A';
				} else {
					help_cmd[j++] = argument[i++];
				}
			}
			help_cmd[j] = '\0';
			
			// Compare with existing cmd arrays (reuse from function scope)
			printChar('\n');
			if(strcmp(help_cmd, cmd1)) {
				helpPrint("CLS - Clear the text screen and reset cursor.\nUsage: CLS\n");
			}
			else if(strcmp(help_cmd, cmd2)) {
				helpPrint("COLORA - Switch to color scheme A.\nUsage: COLORA\n");
			}
			else if(strcmp(help_cmd, cmd3)) {
				helpPrint("COLORB - Switch to color scheme B.\nUsage: COLORB\n");
			}
			else if(strcmp(help_cmd, cmd4)) {
				helpPrint("COLORC - Switch to color scheme C.\nUsage: COLORC\n");
			}
			else if(strcmp(help_cmd, cmd5)) {
				helpPrint("COLORDEF - Restore the default color scheme.\nUsage: COLORDEF\n");
			}
			else if(strcmp(help_cmd, cmdColor)) {
				helpPrint("COLOR - Change color scheme.\n");
				helpPrint("Usage: COLOR <scheme>\n");
				helpPrint("Schemes: MATRIX TERMINAL AMBER DOS SNOW\n");
				helpPrint("         MIDNIGHT OCEAN FOREST VOLCANO\n");
				helpPrint("         PAPER SKY MINT SUNSET\n");
				helpPrint("         INVERSE BLUEPRINT FIRE ICE\n");
			}
			else if(strcmp(help_cmd, cmdColorBG)) {
				helpPrint("COLORBG - Set background color.\n");
				helpPrint("Usage: COLORBG <color>\n");
				helpPrint("Colors: BLACK BLUE GREEN CYAN RED MAGENTA\n");
				helpPrint("        BROWN GRAY DARKGRAY LIGHTBLUE\n");
				helpPrint("        LIGHTGREEN LIGHTCYAN LIGHTRED PINK\n");
				helpPrint("        YELLOW WHITE\n");
			}
			else if(strcmp(help_cmd, cmdColorFG)) {
				helpPrint("COLORFG - Set foreground (text) color.\n");
				helpPrint("Usage: COLORFG <color>\n");
				helpPrint("Colors: BLACK BLUE GREEN CYAN RED MAGENTA\n");
				helpPrint("        BROWN GRAY DARKGRAY LIGHTBLUE\n");
				helpPrint("        LIGHTGREEN LIGHTCYAN LIGHTRED PINK\n");
				helpPrint("        YELLOW WHITE\n");
			}
			else if(strcmp(help_cmd, cmd46)) {
				helpPrint("VGAMODE - Switch between text and VESA graphics modes.\n");
				helpPrint("Usage: VGAMODE <VESA|TEST|TEXT>\n");
				helpPrint("VESA enters 800x600x32 graphics mode.\n");
				helpPrint("TEST enters VESA mode and draws a demo.\n");
				helpPrint("Press ESC in graphics mode to return to text mode.\n");
			}
			else if(strcmp(help_cmd, cmd47)) {
				helpPrint("DESKTOP - Launch the Win95-style RedOS desktop UI.\n");
				helpPrint("Usage: DESKTOP [START|MYCOMPUTER|RECYCLE|DOCS|NOTEPAD|IE|CONTROL|HELP|SHUTDOWN]\n");
				helpPrint("Requires VESA support in QEMU/Bochs.\n");
				helpPrint("Press ESC to return to the text shell.\n");
			}
			else if(strcmp(help_cmd, cmdSysInfo)) {
				helpPrint("SYSINFO - Display system information.\n");
				helpPrint("Shows OS version, architecture, and hardware.\n");
				helpPrint("Usage: SYSINFO\n");
			}
			else if(strcmp(help_cmd, cmdCpuInfo)) {
				helpPrint("CPUINFO - Display CPU information.\n");
				helpPrint("Shows vendor, brand, family, model, features.\n");
				helpPrint("Uses CPUID instruction for detection.\n");
				helpPrint("Usage: CPUINFO\n");
			}
			else if(strcmp(help_cmd, cmdMemInfo)) {
				helpPrint("MEMINFO - Display memory information.\n");
				helpPrint("Shows memory layout and allocation.\n");
				helpPrint("Usage: MEMINFO\n");
			}
			else if(strcmp(help_cmd, cmdUptime)) {
				helpPrint("UPTIME - Display system uptime.\n");
				helpPrint("Shows how long the system has been running.\n");
				helpPrint("Usage: UPTIME\n");
			}
			else if(strcmp(help_cmd, cmdPgInfo)) {
				helpPrint("PGINFO - Display paging information.\n");
				helpPrint("Shows CR0/CR3 registers, page directory\n");
				helpPrint("address, page table entries, and mapped\n");
				helpPrint("memory regions.\n");
				helpPrint("Usage: PGINFO\n");
			}
			else if(strcmp(help_cmd, cmdHeap)) {
				helpPrint("HEAP - Display heap memory information.\n");
				helpPrint("Shows heap usage, free/used blocks, largest\n");
				helpPrint("free block, and a block list for debugging.\n");
				helpPrint("The heap provides kmalloc()/kfree() for\n");
				helpPrint("dynamic memory allocation.\n");
				helpPrint("Usage: HEAP\n");
			}
			else if(strcmp(help_cmd, cmd6)) {
				helpPrint("VID - Run a continuous video memory test.\nUsage: VID\n");
			}
			else if(strcmp(help_cmd, cmd7)) {
				helpPrint("HI - Print a short welcome message.\nUsage: HI\n");
			}
			else if(strcmp(help_cmd, cmd8)) {
				helpPrint("PUT - Write a test sector to disk.\nUsage: PUT\n");
			}
			else if(strcmp(help_cmd, cmd9)) {
				helpPrint("GET - Read a test sector from disk.\nUsage: GET\n");
			}
			else if(strcmp(help_cmd, cmd10)) {
				helpPrint("CREATE - Create a new file entry in the filesystem.\nUsage: CREATE <filename>\n");
			}
			else if(strcmp(help_cmd, cmd11)) {
				helpPrint("DELETE - Remove a file from the filesystem.\nUsage: DELETE <filename>\n");
			}
			else if(strcmp(help_cmd, cmd12)) {
				helpPrint("READ - Display a file's contents.\nUsage: READ <filename>\n");
			}
			else if(strcmp(help_cmd, cmd13)) {
				helpPrint("WRITE - Save data into a file.\nUsage: WRITE <filename> <data>\n");
			}
			else if(strcmp(help_cmd, cmd14)) {
				helpPrint("LIST - Show all files in the directory.\nUsage: LIST\n");
			}
			else if(strcmp(help_cmd, cmd15)) {
				helpPrint("EXEC - Execute a file marked as executable.\nUsage: EXEC <filename>\n");
			}
			else if(strcmp(help_cmd, cmd16)) {
				helpPrint("SIZE - Display the size of a file in bytes.\nUsage: SIZE <filename>\n");
			}
			else if(strcmp(help_cmd, cmd17)) {
				helpPrint("CHMODX - Mark a file as executable.\nUsage: CHMODX <filename>\n");
			}
			else if(strcmp(help_cmd, cmd18)) {
				helpPrint("SECTOR - Show the current disk sector index.\nUsage: SECTOR\n");
			}
			else if(strcmp(help_cmd, cmd19)) {
				helpPrint("HELP - Show the command list or details.\nUsage: HELP [command]\n");
			}
			else if(strcmp(help_cmd, cmd20)) {
				helpPrint("BEEP - Play a tone on the PC speaker.\nUsage: BEEP [freq] [ms]\n");
			}
			else if(strcmp(help_cmd, cmd21)) {
				helpPrint("MUTE - Turn off the PC speaker.\nUsage: MUTE\n");
			}
			else if(strcmp(help_cmd, cmd22)) {
				helpPrint("MOUSE - Show mouse position and buttons.\nUsage: MOUSE\n");
			}
			else if(strcmp(help_cmd, cmd23)) {
				helpPrint("INSTALL - Install a built-in test program.\n");
				helpPrint("Usage: INSTALL <program>\n");
				helpPrint("Programs: HELLO, BEEP, CLEAR, TEST\n");
				helpPrint("Then use EXEC <filename> to run it.\n");
			}
			else if(strcmp(help_cmd, cmd26)) {
				helpPrint("NETINFO - Show RTL8029/NE2000 NIC status.\n");
				helpPrint("Usage: NETINFO\n");
			}
			else if(strcmp(help_cmd, cmd27)) {
				helpPrint("NETTX - Send a raw Ethernet test frame.\n");
				helpPrint("Usage: NETTX\n");
			}
			else if(strcmp(help_cmd, cmd28)) {
				helpPrint("NETRX - Poll once for a received frame.\n");
				helpPrint("Usage: NETRX\n");
			}
			else if(strcmp(help_cmd, cmd29)) {
				helpPrint("ARPWHO - Send ARP request and resolve IPv4 -> MAC.\n");
				helpPrint("Usage: ARPWHO <IPv4>\n");
				helpPrint("Example: ARPWHO 10.0.2.2\n");
			}
			else if(strcmp(help_cmd, cmd30)) {
				helpPrint("ARPCACHE - Show learned ARP cache entries.\n");
				helpPrint("Usage: ARPCACHE\n");
			}
			else if(strcmp(help_cmd, cmd31)) {
				helpPrint("IPTX - Send a minimal IPv4 test packet.\n");
				helpPrint("Usage: IPTX <IPv4>\n");
				helpPrint("Example: IPTX 10.0.2.2\n");
			}
			else if(strcmp(help_cmd, cmd32)) {
				helpPrint("UDPBIND - Bind a local UDP port in socket table.\n");
				helpPrint("Usage: UDPBIND <port>\n");
				helpPrint("Example: UDPBIND 5000\n");
			}
			else if(strcmp(help_cmd, cmd33)) {
				helpPrint("UDPSEND - Send UDP datagram using a bound source port.\n");
				helpPrint("Usage: UDPSEND <src_port> <dst_ip> <dst_port> <message>\n");
				helpPrint("Example: UDPSEND 5000 10.0.2.2 5001 hello\n");
			}
			else if(strcmp(help_cmd, cmd34)) {
				helpPrint("UDPRECV - Receive one queued UDP datagram on a bound port.\n");
				helpPrint("Usage: UDPRECV <port>\n");
				helpPrint("Example: UDPRECV 5000\n");
			}
			else if(strcmp(help_cmd, cmd35)) {
				helpPrint("UDPSOCK - List currently bound UDP ports.\n");
				helpPrint("Usage: UDPSOCK\n");
			}
			else if(strcmp(help_cmd, cmd36)) {
				helpPrint("DHCP - Request/renew IPv4 configuration via DHCP.\n");
				helpPrint("Usage: DHCP\n");
			}
			else if(strcmp(help_cmd, cmd37)) {
				helpPrint("TCPGET - Minimal TCP client GET request utility.\n");
				helpPrint("Usage: TCPGET <dst_ip> <dst_port> [path]\n");
				helpPrint("Example: TCPGET 93.184.216.34 80 /\n");
			}
			else if(strcmp(help_cmd, cmd38)) {
				helpPrint("PING - Send ICMP echo request and wait for reply.\n");
				helpPrint("Usage: PING <IPv4>\n");
				helpPrint("Example: PING 10.0.2.2\n");
			}
			else if(strcmp(help_cmd, cmd39)) {
				helpPrint("TCPCONN - Open TCP client connection.\n");
				helpPrint("Usage: TCPCONN <dst_ip> <dst_port>\n");
			}
			else if(strcmp(help_cmd, cmd40)) {
				helpPrint("TCPSEND - Send text over established TCP connection.\n");
				helpPrint("Usage: TCPSEND <text>\n");
			}
			else if(strcmp(help_cmd, cmd41)) {
				helpPrint("TCPRECV - Print and clear buffered TCP RX data.\n");
				helpPrint("Usage: TCPRECV\n");
			}
			else if(strcmp(help_cmd, cmd42)) {
				helpPrint("TCPCLOSE - Gracefully close current TCP connection.\n");
				helpPrint("Usage: TCPCLOSE\n");
			}
			else if(strcmp(help_cmd, cmd43)) {
				helpPrint("TCPSTAT - Show TCP connection state and sequence counters.\n");
				helpPrint("Usage: TCPSTAT\n");
			}
			else if(strcmp(help_cmd, cmd44)) {
				helpPrint("DNS - Resolve hostname to IPv4 (A record).\n");
				helpPrint("Usage: DNS <hostname> [dns_server_ip]\n");
				helpPrint("Example: DNS example.com\n");
				helpPrint("Example: DNS example.com 8.8.8.8\n");
			}
			else if(strcmp(help_cmd, cmd45)) {
				helpPrint("HTTPGET - HTTP client GET request by hostname or IP.\n");
				helpPrint("Usage: HTTPGET <host_or_ip> [path] [port]\n");
				helpPrint("Example: HTTPGET example.com /\n");
				helpPrint("Example: HTTPGET 93.184.216.34 / 80\n");
			}
			else if(strcmp(help_cmd, cmd24)) {
				helpPrint("EDIT - Full-screen text editor.\n");
				helpPrint("Usage: EDIT <filename>\n");
				helpPrint("Controls:\n");
				helpPrint("  Arrow keys - Move cursor\n");
				helpPrint("  F2 - Save file\n");
				helpPrint("  ESC - Exit editor\n");
			}
			else if(strcmp(help_cmd, cmd25)) {
				// Check if there's a second argument (instruction name)
				// Skip "ASM" and get the instruction
				int k = 3;  // Start after "ASM"
				while(argument[k] == ' ') k++;
				
				if(argument[k] == '\0') {
					// No instruction specified - show general help
					printString("\nASM - Assemble x86 code to binary.\n");
					printString("Usage: ASM <source.asm> <output.bin>\n\n");
					printString("Instructions:\n");
					printString("  Data:  MOV PUSH POP DB\n");
					printString("  Math:  ADD SUB INC DEC NEG\n");
					printString("  Logic: AND OR XOR NOT CMP TEST\n");
					printString("  Jump:  JMP JE JNE JG JL JA JB\n");
					printString("  Flow:  CALL RET NOP\n\n");
					printString("Type HELP ASM <instr> for details.\n");
				} else {
					// Extract instruction name and convert to uppercase
					char instr[16];
					int m = 0;
					while(argument[k] && argument[k] != ' ' && m < 15) {
						if(argument[k] >= 'a' && argument[k] <= 'z')
							instr[m++] = argument[k++] - 'a' + 'A';
						else
							instr[m++] = argument[k++];
					}
					instr[m] = '\0';
					
					printChar('\n');
					// Show detailed help for specific instruction
					if(strcmp(instr, "MOV")) {
						printString("MOV - Copy a value from one place to another.\n\n");
						printString("  Think of it like: destination = source\n\n");
						printString("  MOV EAX, 42      Put the number 42 into EAX\n");
						printString("  MOV EBX, EAX     Copy EAX's value into EBX\n");
						printString("  MOV EAX, [EBX]   Read memory at address EBX\n");
						printString("  MOV EAX, [EBP+8] Read function argument\n");
					}
					else if(strcmp(instr, "PUSH")) {
						printString("PUSH - Put a value on top of the stack.\n\n");
						printString("  The stack is like a pile of plates.\n");
						printString("  PUSH adds one on top.\n\n");
						printString("  PUSH EAX     Save EAX for later\n");
						printString("  PUSH 42      Put number 42 on stack\n");
						printString("  PUSH msg     Put address of msg on stack\n");
					}
					else if(strcmp(instr, "POP")) {
						printString("POP - Take the top value off the stack.\n\n");
						printString("  Removes the top item and puts it\n");
						printString("  into a register. Opposite of PUSH.\n\n");
						printString("  POP EAX      Get top of stack into EAX\n");
						printString("  POP EBP      Restore saved base pointer\n");
					}
					else if(strcmp(instr, "ADD")) {
						printString("ADD - Add two values together.\n\n");
						printString("  Result is stored in the first operand.\n\n");
						printString("  ADD EAX, 5     EAX becomes EAX + 5\n");
						printString("  ADD ESP, 4     Remove 1 arg from stack\n");
						printString("  ADD ESP, 8     Remove 2 args from stack\n");
					}
					else if(strcmp(instr, "SUB")) {
						printString("SUB - Subtract one value from another.\n\n");
						printString("  Result is stored in the first operand.\n\n");
						printString("  SUB EAX, 1     EAX becomes EAX - 1\n");
						printString("  SUB ESP, 16    Make room on the stack\n");
					}
					else if(strcmp(instr, "INC")) {
						printString("INC - Add 1 to a register.\n\n");
						printString("  Same as ADD reg, 1 but shorter.\n");
						printString("  Great for loop counters.\n\n");
						printString("  INC ECX        ECX becomes ECX + 1\n");
					}
					else if(strcmp(instr, "DEC")) {
						printString("DEC - Subtract 1 from a register.\n\n");
						printString("  Same as SUB reg, 1 but shorter.\n");
						printString("  Great for countdown loops.\n\n");
						printString("  DEC ECX        ECX becomes ECX - 1\n");
					}
					else if(strcmp(instr, "NEG")) {
						printString("NEG - Flip the sign of a number.\n\n");
						printString("  Positive becomes negative, and\n");
						printString("  negative becomes positive.\n\n");
						printString("  NEG EAX    If EAX was 5, now it's -5\n");
					}
					else if(strcmp(instr, "AND")) {
						printString("AND - Keep only bits that are 1 in both.\n\n");
						printString("  Useful for masking or clearing bits.\n\n");
						printString("  AND EAX, 0xFF  Keep only the low byte\n");
						printString("  AND EAX, EBX   Keep bits set in both\n");
					}
					else if(strcmp(instr, "OR")) {
						printString("OR - Set bits that are 1 in either value.\n\n");
						printString("  Useful for turning on specific bits.\n\n");
						printString("  OR EAX, 0x20   Turn on bit 5\n");
						printString("  OR EAX, EBX    Combine bits from both\n");
					}
					else if(strcmp(instr, "XOR")) {
						printString("XOR - Flip bits that differ between values.\n\n");
						printString("  XOR a value with itself to get zero!\n");
						printString("  This is the fastest way to zero a reg.\n\n");
						printString("  XOR EAX, EAX   Set EAX to 0\n");
					}
					else if(strcmp(instr, "NOT")) {
						printString("NOT - Flip every bit in a register.\n\n");
						printString("  Every 0 becomes 1, every 1 becomes 0.\n\n");
						printString("  NOT EAX     Invert all bits of EAX\n");
					}
					else if(strcmp(instr, "CMP")) {
						printString("CMP - Compare two values.\n\n");
						printString("  Like subtraction, but doesn't change\n");
						printString("  anything. Just sets flags for jumps.\n");
						printString("  Always use CMP before a jump!\n\n");
						printString("  CMP EAX, 10    Is EAX equal to 10?\n");
						printString("  JE yes          Jump if it was equal\n");
					}
					else if(strcmp(instr, "TEST")) {
						printString("TEST - Check if specific bits are set.\n\n");
						printString("  Like AND, but doesn't change anything.\n");
						printString("  Just sets flags.\n\n");
						printString("  TEST EAX, EAX  Is EAX zero?\n");
						printString("  JZ is_zero      Jump if it was zero\n");
					}
					else if(strcmp(instr, "JMP")) {
						printString("JMP - Jump to a label (always).\n\n");
						printString("  Execution continues at the label.\n");
						printString("  No condition needed, always jumps.\n\n");
						printString("  JMP loop_start  Go to loop_start\n");
					}
					else if(strcmp(instr, "JE") || strcmp(instr, "JZ")) {
						printString("JE / JZ - Jump if values were EQUAL.\n\n");
						printString("  Use after CMP. Jumps when A == B.\n\n");
						printString("  CMP EAX, 5     Compare EAX with 5\n");
						printString("  JE found        Jump if EAX was 5\n");
					}
					else if(strcmp(instr, "JNE") || strcmp(instr, "JNZ")) {
						printString("JNE / JNZ - Jump if NOT equal.\n\n");
						printString("  Use after CMP. Jumps when A != B.\n");
						printString("  Perfect for loops!\n\n");
						printString("  DEC ECX        Count down\n");
						printString("  JNZ loop        Repeat if ECX != 0\n");
					}
					else if(strcmp(instr, "JG")) {
						printString("JG - Jump if Greater (signed numbers).\n\n");
						printString("  Use after CMP. Jumps when A > B.\n\n");
						printString("  CMP EAX, 10    Compare EAX with 10\n");
						printString("  JG big          Jump if EAX > 10\n");
					}
					else if(strcmp(instr, "JGE")) {
						printString("JGE - Jump if Greater or Equal (signed).\n\n");
						printString("  Use after CMP. Jumps when A >= B.\n\n");
						printString("  CMP EAX, 0     Compare EAX with 0\n");
						printString("  JGE ok          Jump if EAX >= 0\n");
					}
					else if(strcmp(instr, "JL")) {
						printString("JL - Jump if Less (signed numbers).\n\n");
						printString("  Use after CMP. Jumps when A < B.\n\n");
						printString("  CMP EAX, 0     Compare EAX with 0\n");
						printString("  JL negative     Jump if EAX < 0\n");
					}
					else if(strcmp(instr, "JLE")) {
						printString("JLE - Jump if Less or Equal (signed).\n\n");
						printString("  Use after CMP. Jumps when A <= B.\n\n");
						printString("  CMP EAX, 10    Compare EAX with 10\n");
						printString("  JLE ok          Jump if EAX <= 10\n");
					}
					else if(strcmp(instr, "JA")) {
						printString("JA - Jump if Above (unsigned numbers).\n\n");
						printString("  Like JG but for positive-only values.\n\n");
						printString("  CMP EAX, 255\n");
						printString("  JA too_big      Jump if EAX > 255\n");
					}
					else if(strcmp(instr, "JAE")) {
						printString("JAE - Jump if Above or Equal (unsigned).\n\n");
						printString("  Like JGE but for positive-only values.\n\n");
						printString("  CMP EAX, 32\n");
						printString("  JAE ok          Jump if EAX >= 32\n");
					}
					else if(strcmp(instr, "JB")) {
						printString("JB - Jump if Below (unsigned numbers).\n\n");
						printString("  Like JL but for positive-only values.\n\n");
						printString("  CMP EAX, 10\n");
						printString("  JB small        Jump if EAX < 10\n");
					}
					else if(strcmp(instr, "JBE")) {
						printString("JBE - Jump if Below or Equal (unsigned).\n\n");
						printString("  Like JLE but for positive-only values.\n\n");
						printString("  CMP EAX, 100\n");
						printString("  JBE ok          Jump if EAX <= 100\n");
					}
					else if(strcmp(instr, "JS")) {
						printString("JS - Jump if result was Negative.\n\n");
						printString("  Checks the sign flag after math.\n\n");
						printString("  SUB EAX, EBX   Subtract\n");
						printString("  JS went_neg     Jump if result < 0\n");
					}
					else if(strcmp(instr, "JNS")) {
						printString("JNS - Jump if result was NOT negative.\n\n");
						printString("  Opposite of JS.\n\n");
						printString("  DEC EAX        Subtract 1\n");
						printString("  JNS still_ok    Jump if EAX >= 0\n");
					}
					else if(strcmp(instr, "CALL")) {
						printString("CALL - Call a function.\n\n");
						printString("  Saves where you are, then jumps to\n");
						printString("  the function. RET brings you back.\n\n");
						printString("  PUSH arg        Put argument on stack\n");
						printString("  CALL EBX        Call function in EBX\n");
						printString("  ADD ESP, 4      Clean up after call\n");
					}
					else if(strcmp(instr, "RET")) {
						printString("RET - Return from a function.\n\n");
						printString("  Goes back to whoever called you.\n");
						printString("  Put your return value in EAX first.\n\n");
						printString("  XOR EAX, EAX   Return value = 0\n");
						printString("  POP EBP        Restore old frame\n");
						printString("  RET            Go back to caller\n");
					}
					else if(strcmp(instr, "NOP")) {
						printString("NOP - Do absolutely nothing.\n\n");
						printString("  Takes up 1 byte, wastes 1 cycle.\n");
						printString("  Used for padding or placeholders.\n\n");
						printString("  NOP\n");
					}
					else if(strcmp(instr, "DB")) {
						printString("DB - Put raw data bytes in your program.\n\n");
						printString("  Use for strings and constant data.\n");
						printString("  Strings end with 0 (null terminator).\n\n");
						printString("  msg: DB \"Hello!\", 10, 0\n");
						printString("  (10 = newline, 0 = end of string)\n");
					}
					else {
						printString("Unknown instruction: ");
						printString(instr);
						printString("\n\nType HELP ASM for the full list.\n");
					}
				}
			}
			else {
				helpPrint("Unknown command. Type HELP for list.\n");
			}
			printChar('\n');
		}
	}
	else if(strcmp(command , cmd20)){ // BEEP
		unsigned int freq = 440;
		unsigned int ms = 200;
		// Parse frequency from argument if provided
		if(argument[0] >= '0' && argument[0] <= '9') {
			freq = 0;
			int i = 0;
			while(argument[i] >= '0' && argument[i] <= '9') {
				freq = freq * 10 + (argument[i] - '0');
				i++;
			}
			// Skip spaces and parse duration
			while(argument[i] == ' ') i++;
			if(argument[i] >= '0' && argument[i] <= '9') {
				ms = 0;
				while(argument[i] >= '0' && argument[i] <= '9') {
					ms = ms * 10 + (argument[i] - '0');
					i++;
				}
			}
		}
		if(freq < 20) freq = 20;
		if(freq > 20000) freq = 20000;
		if(ms > 5000) ms = 5000;
		printString("\nPlaying tone...\n");
		beep(freq, ms);
	}
	else if(strcmp(command , cmd21)){ // MUTE
		speakerOff();
		printString("\nMuted.\n");
	}
	else if(strcmp(command , cmd22)){ // MOUSE
		printString("\nMouse X: ");
		// Print X coordinate
		char num[8];
		int pos = 0;
		int val = mouse_x;
		if(val == 0) {
			num[pos++] = '0';
		} else {
			while(val > 0 && pos < 7) {
				num[pos++] = '0' + (val % 10);
				val /= 10;
			}
			// Reverse
			int s = 0, e = pos - 1;
			while(s < e) {
				char t = num[s]; num[s] = num[e]; num[e] = t;
				s++; e--;
			}
		}
		num[pos] = '\0';
		printString(num);
		
		printString("  Y: ");
		pos = 0;
		val = mouse_y;
		if(val == 0) {
			num[pos++] = '0';
		} else {
			while(val > 0 && pos < 7) {
				num[pos++] = '0' + (val % 10);
				val /= 10;
			}
			int s = 0, e = pos - 1;
			while(s < e) {
				char t = num[s]; num[s] = num[e]; num[e] = t;
				s++; e--;
			}
		}
		num[pos] = '\0';
		printString(num);
		
		printString("  Buttons: ");
		if(mouse_buttons & 1) printString("L ");
		if(mouse_buttons & 2) printString("R ");
		if(mouse_buttons & 4) printString("M ");
		if(!(mouse_buttons & 7)) printString("none");
		printString("\n");
	}
	else if(strcmp(command , cmd26)){ // NETINFO
		showNetInfo();
	}
	else if(strcmp(command , cmd27)){ // NETTX
		sendNetTestFrame();
	}
	else if(strcmp(command , cmd28)){ // NETRX
		pollNetRx();
	}
	else if(strcmp(command , cmd29)){ // ARPWHO
		arpWho(argument);
	}
	else if(strcmp(command , cmd30)){ // ARPCACHE
		arpCacheShow();
	}
	else if(strcmp(command , cmd31)){ // IPTX
		ipTx(argument);
	}
	else if(strcmp(command , cmd32)){ // UDPBIND
		udpBindCmd(argument);
	}
	else if(strcmp(command , cmd33)){ // UDPSEND
		udpSendCmd(argument);
	}
	else if(strcmp(command , cmd34)){ // UDPRECV
		udpRecvCmd(argument);
	}
	else if(strcmp(command , cmd35)){ // UDPSOCK
		udpSocketsShow();
	}
	else if(strcmp(command , cmd36)){ // DHCP
		dhcpCmd();
	}
	else if(strcmp(command , cmd37)){ // TCPGET
		tcpGetCmd(argument);
	}
	else if(strcmp(command , cmd38)){ // PING
		pingCmd(argument);
	}
	else if(strcmp(command , cmd39)){ // TCPCONN
		tcpConnCmd(argument);
	}
	else if(strcmp(command , cmd40)){ // TCPSEND
		tcpSendCmd(argument);
	}
	else if(strcmp(command , cmd41)){ // TCPRECV
		tcpRecvCmd();
	}
	else if(strcmp(command , cmd42)){ // TCPCLOSE
		tcpCloseCmd();
	}
	else if(strcmp(command , cmd43)){ // TCPSTAT
		tcpStatCmd();
	}
	else if(strcmp(command , cmd44)){ // DNS
		dnsCmd(argument);
	}
	else if(strcmp(command , cmd45)){ // HTTPGET
		httpGetCmd(argument);
	}
	else if(strcmp(command , cmd23)){ // INSTALL
		char c = argument[0];
		if(c >= 'a') c -= 32;
		printChar('\n');
		
		if(c == 'H') {
			char fn[] = {'H','E','L','L','O','.','B','I','N',0};
			char pg[27];
			pg[0]=0x55;pg[1]=0x89;pg[2]=0xE5;pg[3]=0x8B;pg[4]=0x45;pg[5]=0x08;
			pg[6]=0x8B;pg[7]=0x18;pg[8]=0x68;pg[9]=0x16;pg[10]=0x00;pg[11]=0x05;
			pg[12]=0x00;pg[13]=0xFF;pg[14]=0xD3;pg[15]=0x83;pg[16]=0xC4;pg[17]=0x04;
			pg[18]=0x31;pg[19]=0xC0;pg[20]=0x5D;pg[21]=0xC3;
			pg[22]='H';pg[23]='i';pg[24]='!';pg[25]='\n';pg[26]=0;
			fs_write_file(fn,pg,27);
			fs_mark_executable(fn);
			printString("HELLO.BIN\n");
		}
		else if(c == 'C') {
			char fn[] = {'C','L','E','A','R','.','B','I','N',0};
			char pg[17];
			pg[0]=0x55;pg[1]=0x89;pg[2]=0xE5;pg[3]=0x8B;pg[4]=0x45;pg[5]=0x08;
			pg[6]=0x89;pg[7]=0xC6;pg[8]=0x8B;pg[9]=0x46;pg[10]=0x10;
			pg[11]=0xFF;pg[12]=0xD0;pg[13]=0x31;pg[14]=0xC0;pg[15]=0x5D;pg[16]=0xC3;
			fs_write_file(fn,pg,17);
			fs_mark_executable(fn);
			printString("CLEAR.BIN\n");
		}
		else if(c == 'B') {
			char fn[] = {'B','E','E','P','.','B','I','N',0};
			char pg[30];
			pg[0]=0x55;pg[1]=0x89;pg[2]=0xE5;pg[3]=0x8B;pg[4]=0x45;pg[5]=0x08;
			pg[6]=0x89;pg[7]=0xC6;pg[8]=0x8B;pg[9]=0x46;pg[10]=0x14;
			pg[11]=0x68;pg[12]=0xC8;pg[13]=0x00;pg[14]=0x00;pg[15]=0x00;
			pg[16]=0x68;pg[17]=0xB8;pg[18]=0x01;pg[19]=0x00;pg[20]=0x00;
			pg[21]=0xFF;pg[22]=0xD0;pg[23]=0x83;pg[24]=0xC4;pg[25]=0x08;
			pg[26]=0x31;pg[27]=0xC0;pg[28]=0x5D;pg[29]=0xC3;
			fs_write_file(fn,pg,30);
			fs_mark_executable(fn);
			printString("BEEP.BIN\n");
		}
		else {
			char fn[] = {'T','E','S','T','.','B','I','N',0};
			char pg[10];
			pg[0]=0x55;pg[1]=0x89;pg[2]=0xE5;pg[3]=0xB8;pg[4]=0x2A;
			pg[5]=0x00;pg[6]=0x00;pg[7]=0x00;pg[8]=0x5D;pg[9]=0xC3;
			fs_write_file(fn,pg,10);
			fs_mark_executable(fn);
			printString("TEST.BIN\n");
		}
	}
	else if(strcmp(command, cmd24)){ // EDIT
		if(argument[0] == '\0') {
			printString("\nUsage: EDIT <filename>\n");
		} else {
			editorStart(argument);
		}
	}
	else if(strcmp(command, cmd25)){ // ASM
		// Parse source and output filenames from argument
		char src_file[16];
		char out_file[16];
		int i = 0, j = 0;
		
		// Skip spaces
		while(argument[i] == ' ') i++;
		
		// Get source filename
		while(argument[i] && argument[i] != ' ' && j < 15) {
			src_file[j++] = argument[i++];
		}
		src_file[j] = 0;
		
		// Skip spaces
		while(argument[i] == ' ') i++;
		
		// Get output filename
		j = 0;
		while(argument[i] && argument[i] != ' ' && j < 15) {
			out_file[j++] = argument[i++];
		}
		out_file[j] = 0;
		
		if(src_file[0] == 0 || out_file[0] == 0) {
			printString("\nUsage: ASM <source.asm> <output.bin>\n");
			printString("Example: ASM HELLO.ASM HELLO.BIN\n");
		} else {
			asmAssemble(src_file, out_file);
		}
	}
	else {
		char msg[] = "Unknown command. Type HELP for available commands.\n";
		printString(msg);
	}
		
}

void vid(){
	char clr = 'A';
	while(1){
		int i = 0;
		while(i < 2 * 80 * 25){
			*(TM_START + i) = clr;
			clr++;
			i++;
		}
	}
}

void put(){
	write();
}

void get(){
	read();
}
