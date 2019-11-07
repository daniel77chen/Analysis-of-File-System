#! /usr/local/cs/bin/python3


import sys
import csv

#0 ... successful execution, no inconsistencies found.
#1 ... unsuccessful execution, bad parameters or system call failure.
#2 ... successful execution, inconsistencies found.

############################ CONSTANTS and MAPS ##############################
csvrows = []      
inode_free_bitmap = []   
block_free_bitmap = []
parent = {}
d_link_counts = {}
link_counts = {}
inode_state = {}   # mapping inode number to the state
block_state = {}   # mapping block number to the state
level_string = ["INDIRECT ","DOUBLE INDIRECT ","TRIPLE INDIRECT "]

num_groups = 0
num_free_blocks = 0        #in this group but there's only one lol
num_free_inodes = 0        #in this group,same as above 
total_num_blocks = 0
total_num_inodes = 0
block_size = 0
inode_size = 0
inode_table_size = 0
first_legal_block = 0
first_free_inode = 0
blockspg = 0
inodespg = 0
max_block = 0
exitCode = 0

############################ Helper Functions ################################ 

def pError(message, exitCode = 1): 
    print (message, file = sys.stderr, flush=True)
    sys.exit(exitCode)

# not sure if this works lol, if it does, we can change inode_audit to use this state dictionary
def init_inode_state():
    for i in range(1,total_num_inodes):
        if(i not in inode_state):
            inode_state[i] = 0
        for row in csvrows:
            if (row[0] == "IFREE" and i == int(row[1])): 
                if (inode_state[i] == 0):
                    inode_state[i] = "free"
            elif (row[0] == "INODE" and i == int(row[1]) and row[2] != 0):
                inode_state[i] = "used"

def init_block_state():
    for b in range(1,total_num_blocks):
        if (b not in block_state):
            block_state[b] = 0
        for row in csvrows:
            if (row[0]  == "BFREE" and b == int(row[1])): 
                if (block_state[b] == 0):
                    block_state[b] = "free"
            elif ((row[0]  == "INODE" and str(b) in row[12:27]) or
                (row[0] == "INDIRECT" and b == int(row[5]))):   
                if (block_state[b] == 0 or block_state[b] == "free"):
                    block_state[b] = "used"
                elif(block_state[b] == "used"):
                    block_state[b] = "duplicate" 

def set_super_constants (super_list):
    global total_num_blocks
    global total_num_inodes
    global block_size
    global inode_size
    global blockspg
    global inodespg
    global first_free_inode 
    global num_groups
    global inode_table_size
    global first_legal_block
    total_num_blocks = int(super_list[1])
    total_num_inodes = int(super_list[2])
    block_size = int(super_list[3])
    inode_size = int(super_list[4])
    blockspg = int(super_list[5])
    inodespg = int(super_list[6])
    first_free_inode = int(super_list[7])
    num_groups = (total_num_blocks / blockspg) + 1
    inode_table_size = (total_num_inodes * inode_size // block_size) + 1
    first_legal_block = 4 + inode_table_size

def set_group_constants (group_list):
    global num_free_blocks
    global num_free_inodes
    num_free_blocks = int(group_list[4])
    num_free_inodes = int(group_list[5])

def setup_link(inode_num, num_links):
    if (inode_num in link_counts):
        pError("somehow there are two (different?) inodes with the same inode number",2) #idk
    else: 
        link_counts[inode_num] = num_links

def inode_audit (inode_num):
    free = 0           #1 if in our free bitmap
    allocated = 0      #1 if is allocated (type != 0)
    if (inode_state[inode_num] == "used"):
        allocated = 1
    if (inode_num in inode_free_bitmap):
        free = 1
    #print(inode_num, "allocated:", allocated, "free:", free)
    #if allocated but also free, then its an inconsistency
    #if not allocated but not free, then it is an inconsistency
    if (allocated  == 1 and free  == 1):
        print("ALLOCATED INODE", inode_num, "ON FREELIST", sep = " ",
        flush = True)
        sys.exit(2)
    if (allocated == 0 and free == 0):
        print("UNALLOCATED INODE", inode_num, "NOT ON FREELIST", sep = " ",
        flush = True)
        sys.exit(2)

def calc_offset(block_num):
    if (block_num == 24):
        return 0     
    elif (block_num == 25): 
        return 255
    else:
        return 255 + 256**2 - 1

def block_audit(row):
    global exitCode
    if (row[0]  == "INODE"):
        if (row[2]  == 's' and int(row[10]) < 60):
            return
        for b in range(12,27):
            if (int(row[b]) == 0): 
                continue 
            if (not is_valid_block(int(row[b]))):
                levelstr = ""
                offset = 0
                if (b >= 24):
                    levelstr = level_string[b-24]
                    offset = calc_offset(b) 
                print("INVALID ", levelstr, "BLOCK ", int(row[b]), " IN INODE ", int(row[1]), " AT OFFSET ", b-12+offset, sep="", flush = True)
                sys.exit(2)
            elif (not is_not_reserved_block(int(row[b]))):
                levelstr = ""
                offset = 0
                if (b >= 24):
                    levelstr = level_string[b-24]
                    offset = calc_offset(b)
                print("RESERVED ", levelstr,  "BLOCK ", int(row[b]), " IN INODE ", int(row[1]), " AT OFFSET ", b-12+offset, sep= "", flush = True)
                sys.exit(2)
            elif (int(row[b]) in block_state and (block_state[int(row[b])] == "used" or block_state[int(row[b])] == "duplicate") 
            and int(row[b]) in block_free_bitmap):
                print("ALLOCATED BLOCK", int(row[b]), "ON FREELIST", sep= " ", flush = True)                
                sys.exit(2)
            elif (int(row[b]) in block_state and block_state[int(row[b])] == "duplicate"):
                levelstr = ""
                offset = 0
                if (b >= 24):
                    levelstr = level_string[b-24]
                    offset = calc_offset(b)
                print("DUPLICATE ", levelstr, "BLOCK ", int(row[b]), " IN INODE ", int(row[1]), " AT OFFSET ", b-12+offset, sep= "", flush = True)
                exitCode = 2
        
    elif (row[0]  == "INDIRECT"): 
        level = int(row[2])
        if (not is_valid_block(int(row[5]))):
            print("INVALID ", level_string[level-1], "BLOCK ", int(row[5]), " IN INODE ", int(row[1]), " AT OFFSET ", int(row[3]), sep="", flush=True)
            sys.exit(2)
        elif (not is_not_reserved_block(int(row[5]))):
            print("RESERVED ", level_string[level-1], "BLOCK ", int(row[5]), " IN INODE ", int(row[1]), " AT OFFSET ", int(row[3]),sep="", flush=True)
            sys.exit(2)
        elif (int(row[5]) in block_state and block_state[int(row[5])] == "duplicate"):
            print("DUPLICATE ",level_string[level-1],"BLOCK ",int(row[5])," IN INODE ",int(row[1])," AT OFFSET ",int(row[3]), sep = "", flush = True)
            exitCode = 2

def dirent_audit(key):
    d_link_count = 0
    if (key in d_link_counts):
        d_link_count = d_link_counts[key]
    if (link_counts[key] != d_link_count):
        print("INODE", key, "HAS", d_link_count, "LINKS BUT LINKCOUNT IS", link_counts[key], sep=" ", flush = True)
        sys.exit(2)
    
def check_invalid(file_inode_num, parent_inode, name): 
    if (file_inode_num < 1 or file_inode_num > total_num_inodes):
        print("DIRECTORY INODE", parent_inode, "NAME", name, "INVALID INODE", file_inode_num, sep=" ", flush= True)
        sys.exit(2)
    elif (inode_state[file_inode_num] == "free"):
        print("DIRECTORY INODE", parent_inode, "NAME", name, "UNALLOCATED INODE", file_inode_num, sep=" ", flush=True)
        sys.exit(2)

def check_dirent_inode(file_inode_num, parent_inode, name): 
    if (name == "'.'"):
        if (file_inode_num != parent_inode):
            print("DIRECTORY INODE", parent_inode, "NAME", name, "LINK TO INODE", file_inode_num, "SHOULD BE", parent_inode,
            sep=" ", flush=True)
            sys.exit(2)
    elif (name == "'..'"):
        if (parent[parent_inode] != file_inode_num):
            print("DIRECTORY INODE", parent_inode, "NAME", name, "LINK TO INODE", file_inode_num, "SHOULD BE", parent[parent_inode],
            sep=" ", flush=True)
            sys.exit(2) 

def update_count(file_inode_num):
    if (file_inode_num in d_link_counts):
        d_link_counts[file_inode_num] = d_link_counts[file_inode_num] + 1
    else:
        d_link_counts[file_inode_num] = 1

def addToBitmap (row):
    if (row[0] == "BFREE"):
        block_free_bitmap.append(int(row[1]))
    elif (row[0] == "IFREE"):
        inode_free_bitmap.append(int(row[1]))

def is_valid_block(block_num):
    return (
        block_num >= 0 and block_num < total_num_blocks)

def is_not_reserved_block(block_num):
    if (not is_valid_block(block_num)):
        return False
    else:
        return block_num >= first_legal_block

def is_not_reserved_inode(inode_num):
    if (inode_num > 2 and inode_num < first_free_inode or 
            inode_num > total_num_inodes):
        return False
    else: 
        return True
    
################################ Parsing ##################################### 
if __name__ == "__main__":
    if len(sys.argv) != 2:
        pError("did not pass the right number of variabes")
    file = sys.argv[1] 
    try:
        csvfile = open(file,newline='')            #making a file obj
        #close the file here
        csvreader = csv.reader(csvfile) #making csv obj from file obj
        for row in csvreader:       
            csvrows.append(row)        #rows is a list of list of strings, each 
                                    #element list is a row
                                    #total rows: csvreader.line_num
    except IOError :
        if (len(sys.argv[1]) >= 2 and sys.argv[1][0] == '-' and sys.argv[1][1] == '-'):
            pError("no arguments are allowed for this program")
        else:
            pError("unable to open file or convert file to csv object")

    for row in csvrows:
        if (row[0] == "SUPERBLOCK"):
            set_super_constants(row)
        elif (row[0] == "GROUP"):
            set_group_constants(row) 
        elif (row[0] =="BFREE" or row[0] == "IFREE"):
            addToBitmap(row)
        elif (row[0] == "DIRENT"):
            update_count(int(row[3]))
            if (row[6] != "'..'" and row[6] != "'.'"):
            #    print("key:", row[3], "parent:", row[1],"name:",row[6])
                parent[int(row[3])] = int(row[1])
    parent[2] = 2

    init_inode_state()
    init_block_state()

    for row in csvrows: 
        if (row[0] == "INODE"):
            setup_link(int(row[1]),int(row[6]))
            block_audit(row)
        elif (row[0] == "INDIRECT"):
            block_audit(row)
    if (exitCode == 2): 
        sys.exit(exitCode)

    # unreferenced ones
    for b in range(first_legal_block,total_num_blocks):
            if (b in block_state and block_state[b] == 0):
                print("UNREFERENCED BLOCK", b, sep = " ", flush = True)
                sys.exit(2)

    for i in range(2,total_num_inodes):
        if (is_not_reserved_inode(i)):
            inode_audit(i)

    for key in link_counts:
        dirent_audit(key)

    for row in csvrows: 
        if (row[0] == "DIRENT"): 
            check_invalid(int(row[3]),int(row[1]),row[6])
    for row in csvrows: 
        if (row[0] == "DIRENT"): 
            check_dirent_inode(int(row[3]),int(row[1]),row[6])

    sys.exit(exitCode)
