/*
 * Dusty Mabe - 2014
 * CCSM.h - Header file for Cache Coherence State Machine for
 *          the MESI protocol.
 */
#ifndef CCSM_H
#define CCSM_H

#include "types.h"

class Cache;     // Forward Declaration
class CacheLine; // Forward Declaration
class Tile;      // Forward Declaration

class CCSM {
    private:
    public:
        int state;
        Cache * cache;
        CacheLine * line;
        Tile * tile;

        CCSM(Tile *t, Cache *c, CacheLine *l); 
        ~CCSM();
        void setState(int s);
        void evict();
        void getFromNetwork(ulong msg);
        void netInitInv();
        void netInitInt();
        void procInitRd(ulong addr);
        void procInitWr(ulong addr);
};

#endif
