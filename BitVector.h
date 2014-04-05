/*
 * Dusty Mabe - 2014
 * BitVector.h - Header file for the BitVector class
 */
#ifndef BV_H
#define BV_H

#include "params.h"

class BitVector {
    private:
        int vector;
        
    public:
        int size;
        BitVector(int value);
        ~BitVector() {};

        int getFirstSetBit();
        int getNumSetBits();
        int getBit(int bit);
        int getNthSetBit(int n);
        int getVector();

        void clearAllBits();
        void setBit(int bit);
        void clearBit(int bit);
};

#endif
