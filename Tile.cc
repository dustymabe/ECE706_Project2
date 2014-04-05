/*
 * Dusty Mabe - 2014
 * cache.cc - Implementation of cache (either L1 or L2)
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "Tile.h"
#include "Cache.h"
#include "CacheLine.h"
#include "CCSM.h"
#include "BitVector.h"
#include "Net.h"
#include "params.h"


// Global NETWORK is defined in simulator.cc
extern Net *NETWORK;

// Global delay counter for the current outstanding memory request.
extern int CURRENTDELAY;
extern int CURRENTMEMDELAY;


Tile::Tile(int number, int partspertile, int partition) {

    index  = number;
    xindex = index / SQRTNPROCS;  
    yindex = index % SQRTNPROCS;  
    cycle    = 0;      // Keep count of cycles (measure of performance)
    locxfer  = 0;      // How many times did we get data from our own L2?
    locdelay = 0;      // Delay for local xfers. Should be same for each access.
    ctocxfer = 0;      // How many times did we get data from remote L2 in this partition?
    ctocdelay= 0;      // Hop delay for ctoc accesses
    memxfer  = 0;      // How many times did we access memory (for data, not writebacks)?
    ptopxfer = 0;      // How many times did we get data from remote L2 in other partition?
    ptopdelay= 0;      // Hop delay for ptop accesses
    accesses = 0;      // How many memory operations were there for this tile? 
    l2accesses = 0;    // How many L2 operations were there for this tile? 
    memcycles = 0;     // Keep up with cycles spent waiting for mem access
    memhopscycles = 0; // Keep up with hop cycles when memory is accessed

    l1cache = new Cache(this, L1, L1SIZE, L1ASSOC, BLKSIZE);
    assert(l1cache);

    l2cache = new Cache(this, L2, L2SIZE, L2ASSOC, BLKSIZE);
    assert(l2cache);

    partscheme = partspertile;

    part = new BitVector(partition);
}


/*
 * Tile::Access()
 *     - Access is the function that gets called when a value 
 *       is to be read/written by the proc in this tile.
 */
void Tile::Access(ulong addr, uchar op) {
    int state;

    // Bump accesses counter
    accesses++;

    // Reset global CURRENTDELAY counter 
    CURRENTDELAY = 0;
    CURRENTMEMDELAY = 0;

    // L1: Check L1 to see if hit
    state = l1cache->Access(addr, op);

    // If a hit then we are done (almost). Must make any write
    // hits in the L1 access the L2 as well (WRITETHROUGH).
    if (state == HIT && op == 'w')
        L2Access(addr, op); // Aggregate L2 access

    // L2: If the L1 Missed then access the aggregate L2
    if (state == MISS)
        L2Access(addr, op);

    // All accesses are done so add the accumulated delay
    // to the cycle counter.
    cycle += CURRENTDELAY;
    cycle += CURRENTMEMDELAY;
}

/*
 * Tile::L2Access()
 *     - Provide a generic access function that will access the
 *       aggregate L2 (logically shared) for a partition. If HIT,
 *       then L2 returns quickly. If MISS, the L2 will contact the
 *       Memory controller (directory) and retrieve the value.
 */
void Tile::L2Access(ulong addr, uchar op) {

    int tileid = mapAddrToTile(addr);
    int msg    = (op == 'w') ? L2WR : L2RD;
    int state = NETWORK->sendReqTileToTile(msg, addr, index, tileid);

    // Bump accesses counter
    l2accesses++;

    // If it was a hit and it was a remote cache then bump counter
    if (state == HIT) {
        if (tileid == index) {
            locxfer++;
            locdelay += CURRENTDELAY;
        } else {
            ctocxfer++;
            ctocdelay += CURRENTDELAY;
        }
    }

    // If it was a miss then we accessed memory or a remote
    // partition
    if (state == MISS) {
        if (CURRENTMEMDELAY != 0) {
            memxfer++;
            memcycles     += CURRENTMEMDELAY;
            memhopscycles += (CURRENTMEMDELAY + CURRENTDELAY);
        } else {
            ptopxfer++;
            ptopdelay += CURRENTDELAY;
        }
    }
}

/*
 * Tile::mapAddrToTile
 *     - Given an address map it to a specific tile 
 *       within the partition.
 */
int Tile::mapAddrToTile(ulong addr) {

    // Get the number of tiles within the partition
    int numtiles = part->getNumSetBits();

    // Since the tiles logically share L2 the blocks are
    // interleaved among the tiles. Find the tile offset
    // within the partition.
    int tileoffset = ADDRHASH(addr) % numtiles;

    // Find the actual tile id of the tile. Note: add
    // 1 because even if offset is 0 we want to find 1st
    // set bit.
    int tileid = part->getNthSetBit(tileoffset + 1);

    return tileid;
}

/*
 * Tile::PrintStats()
 *     - Print a header and then query the L1 and L2 to print
 *       stats about hit/miss rates. etc.
 */
void Tile::PrintStats() {
    printf("========================================================== (Tile %d)\n", index);
    printf("01. cycle completed:                            %lu\n",  cycle);
    printf("02. cache to cache xfer (within partition)      %lu\n",  ctocxfer);
    printf("03. memory xfer (does not include writebacks)   %lu\n",  memxfer);
    printf("04. part to part xfer  (outside partition)      %lu\n",  ptopxfer);
    printf("05. number of accesses                          %lu\n",  accesses);
    printf("06. memory cycles                               %lu\n",  memcycles);
    printf("07. average total access time (cycles)          %f\n" ,  ((float)cycle / (float)accesses));
    printf("08. average interconnect hop cycles             %f\n" ,  ((float)(cycle - memcycles) / (float)accesses));
    printf("09. average mem access cycles (excludes hops)   %f\n" ,  ((float)memcycles / (float)accesses));
    printf("10. average mem access cycles (includes hops)   %f\n" ,  ((float)(memcycles + memhopscycles)  / (float)accesses));
    printf("===== Simulation results (Cache %d L1) =============\n", index);
    l1cache->PrintStats();
    printf("===== Simulation results (Cache %d L2) =============\n", index);
    l2cache->PrintStats();
}

/*
 * Tile::PrintStatsTabular()
 *     - Print a header and then query the L1 and L2 to print
 *       stats about hit/miss rates. etc.
 */
void Tile::PrintStatsTabular(int printhead) {

    char buftemp[100]  = { 0 };
    char bufhead[2048] = { 0 };
    char bufbody[2048] = { 0 };


    sprintf(buftemp, "%15s", "tile");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", index);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "partscheme");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", partscheme);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "cycle");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", cycle);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "accesses");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", accesses);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "L2accesses");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", l2accesses);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "locxfer");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", locxfer);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "ctocxfer");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", ctocxfer);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "ptopxfer");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", ptopxfer);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "memxfer");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", memxfer);
    strcat(bufbody, buftemp);

////sprintf(buftemp, "%15s", "locdelay");
////strcat(bufhead, buftemp);
////sprintf(buftemp, "%15lu", locdelay);
////strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "locAAT");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)locdelay / (float)locxfer));
    strcat(bufbody, buftemp);

////sprintf(buftemp, "%15s", "ctocdelay");
////strcat(bufhead, buftemp);
////sprintf(buftemp, "%15lu", ctocdelay);
////strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "ctocAAT");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)ctocdelay / (float)ctocxfer));
    strcat(bufbody, buftemp);

////sprintf(buftemp, "%15s", "ptopdelay");
////strcat(bufhead, buftemp);
////sprintf(buftemp, "%15lu", ptopdelay);
////strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "ptopAAT");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)ptopdelay / (float)ptopxfer));
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "memAAT");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)(memcycles + memhopscycles) / (float)memxfer));
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "totalAAT");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)cycle / (float)accesses));
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "memcycles");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", memcycles);
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "ahopcycles");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)(cycle - memcycles) / (float)accesses));
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "amemnohops");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)memcycles / (float)accesses));
    strcat(bufbody, buftemp);

    sprintf(buftemp, "%15s", "amemwithhops");
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15f", ((float)(memcycles + memhopscycles)  / (float)accesses));
    strcat(bufbody, buftemp);

    if (printhead) {
        // Print the head
        printf("%s", bufhead);
        // Print the head from L1
        l1cache->PrintStatsTabular(1);
        // Print the head from L2
        l2cache->PrintStatsTabular(1);
        printf("\n");
    } else {
        // Print the bodies
        printf("%s", bufbody);
        // Print the head from L1
        l1cache->PrintStatsTabular(0);
        // Print the head from L2
        l2cache->PrintStatsTabular(0);
        printf("\n");
    }
}


/*
 * Tile::getFromNetwork
 *     - This function will be called by the Net class and
 *       represents when a message has been received by this tile
 *       from the network.
 */
int Tile::getFromNetwork(ulong msg, ulong addr, ulong fromtile) {

    CacheLine * line;
    int state;

    // Handle L1 messages first
    if (msg == L1INV) {
        l1cache->invalidateLineIfExists(addr);
        CURRENTDELAY += L1ATIME;
        return -1;
    }


    // Now handle L2 messages
    switch (msg) {

        case INV:
        case INT:

            // Get the L2 cache line that corresponds to addr
            line = l2cache->findLine(addr);
            CURRENTDELAY += L2ATIME;

            // If the line has been evicted already then 
            // nothing to do.
            if (!line)
                return -1;

            // Pass the message on to the CCSM
            line->ccsm->getFromNetwork(msg);
            return -1;

        case L2RD:
            state = l2cache->Access(addr, 'r');
            // Fake sending back data to the requesting tile
            NETWORK->fakeDataTileToTile(index, fromtile);
            return state;

        case L2WR:
            state = l2cache->Access(addr, 'w');
            // Fake sending back data to the requesting tile
            NETWORK->fakeDataTileToTile(index, fromtile);
            return state;

        default:
            assert(0); // Should not get here
    }
}

/*
 * Tile::broadcastToPartition
 *     - Broadcast a message to all Tiles in a partition
 *       regarding the provided addr. Utilizes the partition
 *       table to determine which tiles are within our partition
 *       and utilizes the network to send the message.  
 *
 */
void Tile::broadcastToPartition(ulong msg, ulong addr) {

    int i;
    int max = 0;

    // Lets play a game with CURRENTDELAY. Since this stuff is
    // done in parallel we will save off the original value and
    // then find the max delay of all parallel requests. 
    ulong origDelay = CURRENTDELAY;
    CURRENTDELAY  = 0;

    for(i=0; i < part->size; i++) {
        if (part->getBit(i)) {
            NETWORK->sendReqTileToTile(msg, addr, index, i);
            max = MAX(max, CURRENTDELAY);
            CURRENTDELAY = 0; // Reset for next iter
        }
    }

    // Add the max to the original delay
    CURRENTDELAY = origDelay + max;
}
