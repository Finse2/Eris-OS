#include "filesystem.h"

// Forward declarations for functions we need
extern void read();
extern void write();
extern void printString(char*);
extern void printChar(char);
extern int blockAddr;
extern char At[1024];

// Forward declaration for strcmp (defined in extra.h)
char strcmp(char* sou, char* dest);

// File Allocation Table buffer
unsigned int fat_buffer[FS_FAT_ENTRIES];

// Directory Table buffer
struct FileEntry dir_buffer[FS_MAX_FILES];

// Temporary buffer for sector operations
char sector_buffer[FS_SECTOR_SIZE];

// Helper function to safely save directory to disk
static void fs_save_directory() {
    blockAddr = FS_DIR_SECTOR;
    // Clear the At buffer first to ensure clean write
    int k;
    for(k = 0; k < FS_SECTOR_SIZE; k++) {
        At[k] = 0;
    }
    // Copy directory entries to At buffer (16 entries * 32 bytes = 512 bytes)
    int i;
    for(i = 0; i < FS_MAX_FILES; i++) {
        struct FileEntry* dst = (struct FileEntry*)(At + i * sizeof(struct FileEntry));
        *dst = dir_buffer[i];
    }
    write();
}

// Initialize file system - load FAT and Directory Table
void fs_init() {
    // Initialize FAT buffer to all zeros
    int i;
    for(i = 0; i < FS_FAT_ENTRIES; i++) {
        fat_buffer[i] = FS_FAT_FREE;
    }
    
    // Load FAT from sector 1
    blockAddr = FS_FAT_SECTOR;
    read();
    // Copy FAT data
    for(i = 0; i < FS_FAT_ENTRIES; i++) {
        unsigned int value = *((unsigned int*)(At + i * 4));
        // Only copy if it's a valid FAT value
        if(value == FS_FAT_FREE || value == FS_FAT_END || value == FS_FAT_BAD || 
           (value >= FS_DATA_START && value < FS_DATA_START + FS_FAT_ENTRIES)) {
            fat_buffer[i] = value;
        } else {
            fat_buffer[i] = FS_FAT_FREE;
        }
    }
    
    // Initialize directory buffer to all zeros
    for(i = 0; i < FS_MAX_FILES; i++) {
        int j;
        for(j = 0; j < FS_MAX_FILENAME; j++) {
            dir_buffer[i].name[j] = '\0';
        }
        dir_buffer[i].size = 0;
        dir_buffer[i].start_sector = 0;
        dir_buffer[i].flags = FS_FILE_FREE;
    }
    
    // Load Directory Table from sector 2
    blockAddr = FS_DIR_SECTOR;
    read();
    
    // Check if directory table is uninitialized (all zeros or garbage)
    int is_initialized = 0;
    for(i = 0; i < FS_MAX_FILES; i++) {
        struct FileEntry* src = (struct FileEntry*)(At + i * sizeof(struct FileEntry));
        if(src->flags & FS_FILE_USED) {
            // Check if it has a valid name
            int j;
            for(j = 0; j < FS_MAX_FILENAME; j++) {
                if(src->name[j] != '\0' && src->name[j] >= 32 && src->name[j] < 127) {
                    is_initialized = 1;
                    break;
                }
            }
            if(is_initialized) break;
        }
    }
    
    // Copy directory entries, but validate them
    for(i = 0; i < FS_MAX_FILES; i++) {
        struct FileEntry* src = (struct FileEntry*)(At + i * sizeof(struct FileEntry));
        
        // Only copy if it's a valid entry (has FS_FILE_USED flag and non-empty name)
        if(src->flags & FS_FILE_USED && is_initialized) {
            // Validate the entry has a proper name (not all zeros or garbage)
            int has_valid_name = 0;
            int j;
            for(j = 0; j < FS_MAX_FILENAME; j++) {
                if(src->name[j] != '\0' && src->name[j] >= 32 && src->name[j] < 127) {
                    has_valid_name = 1;
                    break;
                }
            }
            
            if(has_valid_name) {
                // Copy valid entry
                for(j = 0; j < FS_MAX_FILENAME; j++) {
                    dir_buffer[i].name[j] = src->name[j];
                }
                dir_buffer[i].size = src->size;
                dir_buffer[i].start_sector = src->start_sector;
                dir_buffer[i].flags = src->flags;
            } else {
                // Invalid entry, mark as free
                dir_buffer[i].flags = FS_FILE_FREE;
            }
        } else {
            // Not used or uninitialized, mark as free
            dir_buffer[i].flags = FS_FILE_FREE;
        }
    }
    
    // If directory was uninitialized, write the clean version back to disk
    if(!is_initialized) {
        fs_save_directory();
    }
}

// Find a free sector in the FAT
int fs_find_free_sector() {
    int i;
    for(i = 0; i < FS_FAT_ENTRIES; i++) {
        if(fat_buffer[i] == FS_FAT_FREE) {
            return FS_DATA_START + i;
        }
    }
    return -1; // No free sectors
}

// Calculate number of sectors needed for a given size
unsigned int fs_calculate_sectors_needed(unsigned int size) {
    if(size == 0) return 1;
    return (size + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
}

// Allocate sectors for a file
int fs_allocate_sectors(unsigned int num_sectors, unsigned int* sectors) {
    int allocated = 0;
    int i;
    
    for(i = 0; i < FS_FAT_ENTRIES && allocated < num_sectors; i++) {
        if(fat_buffer[i] == FS_FAT_FREE) {
            sectors[allocated] = FS_DATA_START + i;
            allocated++;
        }
    }
    
    if(allocated < num_sectors) {
        // Not enough free sectors - free what we allocated
        int j;
        for(j = 0; j < allocated; j++) {
            int fat_index = sectors[j] - FS_DATA_START;
            fat_buffer[fat_index] = FS_FAT_FREE;
        }
        return 0; // Failure
    }
    
    // Link sectors in FAT
    for(i = 0; i < allocated - 1; i++) {
        int fat_index = sectors[i] - FS_DATA_START;
        fat_buffer[fat_index] = sectors[i + 1]; // Point to next sector
    }
    // Mark last sector as end
    int last_fat_index = sectors[allocated - 1] - FS_DATA_START;
    fat_buffer[last_fat_index] = FS_FAT_END;
    
    // Save FAT to disk
    blockAddr = FS_FAT_SECTOR;
    for(i = 0; i < FS_FAT_ENTRIES; i++) {
        *((unsigned int*)(At + i * 4)) = fat_buffer[i];
    }
    write();
    
    return 1; // Success
}

// Free sectors starting from a given sector
void fs_free_sectors(unsigned int start_sector) {
    unsigned int current = start_sector;
    
    while(current != FS_FAT_END && current >= FS_DATA_START) {
        int fat_index = current - FS_DATA_START;
        if(fat_index >= 0 && fat_index < FS_FAT_ENTRIES) {
            unsigned int next = fat_buffer[fat_index];
            fat_buffer[fat_index] = FS_FAT_FREE;
            current = next;
        } else {
            break;
        }
    }
    
    // Save FAT to disk
    blockAddr = FS_FAT_SECTOR;
    int i;
    for(i = 0; i < FS_FAT_ENTRIES; i++) {
        *((unsigned int*)(At + i * 4)) = fat_buffer[i];
    }
    write();
}

// Find a file in the directory
int fs_find_file(char* filename) {
    int i;
    for(i = 0; i < FS_MAX_FILES; i++) {
        if(dir_buffer[i].flags & FS_FILE_USED) {
            if(strcmp(dir_buffer[i].name, filename)) {
                return i;
            }
        }
    }
    return -1; // File not found
}

// Create a new file
int fs_create_file(char* filename, unsigned char flags) {
    // Validate filename length first
    int filename_len = 0;
    while(filename[filename_len] != '\0' && filename_len < FS_MAX_FILENAME) {
        filename_len++;
    }
    
    // Check if filename is too long
    if(filename[filename_len] != '\0') {
        // Filename is longer than allowed
        return 0; // Filename too long
    }
    
    // Check if filename is empty
    if(filename_len == 0) {
        return 0; // Empty filename
    }
    
    // Check if file already exists
    if(fs_find_file(filename) >= 0) {
        return 0; // File already exists
    }
    
    // Find free directory entry
    int i;
    int dir_index = -1;
    for(i = 0; i < FS_MAX_FILES; i++) {
        if(!(dir_buffer[i].flags & FS_FILE_USED)) {
            dir_index = i;
            break;
        }
    }
    
    if(dir_index < 0) {
        return 0; // No free directory entries
    }
    
    // Initialize file entry - copy filename with strict bounds checking
    // We already validated the length, so this should be safe
    int j;
    for(j = 0; j < filename_len && j < FS_MAX_FILENAME; j++) {
        dir_buffer[dir_index].name[j] = filename[j];
    }
    // Ensure null termination - fill rest with zeros for safety
    for(; j < FS_MAX_FILENAME; j++) {
        dir_buffer[dir_index].name[j] = '\0';
    }
    dir_buffer[dir_index].size = 0;
    dir_buffer[dir_index].start_sector = 0;
    dir_buffer[dir_index].flags = FS_FILE_USED | flags;
    
    // Save directory to disk
    fs_save_directory();
    
    return 1; // Success
}

// Delete a file
int fs_delete_file(char* filename) {
    int dir_index = fs_find_file(filename);
    if(dir_index < 0) {
        return 0; // File not found
    }
    
    // Free sectors used by file
    if(dir_buffer[dir_index].start_sector != 0) {
        fs_free_sectors(dir_buffer[dir_index].start_sector);
    }
    
    // Clear directory entry
    dir_buffer[dir_index].flags = FS_FILE_FREE;
    dir_buffer[dir_index].name[0] = '\0';
    dir_buffer[dir_index].size = 0;
    dir_buffer[dir_index].start_sector = 0;
    
    // Save directory to disk
    fs_save_directory();
    
    return 1; // Success
}

// Read a file
int fs_read_file(char* filename, char* buffer, unsigned int max_size) {
    int dir_index = fs_find_file(filename);
    if(dir_index < 0) {
        return 0; // File not found
    }
    
    struct FileEntry* file = &dir_buffer[dir_index];
    if(file->start_sector == 0) {
        return 0; // File is empty
    }
    
    unsigned int bytes_read = 0;
    unsigned int current_sector = file->start_sector;
    unsigned int size_to_read = file->size;
    if(size_to_read > max_size) {
        size_to_read = max_size;
    }
    
    while(current_sector != FS_FAT_END && bytes_read < size_to_read && current_sector >= FS_DATA_START) {
        blockAddr = current_sector;
        read();
        
        unsigned int bytes_in_sector = FS_SECTOR_SIZE;
        if(bytes_read + bytes_in_sector > size_to_read) {
            bytes_in_sector = size_to_read - bytes_read;
        }
        
        int i;
        for(i = 0; i < bytes_in_sector; i++) {
            buffer[bytes_read + i] = At[i];
        }
        bytes_read += bytes_in_sector;
        
        // Get next sector from FAT
        int fat_index = current_sector - FS_DATA_START;
        if(fat_index >= 0 && fat_index < FS_FAT_ENTRIES) {
            current_sector = fat_buffer[fat_index];
        } else {
            break;
        }
    }
    
    return bytes_read;
}

// Write data to a file
int fs_write_file(char* filename, char* data, unsigned int size) {
    int dir_index = fs_find_file(filename);
    if(dir_index < 0) {
        // File doesn't exist, create it
        if(!fs_create_file(filename, 0)) {
            return 0; // Failed to create file
        }
        dir_index = fs_find_file(filename);
        if(dir_index < 0) {
            return 0; // Still not found
        }
    }
    
    struct FileEntry* file = &dir_buffer[dir_index];
    
    // Free old sectors if file already has data
    if(file->start_sector != 0) {
        fs_free_sectors(file->start_sector);
    }
    
    // Calculate sectors needed
    unsigned int sectors_needed = fs_calculate_sectors_needed(size);
    unsigned int sectors[FS_FAT_ENTRIES];
    
    // Allocate sectors
    if(!fs_allocate_sectors(sectors_needed, sectors)) {
        return 0; // Failed to allocate sectors
    }
    
    // Write data to sectors
    unsigned int bytes_written = 0;
    int i;
    for(i = 0; i < sectors_needed && bytes_written < size; i++) {
        blockAddr = sectors[i];
        
        // Clear sector buffer
        int j;
        for(j = 0; j < FS_SECTOR_SIZE; j++) {
            At[j] = 0;
        }
        
        // Copy data to sector buffer
        unsigned int bytes_in_sector = FS_SECTOR_SIZE;
        if(bytes_written + bytes_in_sector > size) {
            bytes_in_sector = size - bytes_written;
        }
        
        for(j = 0; j < bytes_in_sector; j++) {
            At[j] = data[bytes_written + j];
        }
        
        write();
        bytes_written += bytes_in_sector;
    }
    
    // Update file entry
    file->size = size;
    file->start_sector = sectors[0];
    
    // Save directory to disk
    fs_save_directory();
    
    return bytes_written;
}

// List all files
int fs_list_files() {
    int count = 0;
    int i;
    
    char header[] = "\n=== Files ===\n";
    printString(header);
    
    for(i = 0; i < FS_MAX_FILES; i++) {
        // Check if entry is used and has a valid name
        if(dir_buffer[i].flags & FS_FILE_USED) {
            // Validate the filename is not empty or garbage
            int has_valid_name = 0;
            int j;
            for(j = 0; j < FS_MAX_FILENAME; j++) {
                if(dir_buffer[i].name[j] == '\0') {
                    if(j > 0) has_valid_name = 1; // Found null terminator after at least one char
                    break;
                }
                // Check for valid printable characters
                if(dir_buffer[i].name[j] < 32 || dir_buffer[i].name[j] >= 127) {
                    break; // Invalid character found
                }
            }
            
            // Only display if we have a valid name
            if(has_valid_name && j > 0) {
                // Add spacing before each file entry
                char spacing[] = "  ";
                printString(spacing);
                
                // Print filename (up to first null terminator)
                for(j = 0; j < FS_MAX_FILENAME && dir_buffer[i].name[j] != '\0'; j++) {
                    printChar(dir_buffer[i].name[j]);
                }
                
                // Add spacing after filename
                char spacing2[] = " - ";
                printString(spacing2);
                
                // Print size
                // Convert size to string
                unsigned int size = dir_buffer[i].size;
                char size_chars[16];
                int pos = 0;
                if(size == 0) {
                    size_chars[pos++] = '0';
                } else {
                    unsigned int temp = size;
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
                char bytes[] = " bytes";
                printString(bytes);
                
                if(dir_buffer[i].flags & FS_FILE_EXECUTABLE) {
                    char exec[] = " [EXECUTABLE]";
                    printString(exec);
                }
                printString("\n");  // Newline after each file
                count++;
            }
        }
    }
    
    if(count == 0) {
        char no_files[] = "No files found.\n";
        printString(no_files);
    }
    
    return count;
}

// Get file size
int fs_get_file_size(char* filename) {
    int dir_index = fs_find_file(filename);
    if(dir_index < 0) {
        return -1; // File not found
    }
    return dir_buffer[dir_index].size;
}

// Mark a file as executable
int fs_mark_executable(char* filename) {
    int dir_index = fs_find_file(filename);
    if(dir_index < 0) {
        return 0; // File not found
    }
    
    dir_buffer[dir_index].flags |= FS_FILE_EXECUTABLE;
    
    // Save directory to disk
    fs_save_directory();
    
    return 1; // Success
}

// Execute a file (load and jump to it)
int fs_execute_file(char* filename) {
    int dir_index = fs_find_file(filename);
    if(dir_index < 0) {
        char msg[] = "File not found: ";
        printString(msg);
        printString(filename);
        printString("\n");
        return 0;
    }
    
    struct FileEntry* file = &dir_buffer[dir_index];
    
    if(!(file->flags & FS_FILE_EXECUTABLE)) {
        char msg[] = "File is not executable: ";
        printString(msg);
        printString(filename);
        printString("\n");
        return 0;
    }
    
    if(file->start_sector == 0 || file->size == 0) {
        char msg[] = "File is empty: ";
        printString(msg);
        printString(filename);
        printString("\n");
        return 0;
    }
    
    // Read file into a buffer (we'll use a fixed address for execution)
    // For safety, we'll read it into a buffer and then execute
    char exec_buffer[4096]; // 4KB execution buffer
    int bytes_read = fs_read_file(filename, exec_buffer, 4096);
    
    if(bytes_read == 0) {
        char msg[] = "Failed to read file: ";
        printString(msg);
        printString(filename);
        printString("\n");
        return 0;
    }
    
    char msg[] = "Executing: ";
    printString(msg);
    printString(filename);
    printString("\n");
    
    // Note: Actual execution would require setting up proper memory protection
    // and jumping to the code. For now, we'll just indicate execution.
    // In a real implementation, you would:
    // 1. Allocate executable memory
    // 2. Copy code to that memory
    // 3. Set up stack and registers
    // 4. Jump to the code
    
    // For demonstration, we'll just print that execution was attempted
    char exec_msg[] = "Execution attempted (requires proper memory setup)\n";
    printString(exec_msg);
    
    return 1;
}
