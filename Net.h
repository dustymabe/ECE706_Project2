/*
 * Dusty Mabe - 2014
 * Net.h - Header file for an implementation of an interconnection
 *         network. Caches and Mem Controllers will pass messages to
 *         the network and they will be delivered to other caches and
 *         memory contorllers. 
 *        
 *         The network will return the amount of time it took to retrieve
 *         the value.
 *
 *         The network is global in that everything will have access to
 *         it without having to explicity have it be a part of their
 *         class data. 
 */
#ifndef NET_H
#define NET_H

#include "types.h"

class Dir;  // Forward Declaration
class Tile; // Forward Declaration


// Message types to be passed back and forth over the network. 
// Messages go between Directory and Tiles (L2 CCSM), between two
// Tiles, etc..
enum {

    // Dir -> Tile (L2 CCSM) messages
    INV=100, // Invalidation
    INT,     // Intervention

    // Tile (L2 CCSM) -> Dir messages
    RD,      // Read
    RDX,     // Read Exclusive
    UPGR,    // Upgrade from S to M

    // Tile (L2) -> Tile (L1) messages
    // Note: happens when L2 line is evicted
    L1INV,

    // Tile (L1) -> Tile (L2) messages
    // Note: happens on L1 miss or L1 writethrough
    L2RD,
    L2WR,
};


class Net {
private:
    Tile ** tiles;
    Dir  *  dir;

public:
    Net(Dir * dirr, Tile ** tiless);
    ~Net();
    ulong sendReqTileToTile(ulong msg, ulong addr, ulong fromtile, ulong totile);
    ulong sendReqDirToTile( ulong msg, ulong addr, ulong totile);
    ulong sendReqTileToDir( ulong msg, ulong addr, ulong fromtile);

    ulong fakeReqDirToTile(ulong addr, ulong totile);
    ulong fakeDataTileToTile(ulong fromtile, ulong totile);
    ulong fakeDataDirToTile(ulong addr, ulong totile);
    ulong flushToMem(ulong addr, ulong fromtile);
    ulong calcTileToDirHops(ulong addr, ulong tile);
    ulong calcTileToTileHops(ulong fromtile, ulong totile);
    ulong calcDistance(int x0, int x1, int y0, int y1);
};

#endif
