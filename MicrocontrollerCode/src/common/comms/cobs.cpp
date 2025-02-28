///
/// \file 				CobsTranscoder.hpp
/// \author 			Geoffrey Hunter <gbmhunter@gmail.com> (www.mbedded.ninja)
/// \edited             n/a
/// \created			2017-06-10
/// \last-modified		2017-09-22
/// \brief 				Contains the CobsTranscoder class.
/// \details
///		See README.rst in root dir for more info.

// User includes
#include "cobs.h"

using namespace std;

namespace Cesium {

bool CobsTranscoder::Encode(const vector<uint8_t> &rawData, vector<uint8_t> &encodedData) {

    int startOfCurrBlock = 0;
    uint8_t numElementsInCurrBlock = 0;

    auto it = rawData.begin();

    // Create space for first (this will be
    // overwritten once count to next 0x00 is known)
    encodedData.push_back(0x00);

    while (it != rawData.end()) {

        if (*it == 0x00) {
            // Save the number of elements before the next 0x00 into
            // the output
            encodedData[startOfCurrBlock] = (uint8_t) (numElementsInCurrBlock + 1);

            // Add placeholder at start of next block
            encodedData.push_back(0x00);

            startOfCurrBlock = encodedData.size() - 1;

            // Reset count of num. elements in current block
            numElementsInCurrBlock = 0;

        } else {
            encodedData.push_back(*it);
            numElementsInCurrBlock++;

            if(numElementsInCurrBlock == 254) {

                encodedData[startOfCurrBlock] = (uint8_t) (numElementsInCurrBlock + 1);

                // Add placeholder at start of next block
                encodedData.push_back(0x00);

                startOfCurrBlock = encodedData.size() - 1;

                // Reset count of num. elements in current block
                numElementsInCurrBlock = 0;
            }

        }
        it++;
    }

    // Finish the last block
    // Insert pointer to the terminating 0x00 character
    encodedData[startOfCurrBlock] = numElementsInCurrBlock + 1;
    encodedData.push_back(0x00);

    return true;
}

bool CobsTranscoder::Decode(const vector<uint8_t> &encodedData, vector<uint8_t> &decodedData) {

    decodedData.clear();

    int encodedDataPos = 0;

    while (encodedDataPos < encodedData.size()) {

        int numElementsInBlock = encodedData[encodedDataPos] - 1;
        encodedDataPos++;

        // Copy across all bytes within block
        for (int i = 0; i < numElementsInBlock; i++) {
            uint8_t byteOfData = encodedData[encodedDataPos];
            if (byteOfData == 0x00) {
                decodedData.clear();
                return false;
            }

            decodedData.push_back(encodedData[encodedDataPos]);
            encodedDataPos++;
        }

        if (encodedData[encodedDataPos] == 0x00) {
            // End of packet found!
            break;
        }

        // We only add a 0x00 byte to the decoded data
        // IF the num. of elements in block was less than 254.
        // If num. elements in block is max (254), then we know that
        // the block was created due to it reaching maximum size, not because
        // a 0x00 was found
        if(numElementsInBlock < 0xFE) {
            decodedData.push_back(0x00);
        }
    }

    return true;
}
} // namespace Cesium