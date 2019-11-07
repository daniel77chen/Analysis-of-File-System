//NAME: Daniel Chen,Winston Lau
//EMAIL: kim77chi@gmail.com,winstonlau99@gmail.com
//ID: 605006027,504934155

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "ext2_fs.h"

int i; 

#define TWELVE_BITS 4095  
#define EXT2_S_IFLNK 40960 //>> 13 //0xA000 shifted = 0b101
#define EXT2_S_IFREG 32768 //>> 13 //0x8000 shifted = 0b100
#define EXT2_S_IFDIR 16384 //>> 13 //0x4000 shifted = 0b010
#define BOOT_S 1024 
#define INODE_S 128
#define GDSCPTR_S 32

void calc_ft (char * ft, int mode) {
    //int mode = unshifted >> 13;
    if ((mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) *ft = 's';
    else if ((mode & EXT2_S_IFREG) == EXT2_S_IFREG) *ft = 'f';
    else if ((mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) *ft = 'd';
    else *ft = '?';
}

void pError (char * message, int code) {
    fprintf (stderr, "%s\n", message);
    exit(code);
}

void checkError (char * message, int returned_value, int ecode) {
    if (returned_value == -1) {
        fprintf (stderr, "%s\n", message);
        exit(ecode);
    }
}

int calc_off (int block_s, int block_num) {
    return BOOT_S + (block_s *(block_num -1));
}

void print_indirect_stuff(int starting_point, int inode_num, int level, int offset, int ll_block, int fd, int block_s) {
    if (level == 0) return;
    int inc;
    int num_ints = block_s / sizeof(int);
    int * array = malloc(num_ints*sizeof(int));

    pread(fd, array, num_ints*sizeof(int),BOOT_S+block_s*(starting_point-1));  
    for (inc = 0; inc < num_ints; inc++) {
        if (array[inc] == 0) continue; 
        printf("INDIRECT,%d,%d,%d,%d,%d\n", inode_num, level, offset+inc, ll_block, array[inc]);
        print_indirect_stuff(array[inc], inode_num, level-1, offset+inc, array[inc], fd, block_s); 
    }
}

int main(int argc, char ** argv) {
    if (argc != 2)
        pError("wrong number of args given",1);
  
  ///////////////SUPERBLOCK//////////////////////////
    int fd = open (argv[1], O_RDONLY);
    checkError ("error while opening file",fd,1);

    int return_val;

    struct ext2_super_block * super = malloc (sizeof (struct ext2_super_block));
    return_val = pread(fd, super, sizeof(struct ext2_super_block), BOOT_S);
    checkError("error while using pread. probably did not give an openable file as argument",return_val,1);

    int num_blocks = super->s_blocks_count;
    int num_inodes = super->s_inodes_count;
    int block_s = EXT2_MIN_BLOCK_SIZE << super->s_log_block_size;
    int inode_s = super->s_inode_size;
    int bpg = super->s_blocks_per_group;
    int ipg = super->s_inodes_per_group;
    int ff_inode = super->s_first_ino;

    printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", num_blocks, num_inodes, block_s, inode_s, bpg, ipg, ff_inode);
    fflush(stdout);
    ///////////////GROUP//////////////////////////
    int b_TnT = num_blocks % bpg;
    int num_groups = ( num_blocks/bpg);
    if (b_TnT)
        num_groups ++;  
    struct ext2_group_desc ** grp = malloc(num_groups*sizeof(struct ext2_group_desc*));


    for (i = 0 ; i < num_groups-1 ; i ++) {
        grp[i] = malloc(sizeof(struct ext2_group_desc));
        return_val = pread(fd, grp[i], sizeof(struct ext2_group_desc), BOOT_S + block_s + (GDSCPTR_S*i) );
        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",i,bpg,ipg, grp[i]->bg_free_blocks_count,
             grp[i]->bg_free_inodes_count,grp[i]->bg_block_bitmap,grp[i]->bg_inode_bitmap,
             grp[i]->bg_inode_table);
        fflush(stdout);
    }
    int last_num_blocks = num_blocks - (bpg * (num_groups -1));
    int last_num_inodes = num_inodes - (ipg * (num_groups -1));
    grp[i] = malloc(sizeof(struct ext2_group_desc));
    return_val = pread(fd, grp[i], sizeof(struct ext2_group_desc), BOOT_S + block_s + (GDSCPTR_S*i) );
    checkError("error while using pread. probably did not give an openable file as argument",return_val,1);

    printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",i,last_num_blocks,last_num_inodes,grp[i]->bg_free_blocks_count,
            grp[i]->bg_free_inodes_count,grp[i]->bg_block_bitmap,grp[i]->bg_inode_bitmap,
            grp[i]->bg_inode_table);
    fflush(stdout);

    ///////////////FREE BLOCK ENTRIES//////////////////////////
    int j,k;
    int ** b_bitmap = malloc (num_groups*sizeof(int*));
    int bytes_needed = bpg;
    if (last_num_blocks % 8 != 0) 
        bytes_needed ++;
    int ints_needed = bytes_needed / sizeof(int);
    if (bytes_needed % sizeof(int) != 0) 
        ints_needed ++;   
    for (i = 0; i < num_groups - 1 ; i++) {
        b_bitmap[i] = malloc (bytes_needed);
        pread(fd,b_bitmap[i],bytes_needed, calc_off(block_s, grp[i]->bg_block_bitmap));
        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        for (j = 0; j < ints_needed; j++) {
            for (k = 0; k < (int) sizeof(int) * 8; k++) { 
                if(!((b_bitmap[i][j] >> k) & 1)) {
                    printf("BFREE,%d\n", j*32 + k + 1);
                    fflush(stdout);
                }
            }
        }
    }
    bytes_needed = last_num_blocks / 8;
    if (last_num_blocks % 8 != 0) 
        bytes_needed ++;
    ints_needed = bytes_needed / sizeof(int);
    if (bytes_needed % sizeof(int) != 0) 
        ints_needed ++;    
    b_bitmap[i] = malloc (bytes_needed);
    pread(fd, b_bitmap[i],bytes_needed, calc_off(block_s, grp[i]->bg_block_bitmap));
    checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
    for (j = 0; j < ints_needed; j++) {
        for (k = 0; k < (int) sizeof(int) * 8; k++) { 
            if(!((b_bitmap[i][j] >> k) & 1)) {
            printf("BFREE,%d\n", j*32 + k + 1 );
            fflush(stdout);
            }
        }
    }
        
    ///////////////FREE INODE ENTRIES//////////////////////////
    
char ** i_bitmap = malloc (num_groups*sizeof(char*));
    
    bytes_needed = ipg / 8;
    if ( ipg % 8 != 0) 
        bytes_needed ++;
    int chars_needed = ipg / sizeof(char);
    if (bytes_needed % sizeof(char) != 0) 
        chars_needed ++;   
    for (i = 0; i < num_groups - 1 ; i++) {
        i_bitmap[i] = malloc (bytes_needed);
        pread(fd,i_bitmap[i],bytes_needed, calc_off(block_s, grp[i]->bg_inode_bitmap));
        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        for (j = 0; j < chars_needed; j++) {
            for (k = 0; k < (int) sizeof(char) * 8; k++) { 
                if(!((i_bitmap[i][j] >> k) & 1)) {
                    printf("IFREE,%d\n", j*8 + k +1);
                    fflush(stdout);
                }
            }
        }
    }
    bytes_needed = last_num_inodes / 8;
    if (last_num_inodes % 8 != 0) 
        bytes_needed ++;
    chars_needed = bytes_needed / sizeof(char);
    if (bytes_needed % sizeof(char) != 0) 
        chars_needed ++;    
    i_bitmap[i] = malloc (bytes_needed);
    pread(fd, i_bitmap[i],bytes_needed, calc_off(block_s, grp[i]->bg_inode_bitmap));
    checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
    
    for (j = 0; j < chars_needed; j++) {
        for (k = 0; k < (int) sizeof(char) * 8; k++) { 
            if(!((i_bitmap[i][j] >> k) & 1)) {
            printf("IFREE,%d\n", j*8 + k +1);
            fflush(stdout);
            }
        }
    }

    ///////////////I-NODE SUMMARY//////////////////////////

int i_TnT = num_inodes % ipg;
int inum_groups = ( num_inodes/ipg);
if (i_TnT)
    inum_groups ++;  
if (inum_groups != num_groups) 
pError ("num groups calculated from inodes doesnt match that calculated from blocks. Exiting TnT",2);

struct ext2_inode *** tinodes = malloc(num_groups*sizeof(struct ext2_inode**));
struct tm ctime_data, mtime_data, atime_data; 
char ft;
time_t time_c, time_m, time_a;
for (i = 0 ; i < num_groups-1 ; i ++) {
    tinodes[i] = malloc(ipg*sizeof(struct ext2_inode*));
    for (j = 0; j < ipg; j++) {
        tinodes[i][j] = malloc(sizeof(struct ext2_inode));
        return_val = pread(fd, tinodes[i], sizeof(struct ext2_inode), 
        BOOT_S + block_s * (grp[i]->bg_inode_table -1)+ 128*j);
        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        if (tinodes[i][j]->i_mode == 0 || tinodes[i][j]->i_links_count == 0)
            continue;
        calc_ft (&ft, tinodes[i][j]->i_mode);
        time_a = (time_t) tinodes[i][j]->i_atime;
        time_c = (time_t) tinodes[i][j]->i_ctime;
        time_m = (time_t) tinodes[i][j]->i_mtime;
        const time_t * ctimer = &time_a;
        const time_t * mtimer = &time_c;
        const time_t * atimer = &time_m;
        ctime_data = * gmtime(ctimer);
        mtime_data = * gmtime(mtimer);  
        atime_data = * gmtime(atimer);  
        printf("INODE,%d,%c,%o,%d,%d,%d,%02d/%02d/%02d %02d:%02d:%02d,\
%02d/%02d/%02d %02d:%02d:%02d,%02d/%02d/%02d %02d:%02d:%02d,%d,%d",j+1,ft,
                tinodes[i][j]->i_mode & TWELVE_BITS, tinodes[i][j]->i_uid,
                tinodes[i][j]->i_gid,tinodes[i][j]->i_links_count,
                ctime_data.tm_mon+1, ctime_data.tm_mday, ctime_data.tm_year, 
                ctime_data.tm_hour, ctime_data.tm_min, ctime_data.tm_sec, 
                mtime_data.tm_mon+1, mtime_data.tm_mday, mtime_data.tm_year, 
                mtime_data.tm_hour, mtime_data.tm_min, mtime_data.tm_sec, 
                atime_data.tm_mon+1, atime_data.tm_mday, atime_data.tm_year, 
                atime_data.tm_hour, atime_data.tm_min, atime_data.tm_sec, 
                tinodes[i][j]->i_size,tinodes[i][j]->i_blocks);
        fflush(stdout);
        if (ft == 's' && tinodes[i][j]->i_size < 60) {
            printf("\n"); continue;
        }
        for (k = 0; k < 15; k++) {
            printf(",%d", (tinodes[i][j]->i_block)[k]);
        }
        printf("\n");
        //////////////////////// CHECKING DIRECTORY ///////////////////////
       
    } 
}

tinodes[i] = malloc(last_num_inodes*sizeof(struct ext2_inode*));
for (j = 0; j < last_num_inodes; j++) {
    tinodes[i][j] = malloc(sizeof(struct ext2_inode));
    return_val = pread(fd, tinodes[i][j], sizeof(struct ext2_inode), 
    BOOT_S + block_s * (grp[i]->bg_inode_table -1)+ 128*j);
    checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
    if (tinodes[i][j]->i_mode == 0 || tinodes[i][j]->i_links_count == 0)
        continue;
    calc_ft (&ft, tinodes[i][j]->i_mode);
    time_a = (time_t) tinodes[i][j]->i_atime;
    time_c = (time_t) tinodes[i][j]->i_ctime;
    time_m = (time_t) tinodes[i][j]->i_mtime;
    const time_t * ctimer = &time_a;
    const time_t * mtimer = &time_c;
    const time_t * atimer = &time_m;
    ctime_data = * gmtime(ctimer);
    mtime_data = * gmtime(mtimer);   
    atime_data = * gmtime(atimer);  
    printf("INODE,%d,%c,%o,%d,%d,%d,%02d/%02d/%02d %02d:%02d:%02d,\
%02d/%02d/%02d %02d:%02d:%02d,%02d/%02d/%02d %02d:%02d:%02d,%d,%d",j+1,ft,
            tinodes[i][j]->i_mode & TWELVE_BITS, tinodes[i][j]->i_uid,
            tinodes[i][j]->i_gid,tinodes[i][j]->i_links_count,
            ctime_data.tm_mon+1, ctime_data.tm_mday, ctime_data.tm_year, 
            ctime_data.tm_hour, ctime_data.tm_min, ctime_data.tm_sec, 
            mtime_data.tm_mon+1, mtime_data.tm_mday, mtime_data.tm_year, 
            mtime_data.tm_hour, mtime_data.tm_min, mtime_data.tm_sec, 
            atime_data.tm_mon+1, atime_data.tm_mday, atime_data.tm_year, 
            atime_data.tm_hour, atime_data.tm_min, atime_data.tm_sec, 
            tinodes[i][j]->i_size,tinodes[i][j]->i_blocks);
    fflush(stdout);
    if (ft == 's' && tinodes[i][j]->i_size < 60) {
        printf("\n"); continue;
    }
    for (k = 0; k < 15; k++) 
        printf(",%d",(tinodes[i][j]->i_block)[k]);
    printf("\n");

    //////////////////////////// INDIRECT /////////////////////////////
    if(ft == 'f' || ft == 'd') {
        k = 12; 
        print_indirect_stuff((tinodes[i][j]->i_block)[k], j+1, 1, k, (tinodes[i][j]->i_block)[k], fd, block_s);
        k++; 
        print_indirect_stuff((tinodes[i][j]->i_block)[k], j+1, 2, 268, (tinodes[i][j]->i_block)[k], fd, block_s);
        k++; 
        print_indirect_stuff((tinodes[i][j]->i_block)[k], j+1, 3, 65804, (tinodes[i][j]->i_block)[k], fd, block_s);

    }
    //////////////////////// CHECKING DIRECTORY ///////////////////////
    if (ft != 'd') continue; 
    int current_offset = 0;
    int counter;
    struct ext2_dir_entry ** de_table = malloc(15*sizeof(struct ext2_dir_entry*));
    for (k = 0; k < 12; k++) {
        if((tinodes[i][j]->i_block)[k] == 0) continue; 
        de_table[k] = malloc(sizeof(struct ext2_dir_entry));            
        for(counter = 0; current_offset < block_s; counter++) {
            de_table[k] = realloc(de_table[k],(counter+1)*sizeof(struct ext2_dir_entry));            
            return_val = pread(fd,&(de_table[k][counter]),sizeof(struct ext2_dir_entry), BOOT_S + block_s*((tinodes[i][j]->i_block)[k]-1)+current_offset);
            checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
            if (de_table[k][counter].name_len == 0 || de_table[k][counter].inode == 0) {
                break;
            }
            printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",j+1,current_offset,de_table[k][counter].inode,de_table[k][counter].rec_len,
            de_table[k][counter].name_len,de_table[k][counter].name);
            current_offset += de_table[k][counter].rec_len;
        }
        current_offset = 0;
    }
        //k is now 12
    //////////////13/////////////////
    k = 12; 
    int b; counter = 0;
    if ((tinodes[i][j]->i_block)[k] != 0 ) {
        int block_13_arr[256];
        return_val = pread(fd,block_13_arr,sizeof(int)*256, BOOT_S + block_s*((tinodes[i][j]->i_block)[k]-1));
        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        de_table[k] = malloc(sizeof(struct ext2_dir_entry));
        for(b = 0 ; b < 256; b++) {
            if(block_13_arr[b] == 0) continue; 
            current_offset = 0;
            while(current_offset < block_s) {
                de_table[k] = realloc(de_table[k],(counter+1)*sizeof(struct ext2_dir_entry));   
                return_val = pread(fd,&(de_table[k][counter]),sizeof(struct ext2_dir_entry), BOOT_S + block_s*(block_13_arr[b]-1)+current_offset);
                checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
                if (de_table[k][counter].name_len == 0 && de_table[k][counter].inode == 0) break;
                printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",j+1,current_offset,de_table[k][counter].inode,de_table[k][counter].rec_len,
                de_table[k][counter].name_len,de_table[k][counter].name);
                current_offset += de_table[k][counter].rec_len;
                counter++;
            }
        }
    }
    k++;        //k is now 13
    //////////////14/////////////////
    int w; counter = 0; 
    if( (tinodes[i][j]->i_block)[k] != 0) { 
        int block_14_arr[256];
        return_val = pread(fd,block_14_arr,256*sizeof(int), BOOT_S + block_s*((tinodes[i][j]->i_block)[k]-1));
        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        int block_14_arr2[256][256];
        for ( w = 0; w < 256; w++) {
            return_val = pread(fd, & block_14_arr2[w],256*sizeof(int), BOOT_S + block_s*(block_14_arr[w]-1));
            checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        }
        de_table[k] = malloc(sizeof(struct ext2_dir_entry));
        for(w = 0; w < 256; w++) {
            for(b = 0; b < 256; b++) {
                if (block_14_arr2[w][b] == 0) continue;
                current_offset = 0;
                while(current_offset < block_s) {
                    de_table[k] = realloc(de_table[k],(counter+1)*sizeof(struct ext2_dir_entry)); 
                    return_val = pread(fd,&(de_table[k][counter]),sizeof(struct ext2_dir_entry), BOOT_S + block_s*(block_14_arr2[w][b]-1)+current_offset);
                    checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
                    if (de_table[k][counter].name_len == 0 || de_table[k][counter].inode == 0) break;
                    printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",j+1,current_offset,de_table[k][counter].inode,de_table[k][counter].rec_len,
                    de_table[k][counter].name_len,de_table[k][counter].name);
                    current_offset += de_table[k][counter].rec_len;
                    counter++;
                }
            }
        }
    }
    k++;        //k is now 14
    // //////////////15/////////////////
    int d; counter = 0;
    if( (tinodes[i][j]->i_block)[k] != 0) {
        int block_15_arr[256];
        return_val = pread(fd,block_15_arr,256*sizeof(int), BOOT_S + block_s*((tinodes[i][j]->i_block)[k]-1));
        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        int block_15_arr2[256][256];
        for ( w = 0; w < 256; w++) {
        if (block_15_arr[w] == 0) { continue; }
            return_val = pread(fd, & block_15_arr2[w],256*sizeof(int), BOOT_S + block_s*(block_15_arr[w]-1));
            checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
        }
        // creating 3D array of int pointers ///////////////////////////
        int *** block_15_arr3 = malloc(256*sizeof(int**));
        for (d = 0; d < 256; d++) {
            block_15_arr3[d] = malloc(256*sizeof(int*)); 
            for (w = 0; w < 256; w++) {
                block_15_arr3[d][w] = malloc(256*sizeof(int)); 
            }
        }
        /////////////////////////////////////
        for (d = 0; d < 256; d++) {
            for (w = 0; w < 256; w++) {
            if(block_15_arr2[d][w] == 0) { continue;}  
                return_val = pread (fd, &block_15_arr3[d][w], 256*sizeof(int), BOOT_S + block_s*(block_15_arr2[d][w]-1)); 
                checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
            }
        }

        de_table[k] = malloc(sizeof(struct ext2_dir_entry));
        for(d = 0; d < 256; d++) {
            for(w = 0; w < 256; w++) {
                for(b = 0; b < 256; b++) {
                    current_offset = 0;
                if (block_15_arr3[d][w][b] == 0) continue;
                    while(current_offset < block_s) {
                        de_table[k] = realloc(de_table[k],(counter+1)*sizeof(struct ext2_dir_entry)); 
                        return_val = pread(fd,&(de_table[k][counter]),sizeof(struct ext2_dir_entry), BOOT_S + block_s*(block_15_arr3[d][w][b]-1)+current_offset);
                        checkError("error while using pread. probably did not give an openable file as argument",return_val,1);
                        if (de_table[k][counter].name_len == 0 || de_table[k][counter].inode == 0) break;
                        printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",j+1,current_offset,de_table[k][counter].inode,de_table[k][counter].rec_len,
                        de_table[k][counter].name_len,de_table[k][counter].name);
                        current_offset += de_table[k][counter].rec_len;
                        counter++;
                    }
                }
            }
        }
        /////////////////// Freeing Memory ////////////////////////
        for (d = 0; d < 256; d++) {
            for (w = 0; w < 256; w++) {
                free(block_15_arr3[d][w]);
            }   
            free(block_15_arr3[d]); 
        }    
        free(block_15_arr3); 
    }
}



////////////////////////// FREEING MEMORY ////////////////////////////
for (i = 0 ; i < num_groups; i++) {
    free(grp[i]);
    free(b_bitmap[i]); 
    free(i_bitmap[i]); 
}
free(b_bitmap); 
free(i_bitmap); 
free(grp);
free(super);
    
return 0;

}