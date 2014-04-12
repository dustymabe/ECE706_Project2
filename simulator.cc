/*
 * Dusty Mabe - 2014
 * main.cc
 *
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fstream>
#include "BitVector.h"
#include "Cache.h"
#include "Dir.h"
#include "Tile.h"
#include "Net.h"
#include "params.h"

Net *NETWORK;

ulong CURRENTDELAY    = 0;
ulong CURRENTMEMDELAY = 0;

ulong PARTSHARING     = 0;


int main(int argc, char *argv[]) {
    
    int i;
    FILE * fp;
    char buf[256];
    char * token;
    char * op;
    int   proc, newproc;
    int   partscheme;
    int   partid;
    int   tabular = 0;
    ulong addr;
    Cache ** cacheArray;
    char delimit[4] = " \t\n"; // tokenize based on "space", "tab", eol
    char strHeader[2048];
    char strStats[2048];
    int count = 0;

    // Check input
    if (argv[1] == NULL) {
        printf("input format: ");
        printf("./sim <partitions> <partsharing> <trace_file> <tabular>\n");
        exit(1);
    }

    //Convert the arguments to integer values
    sscanf(argv[1], "%u", &partscheme);

    //Convert the arguments to integer values
    sscanf(argv[2], "%u", &PARTSHARING);

    // Store the filename
    char *fname =  (char *)malloc(100);
    fname = argv[3];

    if (argv[4] != NULL)
        tabular = 1;


    // Print out the simulator configuration (if not tabular)
    if (!tabular) {
        printf("===== 706 SMP Simulator Configuration =====\n");
        printf("L1_SIZE:                        %d\n", L1SIZE);
        printf("L1_ASSOC:                       %d\n", L1ASSOC);
        printf("L2_SIZE:                        %d\n", L2SIZE);
        printf("L2_ASSOC:                       %d\n", L2ASSOC);
        printf("BLOCKSIZE:                      %d\n", BLKSIZE);
        printf("NUMBER OF PROCESSORS:           %d\n", NPROCS);
        printf("COHERENCE PROTOCOL:             %s\n", "MESI");
        printf("TILES PER PARTITION:            %d\n", partscheme);
        printf("ALLOW PARITION SHARING:         %d\n", PARTSHARING);
        printf("TRACE FILE:                     %s\n", basename(fname));
    } 

    // Create a new directory. Rather than have 4 directories (one 
    // each corner tile) I am just going to use 1 directory and adjust
    // the math accordingly.
    Dir *dir = new Dir(partscheme);
    assert(dir);

    // Create a 4x4 array of Tiles here
    Tile * tiles[NPROCS];
    for (i=0; i < NPROCS; i++) {
        partid = dir->mapTileToPart(i);
        tiles[i] = new Tile(i, partscheme, dir->parttable[partid]->getVector());
        assert(tiles[i]);
    }

    // Create the global network element
    NETWORK = new Net(dir, tiles);
    assert(NETWORK);

    // Open the trace file
    fp = fopen(fname,"r");
    if (fp == 0) {   
        printf("Trace file problem\n");
        exit(0);
    }

    // Read each line of trace file and call Access() for each entry.
    // Each line in the tracefile is of the form:
    //       operation(r,w) address(8 hexa chars)
    //
    //      r 0x7fc61248
    //      w 0x7fc62c08
    //      r 0x7fc63738
    //
    //
    proc = 0;
    while (fgets(buf, 1024, fp)) {

        if (count++ == 100000) {
            count = 0;

            // Find a new proc to migrate to
            newproc = random() % (NPROCS);

            // Clear out dirty blocks in old and new procs
            tiles[proc]->FlushDirtyBlocks();

            // reset part info in all tiles
            for (i=0; i < NPROCS; i++) {
                tiles[i]->part->clearAllBits();
                dir->parttable[i]->clearAllBits(); 
            }

            // Create a new partition with the old proc and the new
            dir->parttable[proc]->setBit(proc);       // Add proc to part info
            dir->parttable[proc]->setBit(newproc);    // Add newproc to part info

            // Set the new partition info in the tiles
            tiles[proc]->part->setVector(dir->parttable[proc]->getVector());
            tiles[newproc]->part->setVector(dir->parttable[proc]->getVector());

            // Finally make the newproc be the current proc
            proc = newproc;

            // Add current proc to new procs partition
          //printf("processor is %d\n", proc);
        }
        assert(proc < NPROCS);
      //printf("processor is %d\n", proc);

        // The "operation" is first on the line
        token = strtok(buf, delimit);
        assert(token != NULL);
        op = token;
      //printf("operation is: %s\n", op);

        // The mem addr is last
        // NOTE: passing NULL to strtok here because
        //       we want to operate on same string
        token = strtok(NULL, delimit);
        assert(token != NULL);
        addr = strtoul(token, NULL, 16);
      //printf("address is: %x\n", (uint) addr);

        tiles[proc]->Access(addr, op[0]);

    }
    fclose(fp);


    // Print the output. Either tabular or normal
    if (tabular) {
    
        // Print the header first
        tiles[0]->PrintStatsTabular(1);

        // Now print all the bodies
        for (i=0; i < NPROCS; i++)
            tiles[i]->PrintStatsTabular(0);
    } else {

        // Print it all out
        for (i=0; i < NPROCS; i++)
            tiles[i]->PrintStats();
    }
}
