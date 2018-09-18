#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

FILE * OUTPUT;
FILE * INIT;
FILE * SAVE;
FILE * INPUT;

struct open_file{
    int position;
    int index;
    int length;
    unsigned char * p; 
};

unsigned char* ldisk[64];
unsigned char* first_seven_blocks[6];
struct open_file OFT[4];


void read_block(int block_index, unsigned char *p){ //read/store a block from ldisk to p
    unsigned char temp[64];
    for (int i = 0; i<64; i++){
        temp[i] = ldisk[block_index][i];
    }
    memcpy(p,temp,64);

}


void write_block(int block_index, unsigned char*p){ //write a block from p to ldisk
    for (int i = 0; i <64; i++){
        ldisk[block_index][i] = p[i];
    }
}


void grab_slot_block_numbers(int i, int * numbers){ //takes in a descriptor index and returns small array of the 3 block #'s that point to the data
    int index = i/4 +1;
    numbers[0] = (uint32_t)first_seven_blocks[index][4+16*(i%4)] << 24 |
          (uint32_t)first_seven_blocks[index][5+16*(i%4)] << 16 |
          (uint32_t)first_seven_blocks[index][6+16*(i%4)] << 8  |
          (uint32_t)first_seven_blocks[index][7+16*(i%4)]; 
    
    numbers[1] = (uint32_t)first_seven_blocks[index][8+16*(i%4)] << 24 |
          (uint32_t)first_seven_blocks[index][9+16*(i%4)] << 16 |
          (uint32_t)first_seven_blocks[index][10+16*(i%4)] << 8  |
          (uint32_t)first_seven_blocks[index][11+16*(i%4)]; 

    numbers[2] = (uint32_t)first_seven_blocks[index][12+16*(i%4)] << 24 |
          (uint32_t)first_seven_blocks[index][13+16*(i%4)] << 16 |
          (uint32_t)first_seven_blocks[index][14+16*(i%4)] << 8  |
          (uint32_t)first_seven_blocks[index][15+16*(i%4)]; 
}


int put_slot_block_number(int block_num, int i){ //takes a block number to be inserted and the OFT descriptor index in order to record the allocated block number in the descriptor
    int index = i/4+1;
    for (int j = 1; j<4; j++){
        if (first_seven_blocks[index][(4*j)+16*(i%4)] ==0 &&
            first_seven_blocks[index][(4*j+1)+16*(i%4)] ==0 &&
            first_seven_blocks[index][(4*j+2)+16*(i%4)] ==0 &&
            first_seven_blocks[index][(4*j+3)+16*(i%4)] ==0){
                unsigned int n = block_num;
                first_seven_blocks[index][(4*j)+16*(i%4)] = (n >> 24) & 0xFF;
                first_seven_blocks[index][(4*j+1)+16*(i%4)] = (n >> 16) & 0xFF;
                first_seven_blocks[index][(4*j+2)+16*(i%4)] = (n >> 8) & 0xFF;
                first_seven_blocks[index][(4*j+3)+16*(i%4)] = n & 0xFF;
                return 0;
        }   
    }
    return -1;
}


int reset_block_numbers(int i){ //takes in a descriptor index and sets all of the slot block numbers to zero
    int index = i/4+1;
    for (int j = 1; j<4; j++){
        first_seven_blocks[index][(4*j)+16*(i%4)] =0;
        first_seven_blocks[index][(4*j+1)+16*(i%4)]=0;
        first_seven_blocks[index][(4*j+2)+16*(i%4)]=0;
        first_seven_blocks[index][(4*j+3)+16*(i%4)] =0;
    }
}


int grab_slot_block_length(int i){ //takes in a descriptor index and returns the descriptor length 
    int index = i/4+1;
    int length = (uint32_t)first_seven_blocks[index][0+16*(i%4)] << 24 |
          (uint32_t)first_seven_blocks[index][1+16*(i%4)] << 16 |
          (uint32_t)first_seven_blocks[index][2+16*(i%4)] << 8  |
          (uint32_t)first_seven_blocks[index][3+16*(i%4)];
    return length; 
}


void put_slot_block_length(int length,int i){ //takes a length to be inserted and the OFT descriptor index in order to record the length in the descriptor
    int index = i/4+1;
    unsigned int n = length;
    first_seven_blocks[index][0+16*(i%4)] = (n >> 24) & 0xFF;
    first_seven_blocks[index][1+16*(i%4)] = (n >> 16) & 0xFF;
    first_seven_blocks[index][2+16*(i%4)] = (n >> 8) & 0xFF;
    first_seven_blocks[index][3+16*(i%4)] = n & 0xFF;
}


void deallocate_bitmap(int block_num){ //uses the first 2 integers (8bytes/64bits) to visually deallocate blocks
    int bitmap_index = (block_num/8);
    first_seven_blocks[0][bitmap_index] &= ~(128 >> (block_num%8));
}


void allocate_bitmap(int block_num){ //uses the first 2 integers (8bytes/64bits) to visually allocatate blocks
    int bitmap_index = (block_num/8);
    first_seven_blocks[0][bitmap_index] |= (128 >>(block_num%8));
}


int free_bitmap_block(){ //iterates the bitmap to return the first available data block to be used, -1 if no free blocks
    for (int i = 0; i<8; i++){
        unsigned char s;
        s = first_seven_blocks[0][i];
        for (int j=0;j<8;j++){
            s |= (128 >> (j%8));
            if (s != first_seven_blocks[0][i]){
                return j + (8*i);
            }
        }
    }
    return -1;
}


void nondirectory_data_block(int block_num,int i){ //creates a block in ldisk for nondirectory data (no need for unallocated -1's) + should update bitmap + record block in first_seven_blocks
    allocate_bitmap(block_num);
    put_slot_block_number(block_num,i);
}


void directory_data_block(int block_num,int i){ //creates a block in ldisk with unallocated (-1) indexes for free directory entries + should update bitmap + record block in first_seven_blocks
    unsigned char *p = (unsigned char *)malloc(64*sizeof(unsigned char));
    for (int i = 0; i<9; i++){
        int offset = 8*(i-1);
        unsigned int n = -1;
        p[4+offset] = (n >> 24) & 0xFF;
        p[5+offset] = (n >> 16) & 0xFF;
        p[6+offset] = (n >> 8) & 0xFF;
        p[7+offset] = n & 0xFF;
    }
    write_block(block_num,p);
    allocate_bitmap(block_num);
    put_slot_block_number(block_num,i);
}


int lseek(int index, int pos){ //take the OFT index and a position number +
                               //if new position is not within the current block then write the buffer to disk
                               //and read the new block + set the current position to new position +
                               //return status
    if (index<0 || index >3 || pos <0 || pos >191 || pos > OFT[index].length){
        return -1; //invalid index or invalid position
    }
    int current = (OFT[index].position-1)/64;
    int new = pos/64;
    if (current != new){
        int blocks[3];
        grab_slot_block_numbers(OFT[index].index,blocks);
		if (blocks[current] != 0){
			write_block(blocks[current],OFT[index].p);
        }
        read_block(blocks[new],OFT[index].p);
    }
    OFT[index].position = pos;
    return pos;
}


int write_dir_file(int OFT_index, unsigned char *p, int bytes_to_copy){ //starts at beginning position of the dir_file +
                                                                        //writes data and increases position
                                                                        //(is not in charge of swapping data blocks in R/W buffer)
    int pos = OFT[0].position;
    int count = 0;
    for(int i = pos; i< (pos+bytes_to_copy); i++){
        OFT[0].p[i%64] = p[count];
        count++;
        OFT[0].position++;
    }
}


int read_dir_file(int OFT_index,unsigned char *p, int bytes_to_copy){ //starts at beginning position of the dir_file +
                                                                      //checks whether or not to continue with reading if the next block has been allocated properly +
                                                                      //swaps out blocks in the R/W buffer if possible
    int pos = OFT[0].position;
    int count = 0;
    int blocks[3];
    grab_slot_block_numbers(0,blocks);
    for(int i = pos; i<(pos+bytes_to_copy);i++){
        if (i == 64){
            if (blocks[1] != 0){
                write_block(blocks[0],OFT[0].p);
                read_block(blocks[1],OFT[0].p);
            }
            else{
                return -1;
            }
        }
        else if (i == 128){
            if (blocks[2] != 0){
                write_block(blocks[1],OFT[0].p);
                read_block(blocks[2],OFT[0].p);
            }
            else{
                return -1;
            }
        }
        p[count] = OFT[0].p[i%64];
        count++;
        OFT[0].position++;
    }
    return 0;
}


int read_file(int OFT_index, int bytes_to_print){
	if (OFT_index <1 || OFT_index >3){
		fprintf(OUTPUT,"error");
		return -1;
	}
    int pos = OFT[OFT_index].position;
    for(int i = pos; i<(pos+bytes_to_print);i++){
        int blocks[3];
        grab_slot_block_numbers(OFT[OFT_index].index,blocks);
        if (i == 64){
            if (blocks[1] != 0){
                write_block(blocks[0],OFT[OFT_index].p);
                read_block(blocks[1],OFT[OFT_index].p);
            }
            else{
                break;
            }
        }
        else if (i == 128){
            if (blocks[2] != 0){
                write_block(blocks[1],OFT[OFT_index].p);
                read_block(blocks[2],OFT[OFT_index].p);
            }
            else{
                break;
            }
        }
        else if (i >= 192){
            break;
        }
        char t[2];
        t[0] = OFT[OFT_index].p[i%64];
        t[1] = '\0';
        fprintf(OUTPUT,"%s",t);
        OFT[OFT_index].position++;
    }
    return 0;

}


int write_file(int OFT_index,char input, int bytes_to_copy){
	if (OFT_index <1 || OFT_index >3){
		fprintf(OUTPUT,"error");
		return -1;
	}
    int pos = OFT[OFT_index].position;
    int count = 0;
    for(int i = pos; i< (pos+bytes_to_copy); i++){
        int blocks[3];
        grab_slot_block_numbers(OFT[OFT_index].index,blocks);
        if (i == 64){
            if (blocks[1] != 0){
                write_block(blocks[0],OFT[OFT_index].p);
                read_block(blocks[1],OFT[OFT_index].p);
            }
            else{
                int free_block = free_bitmap_block();
                nondirectory_data_block(free_block,OFT[OFT_index].index);
                write_block(blocks[0],OFT[OFT_index].p);
                read_block(free_block,OFT[OFT_index].p);
            }
        }
        else if (i == 128){
            if (blocks[2] != 0){
                write_block(blocks[1],OFT[OFT_index].p);
                read_block(blocks[2],OFT[OFT_index].p);
            }
            else{
                int free_block = free_bitmap_block();
                nondirectory_data_block(free_block,OFT[OFT_index].index);
                write_block(blocks[1],OFT[OFT_index].p);
                read_block(free_block,OFT[OFT_index].p);
            }
        }
        else if (i >= 192){
            break;
        }
        OFT[OFT_index].p[i%64] = input;
        count++;
        OFT[OFT_index].position++;
        OFT[OFT_index].length++;
    }
    fprintf(OUTPUT,"%d bytes written",count);
    return count;
}

void get_first_seven(){ //separate data structure to keep track of bit map and file descriptors on the disk +
                        //descriptor lengths are set to -1 in order to find free descriptors (except descriptor 0) +
                        //will write contents of blocks to the disk before saving!!!
    first_seven_blocks[0] = (unsigned char*)malloc(64*sizeof(unsigned char));
    for (int i = 1; i<7;i++){
        first_seven_blocks[i] = (unsigned char*)malloc(64*sizeof(unsigned char));
    }
    put_slot_block_length(0,0);
    for (int i=1; i<24; i++){
        put_slot_block_length(-1,i);
    }
}


int free_file_descriptor(){ //finds the first file descriptor of length -1 and returns the file descriptor index
    int descriptor_index =-1;
    for (int i=1; i<24; i++){
        int index = i/4 + 1;
        int num = 0;
        num = (uint32_t)first_seven_blocks[index][0+16*(i%4)] << 24 |
          (uint32_t)first_seven_blocks[index][1+16*(i%4)] << 16 |
          (uint32_t)first_seven_blocks[index][2+16*(i%4)] << 8  |
          (uint32_t)first_seven_blocks[index][3+16*(i%4)];
        if (num == -1){
            descriptor_index = i;
            break;
        }
    }
    return descriptor_index;
}


int close_file(int OFT_index){ //takes in an OFT index and writes the file's current block to ldisk + free up the OFT
    if (OFT[OFT_index].index == -1){
        return -1;
    }
    int blocks[3];
    grab_slot_block_numbers(OFT[OFT_index].index,blocks);
    write_block(blocks[(OFT[OFT_index].position-1)/64],OFT[OFT_index].p);
    put_slot_block_length(OFT[OFT_index].length,OFT[OFT_index].index);
    OFT[OFT_index].index = -1;
    return OFT_index;
}


int destroy(char *filestring){ //takes a filename and iterates through the OFT R/W buffer until the name is found +
                               //clears the filename and adds the index -1 in order to free it up
                               //clears block slot numbers in the descriptor + free descriptor +
                               
    int save_pos = OFT[0].position;
    int blocks[3];
    grab_slot_block_numbers(0,blocks);
    write_block(blocks[(save_pos-1)/64],OFT[0].p);
    lseek(0,0);
    int grab_status = 0;
    for (int i=0; i<24; i++){
        unsigned char temp[8];
        char file_name[4];
        int read_status = read_dir_file(0,temp,8);
        if (read_status == -1){
            OFT[0].position = save_pos;
            read_block(blocks[(save_pos-1)/64],OFT[0].p);
            break;
        }
			file_name[0] = temp[0];
			file_name[1] = temp[1];
			file_name[2] = temp[2];
			file_name[3] = temp[3];	
        if (strcmp(filestring,file_name) ==0){
            int file_placement_index = (uint32_t)temp[4] << 24 |
            (uint32_t)temp[5] << 16 |
            (uint32_t)temp[6] << 8  |
            (uint32_t)temp[7]; 
            int blocks2[3];
            for (int k=1; k<4; k++){
                if (OFT[k].index == file_placement_index){
                    close_file(k);
                }
            }
            grab_slot_block_numbers(file_placement_index,blocks2);
            for (int j=0;j<3;j++){
                if (blocks2[j] != 0){
                    deallocate_bitmap(blocks2[j]);
                }
            }
            put_slot_block_length(-1,file_placement_index);
            reset_block_numbers(file_placement_index);
            OFT[0].position-=8;
            unsigned char temp2[8];
            temp2[0] = 0;
            temp2[1] = 0;
            temp2[2] = 0;
            temp2[3] = 0;
            unsigned int n = -1;
            temp2[4] = (n >> 24) & 0xFF;
            temp2[5] = (n >> 16) & 0xFF;
            temp2[6] = (n >> 8) & 0xFF;
            temp2[7] = n & 0xFF;
            write_dir_file(0,temp2,8);
            write_block(blocks[(OFT[0].position-1)/64],OFT[0].p);
            grab_status = 1;
        }
    }
    if (grab_status == 0){
        fprintf(OUTPUT,"error");
        return -1; //failed to find file to destroy
    }
    OFT[0].length--;
    fprintf(OUTPUT,"%s destroyed",filestring);
    return 0;

}


int create(char*filestring){ //searches through file descriptors to find a free descriptor with index +
                            //iterates through the data block to check for duplicates and free data area +
                            //creates a new directory data block if full/initialized
    if (strlen(filestring)>3 || strlen(filestring) ==0){
        fprintf(OUTPUT,"error");
        return -1; //file name too short/long
    }
    int des_index = free_file_descriptor();
    if (des_index == -1){
        fprintf(OUTPUT,"error");
        return -2; //no free descriptor
    }
    if (OFT[0].length >23){
        fprintf(OUTPUT,"error");
        return -5; //too many files created
    }
    int block_count[3];
    int b_count = 0;
    grab_slot_block_numbers(0,block_count);
    for (int k=0; k<3 && block_count[k] != 0; k++){
        b_count++;    
    }
    if (b_count == 0){
        int free_block = free_bitmap_block();
        directory_data_block(free_block,OFT[0].index);
        read_block(free_block,OFT[0].p);
    } 
    else if (OFT[0].length%(8*b_count) == 0 && OFT[0].length>0){
        int free_block = free_bitmap_block();
        directory_data_block(free_block,OFT[0].index);
    }
	
    int save_pos = OFT[0].position;
    int blocks[3];
    grab_slot_block_numbers(0,blocks);
    write_block(blocks[(save_pos-1)/64],OFT[0].p);
    lseek(0,0);
    int grab_status = 0;
    for (int i=0; i<24; i++){ //iterate only as many times as allocated directory data
        unsigned char temp[8];
        char file_name[4];
        int read_status = read_dir_file(0,temp,8);
        if (read_status == -1){
            OFT[0].position = save_pos;
            read_block(blocks[(save_pos-1)/64],OFT[0].p);
            break;
        }
			file_name[0] = temp[0]; 
			file_name[1] = temp[1];
			file_name[2] = temp[2];
			file_name[3] = temp[3]; //store the filename
        if (strcmp(filestring,file_name) ==0){
            fprintf(OUTPUT,"error");
            return -3; //duplicate name
        }
        int file_placement_index = (uint32_t)temp[4] << 24 |
          (uint32_t)temp[5] << 16 |
          (uint32_t)temp[6] << 8  |
          (uint32_t)temp[7]; //grab the filename's index
        if (file_placement_index == -1 && grab_status == 0){
            OFT[0].position-=8;
            unsigned char *temp2 = (unsigned char*)malloc(8*sizeof(unsigned char));
            int null =0;
            for (int j = 0; j<strlen(filestring);j++){
                temp2[j] = filestring[j];
                null++;
            } 
            temp2[null] = '\0';
            unsigned int n = des_index;
            temp2[4] = (n >> 24) & 0xFF;
            temp2[5] = (n >> 16) & 0xFF;
            temp2[6] = (n >> 8) & 0xFF;
            temp2[7] = n & 0xFF;
            write_dir_file(0,temp2,8);
            put_slot_block_length(0,des_index);
            write_block(blocks[(OFT[0].position-1)/64],OFT[0].p);
            grab_status = 1;
        }
    }
    if (grab_status == 0){
        fprintf(OUTPUT,"error");
        return -4; //no free data
    }
    OFT[0].length++;
    fprintf(OUTPUT,"%s created",filestring);
    return 0;
}


int get_specific_file_descriptor_index(char*s){ //takes in the name of a file and iterates through all 3 blocks of the directory to return the index number
	int save_pos = OFT[0].position;
    int blocks[3];
    grab_slot_block_numbers(0,blocks);
    write_block(blocks[(save_pos-1)/64],OFT[0].p);
    int file_placement_index = -1;
    lseek(0,0);
    for (int i=0; i<24; i++){
        unsigned char temp[8];
        char file_name[4];

        int read_status = read_dir_file(0,temp,8);
        if (read_status == -1){
            OFT[0].position = save_pos;
            read_block(blocks[(save_pos-1)/64],OFT[0].p);
            break;
        }

        file_name[0] = temp[0];
        file_name[1] = temp[1];
        file_name[2] = temp[2];
        file_name[3] = temp[3];
        if (strcmp(file_name,s)==0){
			file_placement_index = (uint32_t)temp[4] << 24 |
			(uint32_t)temp[5] << 16 |
			(uint32_t)temp[6] << 8  |
			(uint32_t)temp[7]; 
            OFT[0].position = save_pos;
            read_block(blocks[(save_pos-1)/64],OFT[0].p);
			break;
		}
    }
	return file_placement_index; // cannot find file name
}


int open(char *s){ //takes a filename and opens up an already created file +
                   //tries to find a free OFT entry and deliver the first block, position, etc.
	int file_descriptor_index = get_specific_file_descriptor_index(s);
	if (file_descriptor_index == -1){
        fprintf(OUTPUT,"error");
		return -1; //cannot open file name because cannot find file
	}
	for(int i = 1; i<4; i++){
		if (OFT[i].index == file_descriptor_index){
			fprintf(OUTPUT,"error");
			return -1;
		}
		if (OFT[i].index == -1){
			OFT[i].index = file_descriptor_index;
			OFT[i].position = 0;
			int blocks[3];
			grab_slot_block_numbers(file_descriptor_index,blocks);
			int first_block = blocks[0];
			if (first_block == 0){
				first_block = free_bitmap_block();
				nondirectory_data_block(first_block,file_descriptor_index);
				read_block(first_block,OFT[i].p);
			}
			else{
				read_block(first_block,OFT[i].p);
			}
			OFT[i].length = grab_slot_block_length(file_descriptor_index);

            fprintf(OUTPUT,"%s opened %d",s,i);
			return i;		
		}
	}
    fprintf(OUTPUT,"error");
	return -2;
}


int save_file(char *s){ //closes all files in OFT, writes blocks from first_seven_blocks to ldisk, and stores values in text file
    for (int i = 0; i <4; i++){
        close_file(i);
    }
    for (int i = 0; i<7; i++){
        write_block(i,first_seven_blocks[i]);
    }
    SAVE = fopen("disk.txt","w");
    if (SAVE == NULL){
        fprintf(OUTPUT,"error");
        return -1; //error opening file
    }

    for(int i = 0; i < 64; i++){
        unsigned char temp[64];
        read_block(i,temp);
        for (int j = 0; j <64; j++){
            fprintf(SAVE,"%hhu ",temp[j]);
        }
        fprintf(SAVE,"\n");
    }
    fprintf(OUTPUT,"disk saved");
    fclose(SAVE);
    return 0;
}


void init(char *init_string){ //initializes the disk, should import a saved file or new if no file is found
    for (int i = 0; i < 64; i++){
        ldisk[i] = (unsigned char*)malloc(64*sizeof(unsigned char));   
    }
    get_first_seven();
    for(int i = 0; i <7;i++){
        allocate_bitmap(i);
    }
    for (int i = 0; i<4;i++){
        OFT[i].p = (unsigned char*)malloc(64*sizeof(unsigned char));
        OFT[i].position = 0;
        OFT[i].length = 0;
        if (i>0){
            OFT[i].index = -1;
        }
        else{
            OFT[i].index = 0;
        }
    }
    if (strlen(init_string) > 0){
        INIT = fopen("disk.txt","r");
        if (INIT == NULL){
            return; //incorrect open, initialize to basic 
        }
        for (int i = 0; i<64; i++){
            unsigned char * temp = (unsigned char *)malloc(64*sizeof(unsigned char));
            for (int j = 0; j<64; j++){
                fscanf(INIT,"%u",&temp[j]);
            }
            write_block(i,temp);
            if (i<7){
                read_block(i,first_seven_blocks[i]);
            }
            free(temp);
        }
        read_block(7,OFT[0].p);
        OFT[0].length = grab_slot_block_length(0);
        fprintf(OUTPUT,"disk restored");
        fclose(INIT);
    }
    else{
        fprintf(OUTPUT,"disk initialized");
    }
}


void directory(){ //returns list of files
    int save_pos = OFT[0].position;
    int blocks[3];
    grab_slot_block_numbers(0,blocks);
    write_block(blocks[(save_pos-1)/64],OFT[0].p);
    lseek(0,0);
    for (int i=0; i<24; i++){
        unsigned char temp[8];
        char file_name[4];
        int read_status = read_dir_file(0,temp,8);
        if (read_status == -1){
            OFT[0].position = save_pos;
            read_block(blocks[(save_pos-1)/64],OFT[0].p);
            break;
        }
        file_name[0] = temp[0];
        file_name[1] = temp[1];
        file_name[2] = temp[2];
        file_name[3] = temp[3];
        int file_placement_index = (uint32_t)temp[4] << 24 |
          (uint32_t)temp[5] << 16 |
          (uint32_t)temp[6] << 8  |
          (uint32_t)temp[7]; 
        if (file_placement_index != -1 && file_placement_index != 0){
            fprintf(OUTPUT,"%s\n",file_name);
        }
    }
}


int main(){
    INPUT = fopen("input.txt","r");
    if(INPUT == NULL){
        return -1; //issue grabbing command input to parse through
    }
    OUTPUT = fopen("output.txt","w");
    if (OUTPUT == NULL){
        return -2; //issue setting up OUTPUT.txt to write project results
    }
    char line[1000];
    while (fgets(line,1000,INPUT)!=NULL){
        char command[50];
        sscanf(line,"%s",&command);
        if (strlen(line) == 1){
            fprintf(OUTPUT,"\n");
        }

        else if (strcmp(command,"in")==0){
            char str[50];
            sscanf(line,"%s %s",&command,&str);
            init(str);
            fprintf(OUTPUT,"\n");
        }
        else if (strcmp(command,"cr")==0){
            char str[50];
            sscanf(line,"%s %s",&command,&str);
            create(str);
            fprintf(OUTPUT,"\n");
        }
        else if (strcmp(command,"op")==0){
            char str[50];
            sscanf(line,"%s %s",&command,&str);
            open(str);
            fprintf(OUTPUT,"\n");
        }
        
        else if (strcmp(command,"wr")==0){
            char c;
            int int1,int2;
            sscanf(line,"%s %d %c %d",&command,&int1,&c,&int2);
            write_file(int1,c,int2);
            fprintf(OUTPUT,"\n");
        }
        else if (strcmp(command,"sk")==0){
            int int1,int2;
            sscanf(line,"%s %d %d",&command,&int1,&int2);
            int seek = lseek(int1,int2);
            if (seek < 0){
                fprintf(OUTPUT,"error");
            }
            else{
                fprintf(OUTPUT,"position is %d",seek);
            }
            fprintf(OUTPUT,"\n");
        }
        else if (strcmp(command,"rd") ==0){
            int int1,int2;
            sscanf(line,"%s %d %d",&command,&int1,&int2);
            read_file(int1,int2);
            fprintf(OUTPUT,"\n");
        }
        else if (strcmp(command,"dr") ==0){
            directory();
        }
        else if (strcmp(command,"sv") ==0){
            char str[50];
            sscanf(line,"%s %s",&command,&str);
            save_file(str);
            fprintf(OUTPUT,"\n");
        }
        else if (strcmp(command,"cl") ==0){
            int int1;
            sscanf(line,"%s %d",&command,&int1);
            int cl = close_file(int1);
            if (cl < 0){
                fprintf(OUTPUT,"error");
            }
            else{
                fprintf(OUTPUT,"%d closed",cl);
            }
            fprintf(OUTPUT,"\n");
        }
        else if (strcmp(command,"de") ==0){
            char str[50];
            sscanf(line,"%s %s",&command,&str);
            destroy(str);
            fprintf(OUTPUT,"\n");
        }
    }
    fclose(OUTPUT);
    fclose(INPUT);
    return 0;
}