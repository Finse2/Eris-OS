void setMonitorColor(char);
void cls();
void printString(const char*);
void vid();
void put();
void get();

extern void read();
extern void write();

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


int blockAddr;
char At[1024];

char* TM_START;

void blink(){
	setMonitorColor(0x59);
	int TIME_OUT = 0x10fffff;
	while(--TIME_OUT);
	setMonitorColor(0xa5);
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
	
	char cmd1[] = "CLS";
	char cmd2[] = "COLORA";
	char cmd3[] = "COLORB";
	char cmd4[] = "COLORC";
	char cmd5[] = "COLORDEF";
	char cmd6[] = "VID";
	char cmd7[] = "HI";
	char cmd8[] = "PUT";
	char cmd9[] = "GET";
	
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

	char msg1[] = "\nHELLO , Welcome to my OS\n";

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
		
		i = 0;
		while(i < 511){
			At[i] = 0;  // Clears the content
			i++;
		}
	}		
	else if(strcmp(command , cmd9)){
		blockAddr = 0;
		get();
		printString(At);
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