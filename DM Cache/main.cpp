//
//  main.cpp
//  DM Cache
//
//  Created by Max Nedorezov on 11/22/17.
//  Copyright © 2017 Max Nedorezov. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;

enum Hit { miss = 0, hit = 1 };
enum Operation { Read = 0x00, Write = 0xFF};

class CacheLine
{
    int cacheLine[8];

public:
    bool dirty;
    int tag;

    CacheLine()
    {
        dirty = true;
        tag = 0;

        for (int i = 0; i < 8; i++)
            cacheLine[i] = 0;
    }

    Hit tryToWriteDataToOffsetWithTag(int data, int offset, int newTag, int evictedData[10]) // evictedData stores the old tag in the second to last position
    {
        dirty = true;

        if (newTag == tag) {
            cacheLine[offset] = data;
            return hit;
        }

        for (int i = 0; i < 8; i++)
            evictedData[i] = cacheLine[i];

        evictedData[8] = tag; // store old tag in last position of evicted data

        return miss;
    }

    void updateCacheLineWithDataFromRAM(int newDataFromRam[8], int newTag, int &newData, int offset, Operation operation)
    {
        tag = newTag;
        for (int i = 0; i < 8; i++)
            cacheLine[i] = newDataFromRam[i];

        if (operation == Write)
            cacheLine[offset] = newData;
        else
            newData = cacheLine[offset];
    }

    Hit readDataAtOffsetWithTag(int &data, int offset, int dataTag, int evictedData[10]) // evictedData stores the old tag in the second to last position and dirty bit in the last position
    {
        if (dataTag == tag) {
            data = cacheLine[offset]; // return back the data requested
            evictedData[9] = dirty;
            return hit;
        }

        for (int i = 0; i < 8; i++) {
            evictedData[i] = cacheLine[i];
        }
        evictedData[8] = tag;
        evictedData[9] = dirty;

        dirty = false;

        return miss;
    }
};

struct InputInfo
{
    InputInfo(int inAddress, int inOperation, int inData)
    {
        tag = (inAddress & 0xFF00) >> 8; // isolate tag and shift right 8 bits
        lineNumber = (inAddress & 0x00F8) >> 3;
        offset = inAddress & 0x0007;

        if (inOperation != 0) operation = Write; else operation = Read;

        data = inData;
    }

    int tag;
    int lineNumber;
    int offset;

    Operation operation;
    int data;
};

void manipulateCache(InputInfo inputInfo, ofstream &outputFile);
int RAMAddress(int tag, int lineNumber);
void storeAndFetchDataFromRAM(int oldTag, int lineNumber, int evictedData[8], int newDataFromRAM[8], int newTag);

int RAM[65536] = { 0 };
CacheLine cache[32];

int main(int argc, char **argv)
{
    string fileName;
    fileName = argv[1];

    ifstream inputFile;
    inputFile.open(fileName.c_str());
    inputFile >> hex;

    ofstream outputFile;
    outputFile.open("dm-out.txt");

    int address, operation, data;
    while (inputFile >> address && inputFile >> operation && inputFile >> data)
    {
        InputInfo inputInfo = InputInfo(address, operation, data);
        manipulateCache(inputInfo, outputFile);
    }

    inputFile.close();
    outputFile.close();

    return 0;
}

void manipulateCache(InputInfo inputInfo, ofstream &outputFile)
{
    CacheLine cacheLine = cache[inputInfo.lineNumber];

    int evictedData[10] = { 0 };
    int newDataFromRAM[8] = { 0 };

    if (inputInfo.operation == Write)
    {
        Hit hit = cacheLine.tryToWriteDataToOffsetWithTag(inputInfo.data, inputInfo.offset, inputInfo.tag, evictedData);

        if (hit == miss) {
            storeAndFetchDataFromRAM(evictedData[8], inputInfo.lineNumber, evictedData, newDataFromRAM, inputInfo.tag);
            cacheLine.updateCacheLineWithDataFromRAM(newDataFromRAM, inputInfo.tag, inputInfo.data, inputInfo.offset, Write);
        }

    } else {

        int readData = 0;
        Hit hit = cacheLine.readDataAtOffsetWithTag(readData, inputInfo.offset, inputInfo.tag, evictedData);

        if (hit == miss) {
            storeAndFetchDataFromRAM(evictedData[8], inputInfo.lineNumber, evictedData, newDataFromRAM, inputInfo.tag);
            cacheLine.updateCacheLineWithDataFromRAM(newDataFromRAM, inputInfo.tag, readData, inputInfo.offset, Read);
        }

        outputFile << hex << setfill('0') << setw(2) << uppercase << readData << " " << hit << " " << evictedData[9] << endl; // evictedData[9] is the dirty bit
    }

    cache[inputInfo.lineNumber] = cacheLine; // save the current working cache line back to the global cache
}

// probably a good idea to split this into 2 functions but whatever...
void storeAndFetchDataFromRAM(int oldTag, int lineNumber, int evictedData[8], int newDataFromRAM[8], int newTag)
{
    // store the evicted data in the RAM
    int evictedDataRAMAddress = RAMAddress(oldTag, lineNumber);  // evictedData[8] is the old tag
    for (int i = evictedDataRAMAddress; i < evictedDataRAMAddress + 8; i++)
        RAM[i] = evictedData[i - evictedDataRAMAddress];

    // fetch data from RAM into cache and update cache with new data
    int newRAMAddress = RAMAddress(newTag, lineNumber);

    for (int i = 0; i < 8; i++)
        newDataFromRAM[i] = RAM[newRAMAddress + i];
}

int RAMAddress(int tag, int lineNumber) // assume offset is 0 because looking for a whole cache line
{
    int address = (tag << 8) | (lineNumber << 3);
    return address;
}
