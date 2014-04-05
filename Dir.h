/*
 * Dusty Mabe - 2014
 * Dir.h - Header file for an implementation of a directory 
 *         that will sit at the memory controller and track usage
 *         across partitions within the chip.
 */
#ifndef DIR_H
#define DIR_H

#include "types.h"

class BitVector; // Forward Declaration

// Directory states
enum {
    DSTATEEM = 100,
    DSTATES,
    DSTATEI,
};

class DirEntry {

    public:
        ulong blockaddr;
        ulong state;
        ulong location;
        BitVector * sharers;

        DirEntry(ulong blockaddr);
        ~DirEntry();
};

class Dir {
    private:

        DirEntry  **directory;

        // Array of directory entires (1 for each mem block) each containing
        //  - bitvector representing which parts cache the block
        //  - M/S/I states

    public:
        BitVector **parttable; // Table of partitions.

        int numparts; // # of partitions in the system

        Dir(int partscheme);
        ~Dir();
        int mapAddrToTile(int partid, int blockaddr);
        int mapTileToPart(int tileid);
        int invalidateSharers(int addr, int partid);
        int interveneOwner(int addr);
        int findClosestSharer(int addr, int tile);
        void replyData(int addr, int fromtile, int totile);
        void setState(ulong blockaddr, int s);
        ulong getFromNetwork(ulong msg, ulong addr, ulong fromtile);
        void netInitRdX(ulong blockaddr, ulong partid);
        void netInitRd(ulong blockaddr, ulong partid);
        void netInitUpgr(ulong blockaddr, ulong partid);
};

#endif
