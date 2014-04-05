/*
 * Dusty Mabe - 2014
 * BitVector.cc 
 *     - Function definitions for the BitVector class.
 */

#include <assert.h>
#include "BitVector.h"
#include "params.h"

BitVector::BitVector(int value) {

    // Set the bitvector equal to the value
    vector = value;

    // For us the bitvectors will all be size NPROCS 
    size   = NPROCS;
}

int BitVector::getVector() {
    return vector;
}

int BitVector::getFirstSetBit() {
    int i;
    for(i=0; i < size; i++) {
        if (vector & (1 << i))
            return i;
    }
}

int BitVector::getNumSetBits() {
    int i;
    int count = 0;
    for(i=0; i < size; i++) {
        if (vector & (1 << i))
            count++;
    }
    return count;
}

void BitVector::setBit(int bit) {
    vector |= (1 << bit);
}

void BitVector::clearBit(int bit) {
    vector &= ~(1 << bit);
}

int BitVector::getBit(int bit) {
    return ((vector & (1 << bit)) ? 1 : 0);
}

void BitVector::clearAllBits() {
    vector = 0;
}

int BitVector::getNthSetBit(int n) {
    int i;
    for(i=0; i < size; i++) {
        if (vector & (1 << i))
            n--;
        if (n == 0)
            return i;
    }
    assert(0); // should not get here
}
