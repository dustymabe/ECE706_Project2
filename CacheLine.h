/*
 * Dusty Mabe  - 2014
 * cacheLine.h - Header file for cacheLine class 
 */

#ifndef CACHELINE_H
#define CACHELINE_H

#include "types.h"

class CCSM; // Forward Declaration

// Keep some basic states for cache lines 
enum{
    INVALID = 0,
    VALID,
    DIRTY
};

class CacheLine {
protected:
    ulong tag;
    ulong index;
    ulong Flags;   // 0:invalid, 1:valid, 2:dirty 
    ulong seq; 
    ulong state;
 
public:
    CCSM * ccsm;
    CacheLine()                 { tag = 0; Flags = 0; }
    ulong getTag()              { return tag; }
    ulong getIndex()            { return index; }
    ulong getFlags()            { return Flags;}
    ulong getSeq()              { return seq; }
    void setSeq(ulong Seq)      { seq   = Seq;}
    void setFlags(ulong flags)  { Flags = flags;}
    void setTag(ulong a)        { tag   = a; }
    void setIndex(ulong a)      { index = a; }
    void invalidate()           { tag = 0; Flags = INVALID; }
    bool isValid()              { return ((Flags) != INVALID); }
    void init(CCSM *sm) {
        invalidate(); 
        ccsm = sm;
    }
};

#endif
