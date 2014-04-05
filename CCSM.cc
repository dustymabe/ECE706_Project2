/*
 * Dusty Mabe - 2014
 * CCSM.cc - Implementation of a MESI Cache Coherence State Machine (CCSM).
 */

#include <assert.h>
#include "CCSM.h"
#include "CacheLine.h"
#include "Cache.h"
#include "Tile.h"
#include "Dir.h"
#include "Net.h"

// Global NETWORK is defined in simulator.cc
extern Net *NETWORK;

enum{
    STATEM = 0,
    STATEE,
    STATES,
    STATEI,
};

CCSM::CCSM(Tile * t, Cache *c, CacheLine *l) {
    tile  = t;
    cache = c;
    line  = l;
    state = STATEI;
}

/*
 * CCSM:setState
 *     - This function serves to change the state of the CCSM
 *       to s. If we are transitioning to an invalid state then
 *       there is some housekeeping to do.
 */
void CCSM::setState(int s) {

    // If we are going to the invalid state there
    // are a few things to do. 
    if (state != STATEI && s == STATEI) {

        assert(line->isValid()); // line should be valid

        // Since L1 and L2 are inclusive and we are invalidating 
        // out of L2 (only have CCSM in L2) then broadcast 
        // invalidation to L1s in all Tiles in the partition. 
        tile->broadcastToPartition(
            L1INV,
            cache->getBaseAddr(line->getTag(), line->getIndex())
        );

        // If the the line is dirty then this is a writeback
        if (line->getFlags() == DIRTY)
            cache->writeBack();

        // set the cache line state to invalid
        line->invalidate();
    }

    state = s; // Set the new state
}

void CCSM::evict() {
    // On eviction set the state to invalid
    setState(STATEI);
}


void CCSM::netInitInv() {

    int addr = cache->getBaseAddr(line->getTag(), line->getIndex());

    switch (state) {

        // For M we need to transition to Invalid state and flush. 
        case STATEM: 
            NETWORK->flushToMem(addr, tile->index);
            setState(STATEI);
            break;

        // For E&S we need to transistion to Invalid state
        case STATEE: 
        case STATES: 
            setState(STATEI);
            break;

        // For invalid state should not happen
        case STATEI: 
            assert(0);
            break;

        default :
            assert(0); // should not get here
    }
}

void CCSM::netInitInt() {

    int addr = cache->getBaseAddr(line->getTag(), line->getIndex());

    switch (state) {

        // For M we need to transistion to Shared state and flush. 
        case STATEM: 
            NETWORK->flushToMem(addr, tile->index);
            setState(STATES);
            break;

        // For E we transition to Shared
        case STATEE:
            setState(STATES);
            break;
        
        // For S, no need to change state, but we can optionally flush
        case STATES: 
            break;

        // Nothing to do for I
        case STATEI: 
            break;

        default :
            assert(0); // should not get here
    }
}

void CCSM::procInitWr(ulong addr) {

    switch (state) {
        // Nothing to do for modified state
        case STATEM: 
            break;

        // For Exclusive just migrate to Modified state
        case STATEE:
            setState(STATEM);
            break;

        // For S need to send UPGR and go to modified
        case STATES: 
            NETWORK->sendReqTileToDir(UPGR, addr, tile->index);
            setState(STATEM);
            break;

        // For I need to send RDX and go to modified
        case STATEI: 
            NETWORK->sendReqTileToDir(RDX, addr, tile->index);
            setState(STATEM);
            break;

        default :
            assert(0); // should not get here
    }
}

void CCSM::procInitRd(ulong addr) {
    int dirstate;
    switch (state) {
        // Nothing to do for M&E&S states
        case STATEM: 
        case STATEE: 
        case STATES: 
            break;

        // For I, Send RD request to directory and then check
        // response to see if we should go to E or S states.:
        case STATEI: 
            dirstate = NETWORK->sendReqTileToDir(RD, addr, tile->index);
            if (dirstate == DSTATEEM)
                setState(STATEE);
            else
                setState(STATES);
            break;

        default :
            assert(0); // should not get here
    }
}

void CCSM::getFromNetwork(ulong msg) {
    switch (msg) {

        // These come from directory
        case INV: 
            netInitInv();
            break;
        case INT: 
            netInitInt();
            break;
     ///case REPLY: 
     ///    netInitReply();
     ///    break;

     ///// These come from other tiles.
     ///case FLUSH:
     ///    netInitFlush();
     ///    break;
     ///case INVACK:
     ///    netInitFlush();
     ///    break;
        default :
            assert(0); // should not get here
    }
}
