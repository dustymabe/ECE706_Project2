/*
 * Dusty Mabe - 2014
 * Tile.h - 
 */
#ifndef TILE_H
#define TILE_H

#include "types.h"

class Cache;     // Forward Declaration
class BitVector; // Forward Declaration


class Tile {
protected:

    Cache * l1cache;
    Cache * l2cache;

   
public:
    BitVector * part;
    unsigned int index;
    unsigned int partscheme;
    unsigned int xindex;
    unsigned int yindex;
    unsigned int cycle;
    unsigned int locxfer;
    unsigned int locdelay;
    unsigned int ctocxfer;
    unsigned int ctocdelay;
    unsigned int memxfer;
    unsigned int ptopxfer;
    unsigned int ptopdelay;
    unsigned int accesses;
    unsigned int l2accesses;
    unsigned int memcycles;
    unsigned int memhopscycles;
    unsigned int flushcycles;

    Tile(int number, int partspertile, int partition);
    ~Tile();
    void FlushDirtyBlocks();
    void Access(ulong addr, uchar op);
    void L2Access(ulong addr, uchar op);
    void L2Retrieve(ulong addr, uchar op);
    void PrintStats();
    void PrintStatsTabular(int printhead);

    void broadcastToPartition(ulong msg, ulong addr);
    int sendToNeighbor(ulong msg, ulong addr);
    int getFromNetwork(ulong msg, ulong addr, ulong fromtile);
    int mapAddrToTile(ulong addr);
};

#endif

