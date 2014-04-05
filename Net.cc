/*
 * Dusty Mabe - 2014
 * Net.cc - Implementation of interconnection network.
 */

#include <stdlib.h>
#include <assert.h>
#include "Net.h"
#include "Dir.h"
#include "Tile.h"
#include "types.h"
#include "params.h"

// Global delay counter for the current outstanding memory request.
extern int CURRENTDELAY;

Net::Net(Dir * dirr, Tile ** tiless) {
    dir   = dirr;
    tiles = tiless;
}

ulong Net::sendReqTileToTile(ulong msg, ulong addr, ulong fromtile, ulong totile) {
    // Add in the delay
    if (fromtile != totile)
        CURRENTDELAY += HOPDELAY(calcTileToTileHops(fromtile, totile));

    // Service the request
    return tiles[totile]->getFromNetwork(msg, addr, fromtile);
}

ulong Net::sendReqDirToTile(ulong msg, ulong addr, ulong totile) {
    // Add in the delay
    CURRENTDELAY += HOPDELAY(calcTileToDirHops(addr, totile));
    // Service the request
    tiles[totile]->getFromNetwork(msg, addr, -1); // Use invalid tile
    return 1;
}

ulong Net::sendReqTileToDir(ulong msg, ulong addr, ulong fromtile) {
    // Add in the delay
    CURRENTDELAY += HOPDELAY(calcTileToDirHops(addr, fromtile));
    // Service the request
    return dir->getFromNetwork(msg, addr, fromtile);
}

ulong Net::fakeReqDirToTile(ulong addr, ulong totile) {
    // Add in the delay
    CURRENTDELAY += HOPDELAY(calcTileToDirHops(addr, totile));
    return 1;
}

ulong Net::fakeDataTileToTile(ulong fromtile, ulong totile) {
    // Add in the delay
    if (fromtile != totile)
        CURRENTDELAY += DATAHOPDELAY(calcTileToTileHops(fromtile, totile));
    return 1;
}

ulong Net::fakeDataDirToTile(ulong addr, ulong totile) {
    // Add in the delay
    CURRENTDELAY += DATAHOPDELAY(calcTileToDirHops(addr, totile));
    return 1;
}

ulong Net::flushToMem(ulong addr, ulong fromtile) {

    // Don't add mem access time to delay because
    // we don't need to wait.. just calculate time to
    // send data to mem.

    // Don't need to actually send a message to mem
    // just calculate # hops and then delay
    int hops  = calcTileToDirHops(addr, fromtile);
    int delay = DATAHOPDELAY(hops);

    CURRENTDELAY += delay; 
}

ulong Net::calcTileToDirHops(ulong addr, ulong tile) {

    int dirnum = BLKADDR(addr) % 4;
    int hops   = 0;
    switch (dirnum) {
        case 0: // Attached to tile 0. 1 hop to the left
            hops = calcDistance(-1, 0, tiles[tile]->xindex, tiles[tile]->yindex);
            break;
        case 1: // Attached to tile 3. 1 hop to the right
            hops = calcDistance(4, 0, tiles[tile]->xindex, tiles[tile]->yindex);
            break;
        case 2: // Attached to tile 12. 1 hop to the left
            hops = calcDistance(-1, 3, tiles[tile]->xindex, tiles[tile]->yindex);
            break;
        case 3: // Attached to tile 15. 1 hop to the right
            hops = calcDistance(4, 3, tiles[tile]->xindex, tiles[tile]->yindex);
            break;
        default :
            assert(0); // Should not get here.
    }

    return hops;
}

ulong Net::calcTileToTileHops(ulong fromtile, ulong totile) {
    int x0 = tiles[fromtile]->xindex;
    int y0 = tiles[fromtile]->yindex;
    int x1 = tiles[totile]->xindex;
    int y1 = tiles[totile]->yindex;
    return calcDistance(x0, x1, y0, y1);
}

ulong Net::calcDistance(int x0, int x1, int y0, int y1) {
    return (abs(x1-x0) + abs(y1-y0));
}
