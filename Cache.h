/*
 * Dusty Mabe - 2014
 * cache.h - 
 */
#ifndef CACHE_H
#define CACHE_H

#include "types.h"

#define L1 0
#define L2 1

#define  HIT 0
#define MISS 1

class CacheLine; // Forward Declaration
class CCSM;      // Forward Declaration
class Tile;      // Forward Declaration  

class Cache {
protected:
    // Some parameters
    ulong size, lineSize, assoc;
    ulong numSets, tagMask, numLines;
    ulong indexbits, offsetbits, tagbits;
    ulong cacheLevel; //L1 or L2

    // Some counters
    ulong reads, readMisses;
    ulong writes, writeMisses;
    ulong flushes, writeBacks;
    ulong interventions, invalidations;
    ulong transfers;

    // The 2-dimensional cache
    CacheLine **cacheArray;

    // The tile the cache belongs to
    Tile * tile;

public:
    // Variable to keep up with global LRU
    ulong lruCounter;  
     
    Cache(Tile * t, int l, int s, int a, int b);
    ~Cache() { delete cacheArray;}

    CacheLine * fillLine(ulong addr);
    CacheLine * findLine(ulong addr);
    CacheLine * getLRU(ulong);

    void invalidateLineIfExists(ulong addr);

    ulong getRM()       { return readMisses;  }
    ulong getWM()       { return writeMisses; }
    ulong getReads()    { return reads;       }
    ulong getWrites()   { return writes;      }
    ulong getWB()       { return writeBacks;  }
    void writeBack()    { writeBacks++;       }

    ulong Access(ulong, uchar);
    void PrintStats();
    void PrintStatsTabular(int printhead); 
    void updateLRU(CacheLine *);

    ulong calcTag(ulong addr);
    ulong calcIndex(ulong addr);
    ulong getBaseAddr(ulong tag, ulong index);
};

#endif
