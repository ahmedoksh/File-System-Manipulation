#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>

typedef struct __attribute__((__packed__)) super_block{
    uint8_t fs_id[8];
    uint16_t block_size;
    uint32_t file_system_block_count;
    uint32_t fat_start_block;
    uint32_t fat_block_count;
    uint32_t root_dir_start_block;
    uint32_t root_dir_block_count;
}super_block;

struct __attribute__((__packed__)) dir_entry_timedate_t {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} dir_entry_timedate_t;

typedef struct __attribute__((__packed__)) dir_entry_t {
    uint8_t status;
    uint32_t start_block;
    uint32_t block_count;
    uint32_t size;
    struct dir_entry_timedate_t create_time;
    struct dir_entry_timedate_t modify_time;
    uint8_t filename[31];
    uint8_t unused[6];
} dir_entry_t;

/**
 * Allocates memory of the given size using malloc and performs error handling.
 * 
 * @param size The size of the memory to be allocated.
 * @return void* A pointer to the allocated memory.
 */
void *emalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

/**
 * Reads the super block from a file and performs necessary byte order conversions.
 * 
 * @param filename The name of the file to read the super block from.
 * @return super_block* A pointer to the super block structure.
 */
super_block* get_superblock(char *filename){
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
        exit(1);
    }
    super_block *sb = (super_block *)emalloc(sizeof(super_block));
    fread(sb, sizeof(super_block), 1, file);
    sb->block_size = htons(sb->block_size);
    sb->file_system_block_count = htonl(sb->file_system_block_count);
    sb->fat_start_block = htonl(sb->fat_start_block);
    sb->fat_block_count = htonl(sb->fat_block_count);
    sb->root_dir_start_block = htonl(sb->root_dir_start_block);
    sb->root_dir_block_count = htonl(sb->root_dir_block_count);

    fclose(file);
    return sb;
}

/**
 * This function searches for a directory within a file system image.
 * 
 * @param filename The name of the file system image.
 * @param dir_path The path of the directory to find.
 * 
 * The function first opens the file system image and reads the superblock.
 * It then reads the FAT (File Allocation Table) into memory.
 * The function then tokenizes the directory path and searches for each directory in the path sequentially.
 * If a directory is found, it updates the current block and blocks count to the found directory's start block and block count.
 * If a directory is not found, it returns NULL.
 * 
 * @return A pointer to the directory entry if found, NULL otherwise.
 */
dir_entry_t *find_directory(char *filename, char *dir_path){
    super_block sb = *get_superblock(filename);
    
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
        exit(1);
    }
    uint32_t current_block = sb.root_dir_start_block;
    uint32_t blocks_count = sb.root_dir_block_count;
    bool found_dir = false;
    char *part = strtok(dir_path, "/");
    dir_entry_t *entry = (dir_entry_t*)emalloc(sizeof(dir_entry_t));

    fseek(file, sb.fat_start_block * sb.block_size, SEEK_SET);
    uint32_t *fat = (uint32_t *)emalloc(sb.fat_block_count * sb.block_size);
    fread(fat, sb.fat_block_count * sb.block_size, 1, file);
    
    int count = 0;
    while (part != NULL){
        found_dir = false;
        count = 0;
        while (count < blocks_count) {
            fseek(file, current_block * sb.block_size, SEEK_SET);
            for (int i = 0; i < sb.block_size / sizeof(dir_entry_t); i++) {
                fread(entry, sizeof(dir_entry_t), 1, file);
                if (entry->status == 5 && strcmp((char *)entry->filename, part) == 0) {
                    entry->start_block = htonl(entry->start_block);
                    entry->block_count = htonl(entry->block_count);
                    entry->size = htonl(entry->size);
                    entry->create_time.year = htons(entry->create_time.year);
                    entry->modify_time.year = htons(entry->modify_time.year);
                    found_dir = true;
                    current_block = entry->start_block;
                    blocks_count = entry->block_count;
                    break;
                }
            }
            if (found_dir){
                break;
            }
            count++;
            current_block = ntohl(fat[current_block]);
            }
            if (!found_dir){
                return NULL;
            }
            part = strtok(NULL, "/");
    }

    return entry;
}

/**
 * This function searches for a file within a file system image.
 * 
 * @param filename The name of the file system image.
 * @param file_path The path of the file to find.
 * 
 * It searches for a file withing a given file path 
 * If the file is found, it creates a file entry object to return it.
 * If the file is not found, it returns NULL.
 * 
 * @return A pointer to the file entry if found, NULL otherwise.
 */
dir_entry_t *find_file(char *filename, char *file_path){
    super_block sb = *get_superblock(filename);
    char *file_name;
    char *dir_path;
    if (strncmp(file_path, "./", 2) == 0) {
        file_path += 2;
    } else if (strncmp(file_path, "/", 1) == 0){
        file_path += 1;
    }
    char *last_l = strrchr(file_path, '/');

    uint32_t current_block;
    uint32_t blocks_count;
    if (last_l != NULL){
        file_name = last_l + 1;
        size_t dir_path_length = last_l - file_path;
        dir_path = (char *)malloc(dir_path_length + 1);
        strncpy(dir_path, file_path, dir_path_length);
        dir_path[dir_path_length] = '\0';
        dir_entry_t *dir = find_directory(filename, dir_path);
        if (dir == NULL){
            return NULL;
        }
        current_block = dir->start_block;
        blocks_count = dir->block_count;
        free(dir_path);
    }else{
        file_name = file_path;
        current_block = sb.root_dir_start_block;
        blocks_count = sb.root_dir_block_count;
    }

    bool found_file = false;

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
        exit(1);
    }

    fseek(file, sb.fat_start_block * sb.block_size, SEEK_SET);
    uint32_t *fat = (uint32_t *)emalloc(sb.fat_block_count * sb.block_size);
    fread(fat, sb.fat_block_count * sb.block_size, 1, file);

    dir_entry_t *entry = (dir_entry_t*)emalloc(sizeof(dir_entry_t));
    int count = 0;
    while (count < blocks_count) {
        fseek(file, current_block * sb.block_size, SEEK_SET);
        for (int i = 0; i < sb.block_size / sizeof(dir_entry_t); i++) {
            fread(entry, sizeof(dir_entry_t), 1, file);
            if (entry->status == 3 && strcmp((char *)entry->filename, file_name) == 0) {
                entry->start_block = htonl(entry->start_block);
                entry->block_count = htonl(entry->block_count);
                entry->size = htonl(entry->size);
                entry->create_time.year = htons(entry->create_time.year);
                entry->modify_time.year = htons(entry->modify_time.year);
                found_file = true;
                break;
            }
        }
        count++;
        if (found_file){
            break;
        }
        current_block = ntohl(fat[current_block]);
    }

    if (!found_file){
        return NULL;
    }
    return entry;
}

/**
 * This function displays information about a file system image.
 * 
 * @param filename The name of the file system image.
 *
 * It then prints the metadata found in superblock.
 * It also prints the FAT information.
 */
void diskinfo(char *filename){
    super_block sb = *get_superblock(filename);
    
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
        exit(1);
    }
    fseek(file, sb.fat_start_block * sb.block_size, SEEK_SET);

    uint32_t *fat;

    // Step 2: Allocate memory for the FAT
    fat = (uint32_t *)emalloc(sb.fat_block_count * sb.block_size);

    // Step 3: Read the FAT from the file
    fread(fat, sb.fat_block_count * sb.block_size, 1, file);

    uint32_t free_blocks = 0;
    uint32_t reserved_blocks = 0;
    uint32_t allocated_blocks = 0;

    for (uint32_t i = 0; i < sb.file_system_block_count; i++) {
        uint32_t entry = ntohl(fat[i]);
        if (entry == 0) {
            free_blocks++;
        }
        else if (entry == 1) {
            reserved_blocks++;
        }
        else {
            allocated_blocks++;
        }
    }

    assert(free_blocks + reserved_blocks + allocated_blocks == sb.file_system_block_count);
    printf("Super block information\n");
    printf("Block size: %u\n", sb.block_size);
    printf("Block Count: %u\n", sb.file_system_block_count);
    printf("FAT starts: %u\n", sb.fat_start_block);
    printf("FAT blocks: %u\n", sb.fat_block_count);
    printf("Root directory starts: %u\n", sb.root_dir_start_block);
    printf("Root directory blocks: %u\n", sb.root_dir_block_count);

    printf("\nFAT information\n");
    printf("Free blocks: %u\n", free_blocks);
    printf("Reserved blocks: %u\n", reserved_blocks);
    printf("Allocated blocks: %u\n", allocated_blocks);  
    
    fclose(file);
    free(fat);
}

/**
 * This function lists the contents of a directory in a file system image.
 * 
 * @param filename The name of the file system image.
 * @param subdir The path of the directory to list.
 * 
 * If a directory path is provided, it finds the directory in the file system image.
 * If no directory path is provided, it lists the contents of the root directory.
 * The function then reads the directory entries and prints information about each entry, including its type (file or directory), size, name, and creation time.
 * 
 * The function does not return a value.
 */
void disklist(char *filename, char *subdir){
    super_block sb = *get_superblock(filename);
    uint32_t current_block;
    uint32_t blocks_count;
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
        exit(1);
    }
    if (strncmp(subdir, "./", 2) == 0) {
        subdir += 2;
    }else if (strncmp(subdir, "/", 1) == 0){
        subdir += 1;
    }
    if (strlen(subdir) > 0){
        dir_entry_t *start_dir;
        start_dir = find_directory(filename, subdir);
        if (start_dir == NULL){
            printf("Directory not found.\n");
            exit(1);
        }
        current_block = start_dir->start_block;
        blocks_count = start_dir->block_count;
    }else{
        current_block = sb.root_dir_start_block;
        blocks_count = sb.root_dir_block_count;
    }

    fseek(file, sb.fat_start_block * sb.block_size, SEEK_SET);
    uint32_t *fat = (uint32_t *)emalloc(sb.fat_block_count * sb.block_size);
    fread(fat, sb.fat_block_count * sb.block_size, 1, file);

    fseek(file, current_block * sb.block_size, SEEK_SET);
    int count = 0;
    while (count < blocks_count) {
        dir_entry_t *entries = (dir_entry_t *)emalloc(sb.block_size);
        
        for (int i = 0; i < sb.block_size / sizeof(dir_entry_t); i++) {
            fread(&entries[i], sizeof(dir_entry_t), 1, file);
            entries[i].start_block = htonl(entries[i].start_block);
            entries[i].block_count = htonl(entries[i].block_count);
            entries[i].size = htonl(entries[i].size);
            entries[i].create_time.year = htons(entries[i].create_time.year);
            entries[i].modify_time.year = htons(entries[i].modify_time.year);
            

            if (entries[i].status != 0) {
                if (entries[i].status == 5) {
                    printf("D ");
                }
                else {
                    printf("F ");
                }
                printf("%10u ", entries[i].size);
                printf("%30s ", entries[i].filename);
                printf("%4u/%02u/%02u %02u:%02u:%02u", entries[i].create_time.year, entries[i].create_time.month, entries[i].create_time.day, entries[i].create_time.hour, entries[i].create_time.minute, entries[i].create_time.second);
                printf("\n");
            }
        }

        free(entries);
        current_block = ntohl(fat[current_block]);
        count++;
    }

    fclose(file);
}

/**
 * This function copies a file from a file system image to the local file system.
 * 
 * @param filename The name of the file system image.
 * @param file_path The path of the file to copy in the file system image.
 * @param dest_file_path The path of the destination file in the local file system.
 * 
 * The function first finds the file in the file system image.
 * If the file is not found, it prints an error message and exits the program.
 * It then opens the file system image and the destination file.
 * Then reads the file data block by block and writes it to the destination file.
 * 
 * The function does not return a value.
 */
void diskget(char *filename, char *file_path, char *dest_file_path){
    dir_entry_t *file = find_file(filename, file_path);
    if (file == NULL){
        printf("File not found.\n");
        exit(1);
    }
    super_block sb = *get_superblock(filename);

    FILE *src_file = fopen(filename, "rb");
    if (src_file == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
        exit(1);
    }

    FILE *dest_file = fopen(dest_file_path, "wb");


    fseek(src_file, sb.fat_start_block * sb.block_size, SEEK_SET);
    uint32_t *fat = (uint32_t *)emalloc(sb.fat_block_count * sb.block_size);
    fread(fat, sb.fat_block_count * sb.block_size, 1, src_file);

    uint8_t *buffer = (uint8_t *)emalloc(sb.block_size);
    uint32_t current_block = file->start_block;
    
    while (current_block != 0xFFFFFFFF) {
        fseek(src_file, current_block * sb.block_size, SEEK_SET);
        fread(buffer, sb.block_size, 1, src_file);
        fwrite(buffer, sb.block_size, 1, dest_file);
        current_block = ntohl(fat[current_block]);
    }

    fclose(src_file);
    fclose(dest_file);
    free(buffer);
    free(fat);
}

/**
 * This function finds a free block in the FAT (File Allocation Table) and marks it as used.
 * 
 * @param fat A pointer to the FAT.
 * @param fat_size The size of the FAT.
 * 
 * The function iterates over the FAT. If it finds a block that is free (i.e., its value is 0), it marks the block as used (i.e., sets its value to 1) and returns the index of the block.
 * If no free block is found, it prints an error message and exits the program.
 * 
 * @return The index of the free block found, or exits the program if no free block is found.
 */
uint32_t get_free_block(uint32_t *fat, uint32_t fat_size) {
    for (uint32_t i = 0; i < fat_size; i++) {
        if (ntohl(fat[i]) == 0) {
            fat[i] = htonl(1);  // Mark the block as used
            return i;
        }
    }
    fprintf(stderr, "Error: No free blocks available.\n");
    exit(1);
}

/**
 * This function copies a file from the local file system to a file system image.
 * 
 * @param filename The name of the file system image.
 * @param src_file_path The path of the source file in the local file system.
 * @param dest_file_path The path of the destination file in the file system image.
 * 
 * The function then a free entry in the directory and writes the source_file metadata to it.
 * It reads the source file data block by block and writes it to the file system image.
 * It updates the FAT and the directory entry as it writes the file data.
 */
void diskput(char *filename, char *src_file_path, char *dest_file_path){
    FILE *src_file = fopen(src_file_path, "rb");
    super_block sb = *get_superblock(filename);
    if (strncmp(dest_file_path, "./", 2) == 0) {
        dest_file_path += 2;
    }
    else if (strncmp(dest_file_path, "/", 1) == 0) {
        dest_file_path += 1;
    }
    if (src_file == NULL) {
        printf("File not found.\n");
        exit(1);
    }

    uint32_t current_block;
    uint32_t last_block;
    char *dest_file_name;
    char *dest_dir_path;
    char *last_l = strrchr(dest_file_path, '/');
    if (last_l != NULL){
        dest_file_name = last_l + 1;
        size_t dir_path_length = last_l - dest_file_path;
        dest_dir_path = (char *)malloc(dir_path_length + 1);
        strncpy(dest_dir_path, dest_file_path, dir_path_length);
        dest_dir_path[dir_path_length] = '\0';
        dir_entry_t *dir = find_directory(filename, dest_dir_path);
        if (dir == NULL){
            printf("Directory not found.\n");
            exit(1);
        }
        current_block = dir->start_block;
        last_block = dir->start_block + dir->block_count - 1;
        free(dest_dir_path);
    }else{
        dest_file_name = dest_file_path;
        current_block = sb.root_dir_start_block;
        last_block = sb.root_dir_start_block + sb.root_dir_block_count - 1;
    }

    FILE *dest_file = fopen(filename, "rb+");
    if (dest_file == NULL) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
        exit(1);
    }

    fseek(dest_file, sb.fat_start_block * sb.block_size, SEEK_SET);
    uint32_t *fat = (uint32_t *)emalloc(sb.fat_block_count * sb.block_size);
    fread(fat, sb.fat_block_count * sb.block_size, 1, dest_file);

    dir_entry_t *entry = (dir_entry_t*)emalloc(sizeof(dir_entry_t));
    bool found_free_entry = false;
    uint32_t entry_address;
    while (current_block <= last_block) {
        fseek(dest_file, current_block * sb.block_size, SEEK_SET);
        for (int i = 0; i < sb.block_size / sizeof(dir_entry_t); i++) {
            fread(entry, sizeof(dir_entry_t), 1, dest_file);
            if (entry->status == 0) {
                entry->status = 3;
                strncpy((char *)entry->filename, dest_file_name, sizeof(entry->filename));
                entry->start_block =  htonl(get_free_block(fat, sb.file_system_block_count));
                entry->block_count = htonl(1);
                entry->size = htonl(0);

                time_t current_time = time(NULL);
                struct tm *time_info = localtime(&current_time);
                entry->create_time.year = htons(time_info->tm_year + 1900);
                entry->create_time.month = time_info->tm_mon + 1;
                entry->create_time.day = time_info->tm_mday;
                entry->create_time.hour = time_info->tm_hour;
                entry->create_time.minute = time_info->tm_min;
                entry->create_time.second = time_info->tm_sec;
                entry->modify_time = entry->create_time;

                entry_address = ftell(dest_file) - sizeof(dir_entry_t);
                fseek(dest_file, -sizeof(dir_entry_t), SEEK_CUR); // Move back to the start of the entry
                fwrite(entry, sizeof(dir_entry_t), 1, dest_file);
                found_free_entry = true;
                break;
            }
        }
        if (found_free_entry){
            break;
        }
        current_block++;
    }
    if (found_free_entry == false){
        printf("No free space in directory.\n");
        exit(1);
    }
    uint8_t *buffer = (uint8_t *)emalloc(sb.block_size);
    uint32_t curr_block_index = ntohl(entry->start_block);
    uint32_t previous_block = curr_block_index;

    bool first_block = true;

    size_t bytes;
    while ((bytes = fread(buffer, 1, sb.block_size, src_file)) > 0) {
        // Write data to the current block in the destination file
        entry->size = htonl(ntohl(entry->size) + bytes);
        entry->block_count = htonl(ntohl(entry->block_count) + bytes);
        if (first_block) {
            first_block = false;
        }else{
            curr_block_index = get_free_block(fat, sb.file_system_block_count);
            fat[previous_block] = htonl(curr_block_index);
        }
        
        fseek(dest_file, curr_block_index * sb.block_size, SEEK_SET);
        fwrite(buffer, 1, bytes, dest_file);
        previous_block = curr_block_index;
    }
    fat[previous_block] = htonl(0xFFFFFFFF);

    fseek(dest_file, sb.fat_start_block * sb.block_size, SEEK_SET);
    fwrite(fat, sb.fat_block_count * sb.block_size, 1, dest_file);


    fseek(dest_file, entry_address, SEEK_SET); // Move back to the start of the entry
    fwrite(entry, sizeof(dir_entry_t), 1, dest_file);
    fclose(src_file);
    fclose(dest_file);
    free(buffer);
    free(fat);
}

#ifdef DISKINFO
int main(int argc, char *argv[]) {
    if (argc != 2){
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    }
    diskinfo(argv[1]);
    return 0;
}
#endif

#ifdef DISKLIST
int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3){
        fprintf(stderr, "Usage: %s <filename> [<dest_dir>]\n", argv[0]);
        exit(1);
    }
    disklist(argv[1], argc == 3 ? argv[2] : "./");
    return 0;
}
#endif

#ifdef DISKGET
int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4){
        fprintf(stderr, "Usage: %s <filename> <src_file> [<dst_file>]\n", argv[0]);
        exit(1);
    }
    char *dest_file_name;
    if (argc == 3){
        char *last_l = strrchr(argv[2], '/');
        if (last_l != NULL){
            dest_file_name = last_l + 1;
        }else{
            dest_file_name = argv[2];
        }
        diskget(argv[1], argv[2], dest_file_name);
    }else{
        diskget(argv[1], argv[2], argv[3]);
    }
    return 0;
}
#endif

#ifdef DISKPUT
int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4){
        fprintf(stderr, "Usage: %s <filename> <src_file> [<dst_file>]\n", argv[0]);
        exit(1);
    }
    char *dest_file_name;
    if (argc == 3){
        char *last_l = strrchr(argv[2], '/');
        if (last_l != NULL){
            dest_file_name = last_l + 1;
        }else{
            dest_file_name = argv[2];
        }
        diskput(argv[1], argv[2], dest_file_name);
    }else{
        diskput(argv[1], argv[2], argv[3]);
    }
    return 0;
}
#endif