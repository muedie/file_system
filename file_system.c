/**
 *  Safal Lamsal 
**/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define WHITESPACE " \t\n"
#define MAX_COMMAND_SIZE 255 
#define MAX_NUM_ARGUMENTS 5

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES  128
#define NUM_INODES 128
#define MAX_BLOCKS_PER_FILE 32

/**  Main File System Pointer  **/
unsigned char data_blocks[NUM_BLOCKS][BLOCK_SIZE];
          int used_blocks[NUM_BLOCKS]; // 0 - Free, 1 - Used
        
        
/** Directory Entry Structure **/
struct directory_entry {
  char* name;
  int used;
  int inode_idx;
};

struct directory_entry *directory_ptr;

/**  Inode Structure  **/
struct inode {
  time_t date;
  int used;
  int size;
  int blocks[MAX_BLOCKS_PER_FILE];
};

struct inode * inode_array_ptr[NUM_INODES];

int del_counter = 0;
int del_array[128]; // Holds deleted items inode index

/**
  Description: Initializes the file system.
  return (void)
**/
void init() {
  int i;
  directory_ptr = (struct directory_entry*)&data_blocks[0];
  
  for (i = 0; i < NUM_FILES; i++) {
    directory_ptr[i].used = 0;
  }
  
  int inode_idx = 0;
  for (i = 1; i < 130; i++) {
    inode_array_ptr[inode_idx++] = (struct inode*)&data_blocks[i];
  }
  
  for (i = 0; i < 128; i++) {
    int j;
    for ( j = 0; j < MAX_BLOCKS_PER_FILE; j++) {
      inode_array_ptr[i]->blocks[j] = -1;
    }

    del_array[i] = -1; // set up delition array
  }
}

/**
  Description: Finds a free directory entry.
  return: (int) index for free entry.
**/
int findFreeDirectoryEntry() {
  int i;
  int ret = -1;
  for ( i = 0; i < 128; i++ ) {
    if ( directory_ptr[i].used == 0 ) {
      ret = i;
      break;
    }
  }
  return ret;
}

/**
  Description: Finds a free inode.
  return: (int) index in inode array.
**/
int findFreeInode() {
  int i;
  int ret = -1;
  for (i = 0; i < 128; i++) {
    if ( inode_array_ptr[i]->used == 0 ) {
      ret = i;
      break;
    }
  }
  return ret;
}

/**
  Description: Finds free block.
  return (int) index of free block.
**/
int findFreeBlock() {
  int i;
  int ret = -1;
  for (i = 130; i < 4226; i++) {
    if (used_blocks[i] == 0) {
      ret = i;
      break;
    }
  }
  return ret;
}

/**
  Description: Finds a free block entry for given inode index.
  @params: inode_idx - the inode index to find free block entry.
  return: (int) index of free block entry
**/
int findFreeInodeBlockEntry (int inode_idx) {
  int i;
  int ret = -1;
  for ( i = 0; i < 32; i++) {
    if (inode_array_ptr[inode_idx]->blocks[i] == -1) {
      ret = i;
      break;
    }
  }
  return ret;
}

/**
  Description: Returns the amount of disk space left.
  return (int) disk space left.
**/
int df () {
  int count = 0;
  int i = 0;
  for (i = 130; i < 4226; i++) {
    if (used_blocks[i] == 0) {
      count++;
    }
  }
  return count*BLOCK_SIZE;
}

/**
  @params: args - User Input
  Description:   Copies the file to the file system.
  return (void)
**/
void put (char** args) {

  if (args[1] == NULL) {
    printf("Error:\tInvalid input.\nUsage:\tput <filename>\n\n");
    return;
  }
  
  if (strlen(args[1]) > 32) {
    printf("Error: File name must be less than 32 characters.\n\n");
    return;
  }
  
  struct stat buf; /** holds file metadata **/
  int status = stat(args[1], &buf);
  
  if (status == -1) {
    printf("Error: File not found.\n\n");
    return;
  }
  
  if ( buf.st_size > df() ) {
    printf("Error: Not enough disk space.\n\n");
    return;
  }
  
  int dir_idx = findFreeDirectoryEntry();
  if (dir_idx == -1) {
    printf("Error: Not enough directory entries.\n\n");
    return;
  }
  
  int inode_idx = findFreeInode();
  if (inode_idx == -1) {
    printf("Error: No free inodes.\n\n");
    return;
  }
  
  directory_ptr[dir_idx].used = 1;
  directory_ptr[dir_idx].name = (char*)malloc(strlen(args[1]));
  strncpy(directory_ptr[dir_idx].name, args[1], strlen(args[1]));
  directory_ptr[dir_idx].name[strlen(args[1])] = 0;
  directory_ptr[dir_idx].inode_idx = inode_idx;
  
  inode_array_ptr[inode_idx]->size = buf.st_size;
  inode_array_ptr[inode_idx]->date = time(NULL);
  inode_array_ptr[inode_idx]->used = 1;
  
  // Open file
  FILE* fp = fopen(args[1], "r");
  
  int copy_size = buf.st_size;
  int offset = 0; // File offset
  
  // Main data loop
  while (copy_size >= BLOCK_SIZE) {
    int block_idx = findFreeBlock();
    
    if (block_idx == -1) {
      printf("Error: No free block avaiable.\n\n");
      return;
    }
    
    used_blocks[block_idx] = 1;
    
    int inode_block_entry = findFreeInodeBlockEntry(inode_idx);
    if (inode_block_entry == -1) {
      printf("Error: No free inode block.\n\n");
      return;
    }
    inode_array_ptr[inode_idx]->blocks[inode_block_entry] = block_idx;
    
    fseek(fp, offset, SEEK_SET);
    fread(data_blocks[block_idx], BLOCK_SIZE, 1, fp);
    
    copy_size -= BLOCK_SIZE;
    offset += BLOCK_SIZE;
  }
  
  // handle remainder data
  if (copy_size > 0) {
    int block_idx = findFreeBlock();
    
    if (block_idx == -1) {
      printf("Error: No free block avaiable.\n\n");
      return;
    }
    
    used_blocks[block_idx] = 1;
    
    int inode_block_entry = findFreeInodeBlockEntry(inode_idx);
    if (inode_block_entry == -1) {
      printf("Error: No free inode block.\n\n");
      return;
    }
    inode_array_ptr[inode_idx]->blocks[inode_block_entry] = block_idx;
    
    fseek(fp, offset, SEEK_SET);
    fread(data_blocks[block_idx], copy_size, 1, fp);
  }
  
  fclose(fp);
  return;
}

/**
  @params: args - User Input
  Description:  Retrieves the file from the file system. If additional parameter provided, 
                the string will be the name of the file.
  return (void)
**/
void get (char** args) {
  if (args[1] == NULL) {
    printf("Error:\tInvalid input.\nUsage:\tget <filename>\n\tget <filename> <newfilename>\n\n");
    return;
  }
  if (args[2] && strlen(args[2]) > 32) {
    printf("Error: File name must be less than 32 characters.\n\n");
    return;
  }
  
  int i, index = -1;
  for (i = 0; i < NUM_FILES; i++) {
    if (directory_ptr[i].used == 1) {
      if (strcmp(directory_ptr[i].name, args[1]) == 0) {
        index = i;
        break;
      }
    }
  }
  
  if (index == -1) {
    printf("Error: File not found.\n\n");
    return;
  }
  
  char* filename = args[1];
  
  if (args[2]) {
    filename = args[2];
  }
  
  
  FILE* fp = fopen(filename, "w");
  
  if (!fp) {
    printf("Could not open output file: %s\n", args[1] );
    return;
  }
  
  int block_idx = directory_ptr[index].inode_idx;
  int copy_size = inode_array_ptr[block_idx]->size;
  int offset = 0;
  
  int ridx = 0;
  
  while (copy_size > 0) {
  
    int read_index = inode_array_ptr[block_idx]->blocks[ridx];
  
    int num_bytes;
    
    if (copy_size < BLOCK_SIZE) {
      num_bytes = copy_size;
    } else {
      num_bytes = BLOCK_SIZE;
    }
    
    fwrite(data_blocks[read_index], num_bytes, 1, fp);
    
    copy_size -= BLOCK_SIZE;
    offset += BLOCK_SIZE;
    ridx++;
    
    fseek(fp, offset, SEEK_SET);
  }
  
  fclose(fp);
  return;
}

/**
  @params: args - User Input
  Description:  Deletes the file from the file system.
  return (void)
**/
void del (char** args) {
  if (args[1] == NULL) {
    printf("Error:\tInvalid input.\nUsage:\tdel <filename>\n\n");
    return;
  }
  
  int i, index = -1;
  for (i = 0; i < NUM_FILES; i++) {
    if (directory_ptr[i].used == 1) {
      if (strcmp(directory_ptr[i].name, args[1]) == 0) {
        index = i;
        
        directory_ptr[i].used = 0;
        
        int i_index = directory_ptr[i].inode_idx;
        inode_array_ptr[i_index]->used = 0;
        
        int j;
        for (j = 0; j < MAX_BLOCKS_PER_FILE; j++) {
          int block = inode_array_ptr[i_index]->blocks[j];
          inode_array_ptr[i_index]->blocks[j] = -1;
          used_blocks[block] = 0;
        }
        
        break;
      }
    }
  }
  
  if (index == -1) {
    printf("Error: File not found.\n\n");
    return;
  }
  
}

/**
  @params: args - User Input
  Description:  Marks the file as deleted from the file system.
  return (void)
**/
void psuedo_del (char** args) {
  if (args[1] == NULL) {
    printf("Error:\tInvalid input.\nUsage:\tdel <filename>\n\n");
    return;
  }
  
  int i, index = -1;
  for (i = 0; i < NUM_FILES; i++) {
    if (directory_ptr[i].used == 1) {
      if (strcmp(directory_ptr[i].name, args[1]) == 0) {
        index = i;    
        directory_ptr[i].used = 0;

        int j = 0;
        while (del_array[j] != -1) {
          j++;
        }

        del_array[j] = i;
        del_counter++;
      
        break;
      }
    }
  }
  
  if (index == -1) {
    printf("Error: File not found.\n\n");
    return;
  }

}

/**
  Description:  Deletes the file inside del_array from the file system.
  return (void)
**/
void clear_del() {
  int i;
  for (i = 0; i <= del_counter; i++) {

    if (del_array[i] != -1) {
      int i_index = directory_ptr[del_array[i]].inode_idx;
      inode_array_ptr[i_index]->used = 0;
      
      int j;
      for (j = 0; j < MAX_BLOCKS_PER_FILE; j++) {
        int block = inode_array_ptr[i_index]->blocks[j];
        inode_array_ptr[i_index]->blocks[j] = -1;
        used_blocks[block] = 0;
      }
    }

    del_array[i] = -1;
  }

  del_counter = 0;
}

/**
  @params: args - User Input
  Description:  Un-Deletes the file from the file system.
  return (void)
**/
void undelete(char** args) {

  if (args[1] == NULL) {
    printf("Error:\tInvalid input.\nUsage:\tundelete <filename>\n\n");
    return;
  }

  int i;
  for (i = 0; i < del_counter; i++) {
    char* name = directory_ptr[del_array[i]].name;
    if (strcmp(name, args[1]) == 0) {
      directory_ptr[i].used = 1;
      break;
    }
  }

  del_array[i] = -1;
}

/**
  @params: args - User Input
  Description:  Lists the files in the file system.
  return (void)
**/
void list (char** args) {
  int i, found = 0;
  for (i=0; i < 128; i++) {
    if ( directory_ptr[i].used == 1) {
    
      int inode_idx = directory_ptr[i].inode_idx;
      int size = inode_array_ptr[inode_idx]->size;
      time_t date = inode_array_ptr[inode_idx]->date;
      
      char* time_string = ctime(&date);
      time_string[strlen(time_string) -1] = 0;
    
      printf("%d\t%s\t%s\n", size, time_string, directory_ptr[i].name);
      found = 1;
    }
  }
  
  if (!found)
    printf("list: No files found.\n");
    
  return;
}

int main(int argc, char *argv[]) {

  init();

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  
  while(1) {
  
    // print out prompt
    printf("mfs> ");
    
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
    
    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // blank input case
    if(token[0] == NULL) continue;

    // termination cases
    if (strcmp(token[0], "exit") == 0 || strcmp(token[0], "quit") == 0) exit(0);
    
    if (strcmp(token[0], "put") == 0) {
      if (del_counter > 0) {
        clear_del();
      }
      put(token);
    } else if (strcmp(token[0], "get") == 0) {
      get(token);
    } else if (strcmp(token[0], "del") == 0) {
      psuedo_del(token);
      //del(token);
    } else if (strcmp(token[0], "list") == 0) {
      list(token);
    } else if (strcmp(token[0], "df") == 0) {
      printf("Disk Space: %d bytes.\n", df());
    } else if (strcmp(token[0], "undelete") == 0) {
      undelete(token);
    } else{
      printf("Error: Invalid Command.\n\n");
    }

    // freeing allocated vars
    free( working_root );
  }

}
