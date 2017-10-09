#include <string.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <bitset>
using namespace std;
#define DPRINT if(0) fprintf

#include "mmap_regions.h"

// Let's optimize for fast checking and easy code!
#define PAGE_SIZE 4096
bitset<0xc0000> ro_pages;

void init_mmap_region ()
{
    char filename[256];
    sprintf (filename, "/proc/%d/maps", getpid());
    ifstream in(filename, ios::in);
    if (!in.is_open()) {
        cerr << "cannot open " << filename <<endl;
        return;
    }
    string line;
    while (!in.eof()) { 
        if (in.fail() || in.bad()) assert (0);
        getline (in, line);
        if (line.empty()) continue;
        //cerr <<line<<endl;
        u_long start = std::stoul(line.substr(0, 8), 0, 16);
        u_long end = stoul(line.substr(9, 8), 0, 16);
        string flag_str = line.substr (18, 4);

        int flag = PROT_NONE;
        if (flag_str[0] == 'r') flag |= PROT_READ;
        if (flag_str[1] == 'w') flag |= PROT_WRITE;
        if (flag_str[2] == 'x') flag |= PROT_EXEC;
        if (flag == PROT_NONE) continue;
        add_mmap_region (start, end-start, flag, (flag_str[3] == 'p'?MAP_PRIVATE:MAP_SHARED));
        DPRINT (stderr, "init_mmap_region: start %lx end %lx, length %ld, prot %x, flag %x\n", start, end, end-start, flag, (flag_str[3] == 'p'?MAP_PRIVATE:MAP_SHARED));
    }
}

void add_mmap_region (u_long addr, int len, int prot, int flags) 
{ 
    bool val = (prot & PROT_READ) && !(prot & PROT_WRITE) && (flags & MAP_PRIVATE);
    DPRINT (stderr, "add mmap region from %lx to %lx read only? %d\n", addr, addr+len, val);
    for (auto i = addr; i < addr+len; i += PAGE_SIZE) {
	ro_pages.set(i/PAGE_SIZE, val);
    }
}

void delete_mmap_region (u_long addr, int len) 
{
    DPRINT (stderr, "delete mmap region from %lx to %lx\n", addr, addr+len);
    for (auto i = addr; i < addr+len; i += PAGE_SIZE) {
	ro_pages.reset(i/PAGE_SIZE);
    }
}

void change_mmap_region (u_long addr, int len, int prot)
{
    bool val = !(prot&PROT_WRITE);
    DPRINT (stderr, "change prot mmap region from %lx to %lx read-only? %d\n", addr, addr+len, val);
    for (auto i = addr; i < addr+len; i += PAGE_SIZE) {
	ro_pages.set(i/PAGE_SIZE, val);
    }
}

bool is_readonly (u_long addr, int len) 
{
    for (u_int i = addr/PAGE_SIZE; i <= (addr+len-1)/PAGE_SIZE; i++) {
	if (!ro_pages.test(i)) return false;
    }
    return true;
}

//given a memory range, see if it's in a read-only region
bool is_readonly_mmap_region (u_long addr, int len, u_long& start, u_long& end) 
{
    u_int i;
    for (i = addr/PAGE_SIZE; i <= (addr+len-1)/PAGE_SIZE; i++) {
	if (!ro_pages.test(i)) {
	    DPRINT (stderr, "addr %lx len %d is not in a read-only region\n", addr, len);
	    return false;
	}
    }
    for (i++; i < 0xc00000 && ro_pages.test(i); i++);
    end = i*PAGE_SIZE;

    for (i = addr/PAGE_SIZE - 1; i >= 0 && ro_pages.test(i); i--);
    start = (i+1)*PAGE_SIZE;

    DPRINT (stderr, "addr %lx len %d is in a read-only region from %lx to %lx\n", addr, len, start, end);
    return true;
}

