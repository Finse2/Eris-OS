#ifndef FILESYSTEM_H
#define FILESYSTEM_H

// File System Layout:
// Sector 0: OS Boot Sector (PROTECTED - contains the OS, cannot be written to)
// Sector 1: File Allocation Table (FAT) - tracks free/used sectors
// Sector 2: Directory Table - stores file metadata
// Sectors 3+: Data sectors for files

#define FS_FAT_SECTOR 1
#define FS_DIR_SECTOR 2
#define FS_DATA_START 3
#define FS_SECTOR_SIZE 512
#define FS_MAX_FILES 16  // 16 files * 32 bytes = 512 bytes (1 sector)
#define FS_MAX_FILENAME 12  // Reduced to fit in 16-byte entry structure
#define FS_FAT_ENTRIES 128  // 512 bytes / 4 bytes per entry

// File entry flags
#define FS_FILE_FREE 0x00
#define FS_FILE_USED 0x01
#define FS_FILE_EXECUTABLE 0x02

// FAT entry values
#define FS_FAT_FREE 0x00000000
#define FS_FAT_END 0xFFFFFFFF
#define FS_FAT_BAD 0xFFFFFFFE

// Execution settingsv
#define EXEC_LOAD_ADDR 0x50000   // Load programs at 320KB mark
#define EXEC_MAX_SIZE  0x10000   // Max 64KB program size

// Kernel API structure - passed to programs so they can call kernel functions
// Programs receive a pointer to this structure as their first argument
struct KernelAPI {
    void (*print_string)(char*);           // Print a null-terminated string
    void (*print_char)(char);              // Print a single character
    void (*print_number)(unsigned int);    // Print a decimal number
    void (*print_hex)(unsigned int);       // Print a hex number (with 0x prefix)
    void (*cls)(void);                     // Clear the screen
    void (*beep)(unsigned int, unsigned int); // Play a tone (freq, duration_ms)
    void (*set_color)(char);               // Set screen color attribute
    int (*read_file)(char*, char*, unsigned int); // Read a file into buffer
    int (*write_file)(char*, char*, unsigned int); // Write buffer to file
    int (*get_file_size)(char*);           // Get size of a file
    void* (*kmalloc)(unsigned int);        // Allocate heap memory
    void (*kfree)(void*);                  // Free heap memory
};

// Function pointer type for program entry point
// Programs should be: int program_main(struct KernelAPI* api)
typedef int (*ProgramEntryFunc)(struct KernelAPI*);

// Directory entry structure (32 bytes per entry)
// Total: 16 files * 32 bytes = 512 bytes (1 sector)
struct FileEntry {
    char name[FS_MAX_FILENAME];  // 12 bytes
    unsigned int size;            // 4 bytes
    unsigned int start_sector;    // 4 bytes
    unsigned char flags;          // 1 byte
    unsigned char reserved[11];   // 11 bytes padding to make 32 bytes total
};

// File system functions
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

// Internal functions
int fs_find_free_sector();
int fs_allocate_sectors(unsigned int num_sectors, unsigned int* sectors);
void fs_free_sectors(unsigned int start_sector);
unsigned int fs_calculate_sectors_needed(unsigned int size);

#endif
