/*
 * Dusty Mabe - 2014
 * Dir.cc - Implementation of a directory to track sharing across
 *          partitions.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "Dir.h"
#include "BitVector.h"
#include "Net.h"
#include "Tile.h"
#include "types.h"


// Global NETWORK is defined in simulator.cc
extern Net *NETWORK;

// Global delay counter for the current outstanding memory request.
extern int CURRENTDELAY;
extern int CURRENTMEMDELAY;
extern int PARTSHARING;

/*
 * DirEntry constructor
 *    - Build up the data structures that belong to a
 *      directory entry pertaining to the block with base
 *      address blockaddr.
 */
DirEntry::DirEntry(ulong blockaddr) {
    blockaddr = blockaddr;
    state     = DSTATEI;
    sharers   = new BitVector(0);
}

/*
 * DirEntry destructor
 *    - Free mem related to DirEntry object.
 */
DirEntry::~DirEntry() {
    delete sharers;
}


/*
 * Dir constructor
 *    - Build up the data structures that belong to a
 *      directory.
 */
Dir::Dir(int partscheme) {
    int i;

    // We need a directory for every block. How many do we need
    // for a 32 bit address space and 64 byte blocks?
    //
    //      2^32 bytes * (1 block / 2^6 bytes) = 2^26 blocks
    //
    // Therefore we need 2^26 directory entries. What we will
    // do now is just allocoate the pointers for that many 
    // directories but we won't allocate the directories until
    // they are accessed.  
    directory = new DirEntry*[(1 << 26) - 1];

    // Verify each direntry is initialized to NULL
    for (i=0; i < (1<<26); i++)
        assert(directory[i] == NULL);

    // Calculate the # of partitions in the system.
    numparts = NPROCS/partscheme;

    // We need a table of partition vectors.
    parttable = new BitVector*[numparts];


    // Based on the partition scheme file in the appropriate
    // vectors with information.
    switch (partscheme) {

        case 1:
            for (i=0; i < numparts; i++)
                parttable[i] = new BitVector(0b1 << i);
            break;

        case 2:
            for (i=0; i < numparts; i++)
                parttable[i] = new BitVector(0b11 << 2*i);
            break;
                                      //   111111 
        case 4:                       //   5432109876543210
            parttable[0] = new BitVector(0b0000000000110011);
            parttable[1] = new BitVector(0b0000000011001100);
            parttable[2] = new BitVector(0b0011001100000000);
            parttable[3] = new BitVector(0b1100110000000000);
            break;
                                      //   111111 
        case 8:                       //   5432109876543210
            parttable[0] = new BitVector(0b1111111100000000);
            parttable[1] = new BitVector(0b0000000011111111);
            break;

                                      //   111111 
        case 16:                      //   5432109876543210
            parttable[0] = new BitVector(0b1111111111111111);
            break;

        default:
            assert(0); // Should not get here
    }
}

/*
 * Dir::mapAddrToTile
 *     - Given an address and a partition ID, map them
 *       to a specific tile within the partition. 
 */
int Dir::mapAddrToTile(int partid, int addr) {

    // Get the vector representing the partition
    BitVector *bv = parttable[partid];

    // Get the number of tiles within the partition
    int numtiles = bv->getNumSetBits();

    // Since the tiles logically share L2 the blocks are 
    // interleaved among the tiles. Find the tile offset
    // within the partition.
    int tileoffset = ADDRHASH(addr) % numtiles; 

    // Find the actual tile id of the tile. Note: add
    // 1 because even if offset is 0 we want to find 1st
    // set bit. 
    int tileid = bv->getNthSetBit(tileoffset + 1);

    return tileid;
}

/*
 * Dir::mapTileToPart
 *     - Given a tile index find the partition it belongs to
 */
int Dir::mapTileToPart(int tileid) {
    int i;
    for (i=0; i < numparts; i++)
        if (parttable[i]->getBit(tileid))
            return i;

    assert(0); // Should not get here
}

/*
 * Dir::invalidateSharers
 *     - Given a block address use the partitions vectors to determine
 *       what partitions share the block and send invalidations to all
 *       of them. Skip the pid partition.
 */
int Dir::invalidateSharers(int addr, int pid) {
    int max = 0;

    // Lets play a game with CURRENTDELAY. Since this stuff is
    // done in parallel we will save off the original value and
    // then find the max delay of all parallel requests. 
    ulong origDelay = CURRENTDELAY;
    CURRENTDELAY  = 0;

    // Get the bitvector of sharers.
    DirEntry  *de = directory[BLKADDR(addr)];
    BitVector *bv = de->sharers;

    //printf("Sharers are %x\n", bv->vector);

    int tileid;
    int partid;

    // Iterate over sharers and send INV to any that
    // exist. Also clear bit from vector.
    for(partid=0; partid < bv->size; partid++) {

        if (partid == pid)
            continue;

        if (bv->getBit(partid)) {

            // Get the actual tileid of the tile within the
            // partition that is responsible for addr
            tileid = mapAddrToTile(partid, addr);
            NETWORK->sendReqDirToTile(INV, addr, tileid);
            bv->clearBit(partid);

            // Update max and reset
            max = MAX(max, CURRENTDELAY);
            CURRENTDELAY = 0; // Reset for next iter
        }
    }

    // Add the max to the original delay
    CURRENTDELAY = origDelay + max;

}

/*
 * Dir::findClosestSharer
 *     - Given an address and a requesting tile, find the closest
 *       tile that already contains the block.
 *
 * returns the tile that originally had block in shared state that is
 * the closest to tile.
 */
int Dir::findClosestSharer(int addr, int tile) {
    int minhops = 10000;  // min tile to tile hops
    int closest = -1; // Tile that is closest to tile 
    int distance, tileid, partid;

    // Get the partition that the tile belongs to
    ulong pid = mapTileToPart(tile); 

    // Get the bitvector of sharers.
    DirEntry  *de = directory[BLKADDR(addr)];
    BitVector *bv = de->sharers;

    // Iterate over sharers 
    for(partid=0; partid < bv->size; partid++) {

        if (partid == pid)
            continue;

        if (bv->getBit(partid)) {
            // Get the actual tileid of the tile within the
            // partition that is responsible for addr
            tileid = mapAddrToTile(partid, addr);

            // Is it the closest tile?
            distance=NETWORK->calcTileToTileHops(tileid, tile);
            if (distance < minhops) {
                minhops = distance;
                closest = tileid;
            }
        }
    }

    // Return back the tile that had the block that is closest to
    // fromtile.
    return closest;
}

/*
 * Dir::interveneOwner
 *     - Given a block address use the DirEntry sharers bitvector
 *       to find the partition that owns the block. Map the addr
 *       and partid to a specific tile and then send an intervention
 *       to the tile.
 */
int Dir::interveneOwner(int addr) {
    // Get the bitvector of sharers.
    DirEntry  *de = directory[BLKADDR(addr)];
    BitVector *bv = de->sharers;

    int tileid;
    int partid;

    // Iterate over sharers and send INT to any that
    // exist.
    for(partid=0; partid < bv->size; partid++) {
        if (bv->getBit(partid)) {
            // Get the actual tileid of the tile within the
            // partition that is responsible for addr
            tileid = mapAddrToTile(partid, addr);
            NETWORK->sendReqDirToTile(INT, addr, tileid);
        }
    }
}

/*
 * Dir::replyData
 *     - Reply data to a requesting block
 */
void Dir::replyData(int addr, int fromtile, int totile) {

    // Is forwarding data requests to other partitions allowed? 
    // If not then just set fromtile to -1
    if (PARTSHARING == 0)
        fromtile = -1;

    // If fromtile == -1 then there is no sharer
    if (fromtile == -1) {

        // Had to access memory so add in the delay
        CURRENTMEMDELAY += MEMATIME;
        // Reply Data
        NETWORK->fakeDataDirToTile(addr, totile);

    } else {

        // Accessed the L2 $ of sending tile
        CURRENTDELAY += L2ATIME;
        // Reply Data - simulate sending from closesttile;
        NETWORK->fakeReqDirToTile(addr, fromtile);     // Fwd req to fromtile
        NETWORK->fakeDataTileToTile(fromtile, totile); // Data fromtile totile

    }
}

/*
 * Dir::setState
 *     - This function serves to change the state of the Directory
 *       entry to s. If we are transitioning to an invalid state then
 *       there is some housekeeping to do.
 */
void Dir::setState(ulong addr, int s) {

    DirEntry * de = directory[BLKADDR(addr)];
    assert(de); // verify de is not NULL

    de->state = s; // Set the new state

    // If we are going to the invalid state then delete the
    // memory associated with the directory entry.
    if (s == DSTATEI)
        delete de;
}

/*
 * Dir::getFromNetwork
 *     - This function serves to receive messages from the
 *       interconnection network.
 *
 * Returns the directory state to the caller
 */
ulong Dir::getFromNetwork(ulong msg, ulong addr, ulong fromtile) {

    // Get the blockaddr
    ulong blockaddr = BLKADDR(addr);

    if (directory[blockaddr] == NULL)
        directory[blockaddr] = new DirEntry(blockaddr);

    switch (msg) {
        case RD: 
            netInitRd(addr, fromtile);
            break;
        case RDX: 
            netInitRdX(addr, fromtile);
            break;
        case UPGR: 
            netInitUpgr(addr, fromtile);
            break;
        default :
            assert(0); // should not get here
    }

    return directory[blockaddr]->state;
}

/*
 * Dir::netInitRdX
 *     - This function handles the logic for when a RdX request
 *       is delivered to the directory.
 */
void Dir::netInitRdX(ulong addr, ulong fromtile) {
    int closesttile;

    DirEntry * de = directory[BLKADDR(addr)];

    // Get the partition that the tile belongs to
    ulong partid = mapTileToPart(fromtile); 

    switch (de->state) {

        // For EM we need to invalidate the current owner
        // and reply with data to new owner. Will stay in M state.
        case DSTATEEM: 
            // Find the closest sharer
            closesttile = findClosestSharer(addr, fromtile);
            // Invalidate current owner.
            invalidateSharers(addr, partid);
            // Reply Data
            replyData(addr, closesttile, fromtile);
            // Add new owner to bit map.
            de->sharers->setBit(partid);
            break;

        // For S we need to transition to M and invalidate all
        // sharers.
        case DSTATES: 
            // Find the closest sharer
            closesttile = findClosestSharer(addr, fromtile);
            // Invalidate all sharers
            invalidateSharers(addr, partid);
            // Reply Data
            replyData(addr, closesttile, fromtile);
            // Add new owner to bit map.
            de->sharers->setBit(partid);
            // Transition to EM
            setState(addr, DSTATEEM);
            break;

        // For invalid state just transition to M
        case DSTATEI: 
            // Reply Data
            replyData(addr, -1, fromtile);
            // Add new owner to bit map.
            de->sharers->setBit(partid);
            // Transition to EM
            setState(addr, DSTATEEM);
            break;

        default :
            assert(0); // should not get here
    }
}


/*
 * Dir::netInitRd
 *     - This function handles the logic for when a Rd request
 *       is delivered to the directory.
 */
void Dir::netInitRd(ulong addr, ulong fromtile) {
    int closesttile;

    DirEntry * de = directory[BLKADDR(addr)];

    // Get the partition that the tile belongs to
    ulong partid = mapTileToPart(fromtile); 

    switch (de->state) {

        // For EM we need to transistion to shared state 
        // and send an intervention to previous owner.
        case DSTATEEM: 
            // Find the closest sharer
            closesttile = findClosestSharer(addr, fromtile);
            // send intervention
            interveneOwner(addr);
            // Reply Data
            replyData(addr, closesttile, fromtile);
            // Add new sharer to bit map.
            de->sharers->setBit(partid);
            // Transition to S
            setState(addr, DSTATES);
            break;

        // For S, no need to change state
        case DSTATES: 
            // Find the closest sharer
            closesttile = findClosestSharer(addr, fromtile);
            // Reply Data
            replyData(addr, closesttile, fromtile);
            // Add new sharer to bit map.
            de->sharers->setBit(partid);
            break;

        // For I, transition to EM
        case DSTATEI: 
            // Reply Data
            replyData(addr, -1, fromtile);
            // Add new sharer to bit map.
            de->sharers->setBit(partid);
            // Transition to EM
            setState(addr, DSTATEEM);
            break;

        default :
            assert(0); // should not get here
    }
}

/*
 * Dir::netInitUpgr
 *     - This function handles the logic for when an Upgr request
 *       is delivered to the directory.
 */
void Dir::netInitUpgr(ulong addr, ulong fromtile) {

    DirEntry * de = directory[BLKADDR(addr)];
    // Get the partition that the tile belongs to
    ulong partid = mapTileToPart(fromtile); 

    switch (de->state) {
        // For ME we should never get UPGR since there
        // should not be more than one copy in the system
        case DSTATEEM: 
            assert(0);
            break;

        // For S, transistion to EM
        case DSTATES:
            // Invalidate all sharers, but first clear
            // the bit related to partid because that one 
            // shouldn't be invalidated.
            de->sharers->clearBit(partid);
            invalidateSharers(addr, partid);
            // Reply - no data
            NETWORK->fakeReqDirToTile(addr, fromtile);
            // Transition to EM
            setState(addr, DSTATEEM);
            // Add partid back into sharers bit map.
            de->sharers->setBit(partid);
            break;

        // For I we should never get UPGR because there are
        // no copies in the system.
        case DSTATEI: 
            assert(0);
            break;

        default :
            assert(0); // should not get here
    }
}
