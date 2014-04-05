/*
 * Dusty Mabe - 2014
 * cache.cc - Implementation of cache (either L1 or L2)
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include "Cache.h"
#include "CacheLine.h"
#include "CCSM.h"
#include "Tile.h"
#include "params.h"

// Global delay counter for the current outstanding memory request.
extern int CURRENTDELAY;

/*
 * Cache::Cache - create a new cache object.
 * Arguments:
 *      - l - the cache level (L1 or L2)
 *      - s - the size of the cache
 *      - a - the associativity of the cache
 *      - b - the size of each block (line) in the cache
 *
 */
Cache::Cache(Tile * t, int l, int s, int a, int b) {

    CCSM * ccsm;
    ulong i, j;

    // Initialize all counters
    lruCounter   = 0;
    writeBacks   = 0;
    readMisses   = 0;
    writeMisses  = 0;
    reads = writes = 0;

    // Process arguments
    tile       = t;
    cacheLevel = l;
    size       = (ulong)(s);
    lineSize   = (ulong)(b);
    assoc      = (ulong)(a);       // assoc = # of lines within a set 
    numSets    = (ulong)((s/b)/a); // How many sets in a cache?
    numLines   = (ulong)(s/b);     // How many lines in the cache?

    indexbits  = log2(numSets);   //log2 from cmath
    offsetbits = log2(lineSize);  //log2 from cmath
    tagbits    = 32 - indexbits - offsetbits;
    
    // Generate a bit mask that will mask off all of the 
    // tag bits from the address when ANDed with the addr.
    // This will generate something like 0b11110000...
    tagMask  = 1 << (indexbits + offsetbits);
    tagMask -= 1;
  
    // create a two dimentional cache, sized as cache[sets][assoc]
    cacheArray = new CacheLine*[numSets];
    for (i=0; i < numSets; i++) {
        cacheArray[i] = new CacheLine[assoc];
    }

    // If this is an L2 cache then we will create
    // a CCSM for each cache line. Since our L1 is write-through
    // we don't need a CCSM for L1 and can just keep up with the
    // state at the L2 cache. 
    if (cacheLevel == L2)
        for (i=0; i < numSets; i++)
            for (j=0; j< assoc; j++) {
                ccsm = new CCSM(tile, this, &(cacheArray[i][j]));
                cacheArray[i][j].init(ccsm);
            }
}

/*
 * Cache::calcTag
 *     - Return the tag from addr. This is done by
 *       right shifting (indexbits + offsetbits) from
 *       the address so that you are just left with the
 *       tag bits. 
 */
ulong Cache::calcTag(ulong addr) {
    return (addr >> (indexbits + offsetbits));
}

/*
 * Cache::calcIndex
 *     - Return the index from addr. This is done by 
 *       first masking off the tag bits (using the 
 *       previously calculated tagMask) and then right 
 *       shifting (offsetbits) from the address so that 
 *       we are just left with the index bits. 
 */
ulong Cache::calcIndex(ulong addr) {
    return ((addr & tagMask) >> offsetbits);
}

/*
 * Cache::getBaseAddr
 *     - Given a tag and an index rebuild the base address
 *       for the block referenced by that tag/index.
 *
 *       This means we must take the tag and shift over by the
 *       number of index bits and then & that with the index.
 *       Then left shift that by the number of offset bits. 
 */
ulong Cache::getBaseAddr(ulong tag, ulong index) {
    return ((tag << indexbits) | index) << offsetbits;
}


/*
 * Cache::Access
 *     - Perform an access (r/w) to a particular addr. 
 *
 * Returns MISS if miss
 * Returns HIT  if hit
 */
ulong Cache::Access(ulong addr, uchar op) {
    CacheLine * line;
    int state;

    // Update global delay counter with access time
    if (cacheLevel == L2)
        CURRENTDELAY += L2ATIME;
    else
        CURRENTDELAY += L1ATIME;

    // Per cache global counter to maintain LRU order
    // among cache ways; updated on every cache access.
    lruCounter++;

    // Clear the bus indicator that a flush has been performed
  //bus->clearFlushed();
            
    // Update w/r counters
    if (op == 'w')
        writes++;
    else
        reads++;
    
    // See if the block that contains addr is already 
    // in the cache. 
    if (!(line = findLine(addr)))
        state = MISS;
    else
        state = HIT;

    // If not in the cache then fetch it and
    // update the counters
    if (state == MISS) {
        line = fillLine(addr);
        if (op == 'w') 
            writeMisses++;
        else
            readMisses++;
    }

    // If a write then set the flag to be DIRTY
    if (op == 'w')
        line->setFlags(DIRTY);    

    // If cache hit then update LRU
    if (state == HIT)
        updateLRU(line);

    // Update the cache coherence protocol state machine
    // for this line in the cache
    if (cacheLevel == L2) {
        if (op == 'w')
            line->ccsm->procInitWr(addr);
        else
            line->ccsm->procInitRd(addr);
    }

    // Return an indication of if we hit or miss.
    return state;
}

/*
 * Cache::findLine
 *     - Find a line within the cache that corresponds
 *       to the address addr. 
 *
 * Returns a CacheLine object or NULL if not found.
 */
CacheLine * Cache::findLine(ulong addr) {
    ulong index, j, tag;

    // Calculate tag and index from addr
    tag   = calcTag(addr);   // Tag value
    index = calcIndex(addr); // Set index
  
    // Iterate through set to see if we have a hit.
    for(j=0; j<assoc; j++) {

        // If not valid then continue
        if (cacheArray[index][j].isValid() == 0)
            continue;

        // Does the tag match.. If so then score!
        if (cacheArray[index][j].getTag() == tag) {
            return &(cacheArray[index][j]);
        }
    }

    // If we made it here then !found. 
    return NULL;
}

/*
 * Cache::updateLRU
 *     - Update the sequence for line to be 
 *       the current cycle.
 */
void Cache::updateLRU(CacheLine *line) {
    line->setSeq(lruCounter);  
}

/*
 * Cache::getLRU
 *     - Get the LRU cache line for the set that addr 
 *       maps to. If an invalid line exists in the set
 *       then return it. If not then choose the LRU line
 *       as the victim and return it.
 *
 * Returns a CacheLine object that represents the victim.
 */
CacheLine * Cache::getLRU(ulong addr) {
    ulong index, j, victim, min;

    // set victim = assoc (an impossible value)
    victim = assoc;

    // set min to current lruCounter (max possible seq)
    min = lruCounter;

    // Calculate set index
    index = calcIndex(addr);
   
    // First see if there are any invalid blocks
    for(j=0;j<assoc;j++) { 
      if(cacheArray[index][j].isValid() == 0)
          return &(cacheArray[index][j]);     
    }   

    // No invalid lines. Find LRU. 
    for(j=0;j<assoc;j++) {
        if (cacheArray[index][j].getSeq() <= min) { 
            victim = j; 
            min = cacheArray[index][j].getSeq();
        }
    } 
    
    // Verify a victim was found
    assert(victim != assoc);

    // Return the victim
    return &(cacheArray[index][victim]);
}

/*
 * Cache::fillLine
 *     - Allocate a new line in the cache for the block
 *       that contains addr.
 *
 * Returns a CacheLine object that represents the filled line.
 */
CacheLine *Cache::fillLine(ulong addr) { 
    CacheLine *victim;

    // Get the LRU block (or invalid block)
    victim = getLRU(addr);
    assert(victim);

    // If the chosen victim is dirty then update writeBack
    if (victim->isValid() && victim->getFlags() == DIRTY)
        writeBack();

    // If the chosen victim is valid then mark as invalid 
    // in the CCSM
    if (cacheLevel == L2 && victim->isValid())
        victim->ccsm->evict();

    // Since we are placing data into this line
    // then update the LRU information to indicate
    // it was accessed this cycle.
    updateLRU(victim);

    // Update information for this cache line.
    victim->setTag(calcTag(addr));
    victim->setIndex(calcIndex(addr));
    victim->setFlags(VALID);    

    return victim;
}

/*
 * Cache::invalidateLineIfExists
 *     - This function serves to invalidate a line associated
 *       with addr if it exists in the cache. 
 *
 *       This is primarily used for when invalidation requests
 *       are sent to the L1 as a result of the line getting evicted
 *       from the L2 (L1 and L2 are inclusive)
 */
void Cache::invalidateLineIfExists(ulong addr) { 
    CacheLine *line;
    if (line = findLine(addr)) {
        
        // If the line is dirty then update writeBack
        if (line->isValid() && line->getFlags() == DIRTY)
            writeBack();
        line->invalidate();

    }
}


/*
 * Cache::PrintStats
 *     - Print statistics for this cache.
 */
void Cache::PrintStats() {
    printf("01. number of reads:                            %lu\n", reads);
    printf("02. number of read misses:                      %lu\n", readMisses);
    printf("03. number of writes:                           %lu\n", writes);
    printf("04. number of write misses:                     %lu\n", writeMisses);
    printf("05. number of write backs:                      %lu\n", writeBacks);
////printf("06. number of invalid to exclusive (INV->EXC):  %lu\n", ItoE);
////printf("07. number of invalid to shared (INV->SHD):     %lu\n", ItoS);
////printf("08. number of modified to shared (MOD->SHD):    %lu\n", MtoS);
////printf("09. number of exclusive to shared (EXC->SHD):   %lu\n", EtoS);
////printf("10. number of shared to modified (SHD->MOD):    %lu\n", StoM);
////printf("11. number of invalid to modified (INV->MOD):   %lu\n", ItoM);
////printf("12. number of exclusive to modified (EXC->MOD): %lu\n", EtoM);
////printf("13. number of owned to modified (OWN->MOD):     %lu\n", OtoM);
////printf("14. number of modified to owned (MOD->OWN):     %lu\n", MtoO);
////printf("15. number of shared to invalid (SHD->INV):     %lu\n", StoI);
////printf("16. number of cache to cache transfers:         %lu\n", transfers);
////printf("17. number of interventions:                    %lu\n", interventions);
////printf("18. number of invalidations:                    %lu\n", invalidations);
////printf("19. number of flushes:                          %lu\n", flushes);

}

/*
 * Cache::PrintStatsTabular
 *     - Print statistics for this cache.
 */
void Cache::PrintStatsTabular(int printhead) {

    char buftemp[100]  = { 0 };
    char bufhead[2048] = { 0 };
    char bufbody[2048] = { 0 };
    char level[100]    = "L2";

    if (cacheLevel == L1)
        level[1] = '1';

    sprintf(level+2, "%s", "reads");
    sprintf(buftemp, "%15s", level);
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", reads);
    strcat(bufbody, buftemp);

    sprintf(level+2, "%s", "rdMisses");
    sprintf(buftemp, "%15s", level);
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", readMisses);
    strcat(bufbody, buftemp);

    sprintf(level+2, "%s", "writes");
    sprintf(buftemp, "%15s", level);
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", writes);
    strcat(bufbody, buftemp);

    sprintf(level+2, "%s", "wrMisses");
    sprintf(buftemp, "%15s", level);
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", writeMisses);
    strcat(bufbody, buftemp);

    sprintf(level+2, "%s", "wrBacks");
    sprintf(buftemp, "%15s", level);
    strcat(bufhead, buftemp);
    sprintf(buftemp, "%15lu", writeBacks);
    strcat(bufbody, buftemp);

    if (printhead)
        printf("%s", bufhead);
    else
        printf("%s", bufbody);

}

