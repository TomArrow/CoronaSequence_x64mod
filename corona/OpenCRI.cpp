/**
 *  @todo  use our own longjmp instead of libpng's.  this way we don't need
 *         to use PNG_SETJMP_SUPPORTED in Windows, and don't depend on
 *         png_ptr->jmpbuf in older versions of libpng.
 */
/*#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)*/

//#include "RawSpeed-API.h"
//#include "rawspeedconfig.h"

 //#include "amaze/rtengine/array2D.h"
//#include "amaze/rtengine/amaze_demosaic_RT.h"


#include "Debug.h"
#include "Open.h"
#include "SimpleImage.h"
#include "Utility.h"
#include "OpenCRI.h"
#include <map>
#include <vector>
#include <stdexcept>
#include "dng_sdk/source/dng_stream.h"
#include "dng_sdk/source/dng_lossless_jpeg.h"
#include "dng_sdk/source/dng_lossless_jpeg.cpp"


//#include "common.h"
//#include <sysinfoapi.h>



namespace corona {

  //typedef unsigned char byte;
  
    typedef std::map<uint32_t, std::vector<byte>> criTagData_t;

    static criTagData_t readCRITags(byte* data,int length) {
        criTagData_t retVal;

        uint32_t currentTag;
        uint32_t currentTagLength;
        uint32_t currentPosition = 0;

        while (currentPosition < length - 3)
        {

            currentTag = *((uint32_t*)(data + currentPosition)); currentPosition +=sizeof(uint32_t);
            if (currentTag == 0)
            {
                continue;
            }
            currentTagLength = *((uint32_t*)(data + currentPosition)); currentPosition += sizeof(uint32_t);

            std::vector<byte> currentTagData(data + currentPosition,data+currentPosition+currentTagLength);
            currentPosition += currentTagLength;
            retVal.insert(std::pair<uint32_t,std::vector<byte>>(currentTag, currentTagData));
        }

        return retVal;
    }






    dng_memory_stream_simple::dng_memory_stream_simple(byte* data, uint64_t dataLength): dataPointer(data), myDataLength(dataLength) {
    };

    uint64 dng_memory_stream_simple::DoGetLength() {
        return (uint64)myDataLength;
    };

    void dng_memory_stream_simple::DoRead(void* data,
        uint32 count,
        uint64 offset) {
        if (offset >= (uint64_t)myDataLength) {
            return;
        }
        if ((uint64_t)offset + (uint64_t)count > (uint64_t)myDataLength) {
            count = (uint64_t)myDataLength - (uint64_t)offset;
        }
        memcpy(data,dataPointer+ (uint64_t)offset,count);
    };



    void dng_memory_spooler_simple::Spool(const void* data,
        uint32 count) {
        
        myData.insert(myData.end(), &((byte*)data)[0], &((byte*)data)[count]);
    };

    void dng_memory_spooler_simple::DoRead(void* data,
        uint32 count,
        uint64 offset) {
        if (offset >= myData.size()) {
            return;
        }
        if (offset + count > myData.size()) {
            count = myData.size() - offset;
        }
        memcpy(data, (void*)(myData.data() + (uint64_t)offset), count);
    };
    std::vector<byte>* dng_memory_spooler_simple::getDataVectorPointer() {
        return &myData;
    };






    // The following function is adapted from ffmpeg:
     /*
      * CRI image decoder
      *
      * Copyright (c) 2020 Paul B Mahol
      *
      * This file is part of FFmpeg.
      *
      * FFmpeg is free software; you can redistribute it and/or
      * modify it under the terms of the GNU Lesser General Public
      * License as published by the Free Software Foundation; either
      * version 2.1 of the License, or (at your option) any later version.
      *
      * FFmpeg is distributed in the hope that it will be useful,
      * but WITHOUT ANY WARRANTY; without even the implied warranty of
      * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
      * Lesser General Public License for more details.
      *
      * You should have received a copy of the GNU Lesser General Public
      * License along with FFmpeg; if not, write to the Free Software
      * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
      */
    static void unpack_10bit(byte* data, int dataLength, uint16_t* dst, int shift,
        int w, int h, ptrdiff_t stride)
    {
        int count = w * h;
        int pos = 0;

        int readByteOffset = 0;

        while (count > 0) {
            uint32_t a0, a1, a2, a3;
            if (readByteOffset >= (dataLength - 16))
                break;
            /*if (bytestream2_get_bytes_left(gb) < 4)
                break;*/
            a0 = *((uint32_t*)&data[readByteOffset]); readByteOffset += 4;
            a1 = *((uint32_t*)&data[readByteOffset]); readByteOffset += 4;
            a2 = *((uint32_t*)&data[readByteOffset]); readByteOffset += 4;
            a3 = *((uint32_t*)&data[readByteOffset]); readByteOffset += 4;
            dst[pos] = (((a0 >> 1) & 0xE00) | (a0 & 0x1FF)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 1)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a0 >> 13) & 0x3F) | ((a0 >> 14) & 0xFC0)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 2)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a0 >> 26) & 7) | ((a1 & 0x1FF) << 3)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 3)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a1 >> 10) & 0x1FF) | ((a1 >> 11) & 0xE00)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 4)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a1 >> 23) & 0x3F) | ((a2 & 0x3F) << 6)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 5)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a2 >> 7) & 0xFF8) | ((a2 >> 6) & 7)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 6)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a3 & 7) << 9) | ((a2 >> 20) & 0x1FF)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 7)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a3 >> 4) & 0xFC0) | ((a3 >> 3) & 0x3F)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 8)
                    break;
                dst += stride;
                pos = 0;
            }
            dst[pos] = (((a3 >> 16) & 7) | ((a3 >> 17) & 0xFF8)) << shift;
            pos++;
            if (pos >= w) {
                if (count == 9)
                    break;
                dst += stride;
                pos = 0;
            }

            count -= 9;
        }
    }



  

  //////////////////////////////////////////////////////////////////////////////

  Image* OpenCRI(File* file) {

    COR_GUARD("OpenCRI");

    int width;
    int height;
    byte bayerPattern[2][2];
    bool mustDo10bitUnpack = false;
    RAWDATAFORMAT rawDataFormat;

    file->seek(0, corona::File::END);
    uint64_t filesize = file->tell();
    file->seek(0, corona::File::BEGIN);

    byte* fileContents = new byte[filesize];

    file->read(fileContents, filesize);

    criTagData_t tagData = readCRITags(fileContents,filesize);

    delete[] fileContents;

    bool isInvalid = false;
    std::vector<byte>* tmpValue;
    if (tagData.find(FrameInfo) != tagData.end())
    {
        tmpValue = &tagData[FrameInfo];
        width = *((uint32_t*)&(*tmpValue)[0]);
        height = *((uint32_t*)&(*tmpValue)[4]);
        //width = (int)BitConverter.ToUInt32(tmpValue, 0);
        //height = (int)BitConverter.ToUInt32(tmpValue, 4);

        // Bayer pattern and bit depth.
        uint32_t colorModel = *((uint32_t*)&(*tmpValue)[8]);
        //UInt32 colorModel = BitConverter.ToUInt32(tmpValue, 8);
        switch (colorModel)
        {

        case COLOR_MODEL_BAYER_GRGR_CINTEL_10:
        {
            byte bayerPatternLocal[2][2] ={ { 1, 0 }, { 2, 1 } };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            //throw new Exception("10 bit modes not supported (yet?)");
            //rawDataFormat = RAWDATAFORMAT.BAYERRG12p;
            mustDo10bitUnpack = true;
            rawDataFormat = BAYER12BITBRIGHTCAPSULEDIN16BIT;
        }
            break;

            // 12 bit modes
            // NOTE: Untested so far!
        case COLOR_MODEL_BAYER_BGGR_CINTEL_12:
            {
                byte bayerPatternLocal[2][2] = { { 2, 1 }, { 1, 0 } };
                *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
                rawDataFormat = BAYERRG12p; 
            }
            break;
        case COLOR_MODEL_BAYER_GBGB_CINTEL_12:
        {
            byte bayerPatternLocal[2][2] = { { 1, 2 }, { 0, 1 } };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            rawDataFormat = BAYERRG12p;
        }
            break;
        case COLOR_MODEL_BAYER_RGRG_CINTEL_12:
        {
            byte bayerPatternLocal[2][2] = { {0, 1},{1 ,2} };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            rawDataFormat = BAYERRG12p;
        }
            break;
        case COLOR_MODEL_BAYER_GRGR_CINTEL_12:
        {
            byte bayerPatternLocal[2][2] = { { 1, 0 }, { 2, 1 } };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            rawDataFormat = BAYERRG12p;
        }
            break;

            // 16 bit modes
        case COLOR_MODEL_BAYER_BGGR_CINTEL_16:
        {
            byte bayerPatternLocal[2][2] = { { 2, 1 }, { 1, 0 } };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            rawDataFormat = BAYER12BITDARKCAPSULEDIN16BIT;
        }
            break;
        case COLOR_MODEL_BAYER_GBGB_CINTEL_16:
        {
            byte bayerPatternLocal[2][2] = { { 1, 2 }, { 0, 1 } };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            rawDataFormat = BAYER12BITDARKCAPSULEDIN16BIT;
        }
            break;
        case COLOR_MODEL_BAYER_RGRG_CINTEL_16:
        {
            byte bayerPatternLocal[2][2] = { {0, 1},{1 ,2} };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            rawDataFormat = BAYER12BITDARKCAPSULEDIN16BIT;
        }
            break;
        case COLOR_MODEL_BAYER_GRGR_CINTEL_16:
        {
            byte bayerPatternLocal[2][2] = { { 1, 0 }, { 2, 1 } };
            *(bayerPattern_t*)bayerPattern = *(bayerPattern_t*)bayerPatternLocal;
            rawDataFormat = BAYER12BITDARKCAPSULEDIN16BIT;
        }
            break;

        default:
            break;
        }


    }
    else
    {
        isInvalid = true;
    }
    
    if (isInvalid)
    {
        COR_LOG("No width/height tag data found in CRI file");
        return NULL;
    }





    if (tagData.find(FrameData) != tagData.end())
    {

        // Detect compression
        // Only horizontal tiles are supported so far. Assuming there is no vertical tiling.
        if (tagData.find(TileSizes) != tagData.end())
        {
            byte* decodedOutputBuffer = new byte[width * height * 2];

            std::vector<byte>* tileSizeData = &tagData[TileSizes];
            int tileCount = tileSizeData->size() / 8; // The tilesizes are saved as Uint64s I think, so dividing by 8 should give the right number.

            uint64_t totalSizeFromTileSizes = 0;
            uint64_t* tileSizes = new uint64_t[tileCount];
            for (int i = 0; i < tileCount; i++)
            {
                //tileSizes[i] = BitConverter.ToUInt64(tileSizeData, i * 8);
                tileSizes[i] = *(uint64_t*)&(*tileSizeData)[i*8];
                totalSizeFromTileSizes += tileSizes[i];
            }

            std::vector<byte>* compressedData = &tagData[FrameData];


            //JpegDecoder jpegLibraryDecoder = new JpegDecoder();
            //ReadOnlyMemory<byte> rawTileReadOnlyMemory;

            byte* tmpBuffer;
            uint64_t alreadyRead = 0;
            uint32_t horizOffset = 0;
            for (int tileNum = 0; tileNum < tileCount;tileNum++) {
                uint64_t tileSize = tileSizes[tileNum];

            //}
            //for each(uint64_t tileSize in tileSizes)
            //{
                tmpBuffer = new byte[tileSize];

                // Catch damaged CRI files:
                // 
                bool isDamaged = false;
                if (alreadyRead + tileSize <= (uint64_t)compressedData->size()) {

                    //Array.Copy(compressedData, (int)alreadyRead, tmpBuffer, 0, (int)tileSize);
                    memcpy(tmpBuffer,compressedData->data() + alreadyRead, tileSize);
                }
                else if (alreadyRead > ((uint64_t)compressedData->size() - 1)) // See if we can get anything at all out of this...
                {
                    //errorInfo.addError(new ISSError(ISSError.ErrorCode.ORIGINAL_FILE_PARTIALLY_CORRUPTED_RESCUE, ISSError.ErrorSeverity.SEVERE, getImageName(index), "Source CRI file corrupted! Image info for tile starting at " + alreadyRead + " completely missing. Ignoring, but output image will be obviously missing that part.", new byte[0]));
                    //COR_LOG( va("Source CRI file corrupted! Image info for tile starting at %d completely missing. Ignoring, but output image will be obviously missing that part.", alreadyRead));
                    COR_LOG( "Source CRI file corrupted! Image info for tile completely missing. Ignoring, but output image will be obviously missing that part.");
                    // completely broken. No use messing with this file anymore. 
                    break;
                }
                else
                { // If there's a little bit of stuff, try rescuing what we can.
                    //errorInfo.addError(new ISSError(ISSError.ErrorCode.ORIGINAL_FILE_PARTIALLY_CORRUPTED_RESCUE, ISSError.ErrorSeverity.SEVERE, getImageName(index), "Source CRI file corrupted! Image info for tile starting at " + alreadyRead + " partially missing. Attempting to rescue what's left.", new byte[0]));
                    //COR_LOG("Source CRI file corrupted! Image info for tile starting at " + alreadyRead + " partially missing. Attempting to rescue what's left.");
                    COR_LOG("Source CRI file corrupted! Image info for tile partially missing. Attempting to rescue what's left.");

                    uint64_t amountToCopy = (uint64_t)compressedData->size() - alreadyRead;
                    isDamaged = true;
                    //Array.Copy(compressedData, (int)alreadyRead, tmpBuffer, 0, (int)amountToCopy);
                    memcpy(tmpBuffer, compressedData->data() + alreadyRead, amountToCopy);
                }

                alreadyRead += tileSize;

                //rawTileReadOnlyMemory = new ReadOnlyMemory<byte>(tmpBuffer);
                //jpegLibraryDecoder.SetInput(rawTileReadOnlyMemory);
                //jpegLibraryDecoder.SetFrameHeader()
                //jpegLibraryDecoder.Identify(); // fails to identify. missing markers or whatever: Failed to decode JPEG data at offset 91149. No marker found.'


                //dng_stream stream = new dng_stream(tmpBuffer);
                dng_memory_stream_simple stream(tmpBuffer, tileSize);
                
                //dng_spooler spooler = new dng_spooler();
                dng_memory_spooler_simple spooler;

                uint32_t tileWidth = 0, tileHeight = 0;

                if (isDamaged)
                {
                    // Be careful if damaged. 
                    try
                    {

                        // TODO: Replace AVX dynamically using compile time settings
                        //DecodeLosslessJPEG(stream, spooler, ref tileWidth, ref tileHeight, false);
                        ::DecodeLosslessJPEGMod<(::SIMDType)::AVX>(stream,spooler,tileWidth, tileHeight,false, (uint64)tileSize);
                    }
                    catch (...)
                    {
                        //COR_LOG("Source CRI file corrupted! A rescue of remaining data was attempted but possibly failed with error: " + e.Message, new byte[0]));
                        COR_LOG("Source CRI file corrupted! A rescue of remaining data was attempted but possibly failed.");

                    }
                }
                else
                {

                    //DNGLosslessDecoder.DecodeLosslessJPEGProper(stream, spooler, ref tileWidth, ref tileHeight, false);
                    ::DecodeLosslessJPEGMod<(::SIMDType)::AVX>(stream, spooler, tileWidth, tileHeight, false, (uint64)tileSize);
                }



                uint32_t tileActualWidth = tileWidth / 2;
                uint32_t tileActualHeight = tileHeight * 2;
                //byte[] tileBuff = new byte[jpegLibraryDecoder.Width * jpegLibraryDecoder.Height * 2];
                uint64_t actualTileBuffSize = tileActualWidth * tileActualHeight * 2;
                byte* tileBuff =new byte[actualTileBuffSize];
                std::vector<byte>* tileBuffTmp = spooler.getDataVectorPointer();

                /*if (isDamaged)
                {
                    tileBuff
                    //tileBuff = new byte[tileActualWidth * tileActualHeight * 2];
                    //byte[] tileBuffTmp = spooler.toByteArray();
                    //Array.Copy(tileBuffTmp, 0, tileBuff, 0, tileBuffTmp.Length);
                }
                else
                {
                    
                    //tileBuff = spooler.toByteArray();
                }*/
                memcpy(tileBuff, tileBuffTmp->data(), std::min(actualTileBuffSize, tileBuffTmp->size()));


                int actualX;
                for (int y = 0; y < tileActualHeight; y++)
                {
                    for (int x = 0; x < tileActualWidth; x++)
                    {
                        actualX = (uint32_t)horizOffset + x;
                        decodedOutputBuffer[y * width * 2 + actualX * 2] = tileBuff[y * tileActualWidth * 2 + (x) * 2];
                        decodedOutputBuffer[y * width * 2 + actualX * 2 + 1] = tileBuff[y * tileActualWidth * 2 + (x) * 2 + 1];

                    }
                }

                horizOffset += (uint32_t)tileActualWidth;

                delete[] tmpBuffer;
                delete[] tileBuff;
            }

            //delete[] decodedOutputBuffer;
            delete[] tileSizes;

            PixelFormat format= PF_R16G16B16;
            return new SimpleImage(width, height/4, format, decodedOutputBuffer);

            //File.WriteAllBytes("decoded raw cri"+width + " "+ height + ".raw", decodedOutputBuffer);
            //return decodedOutputBuffer;
        }
        else
        {

            // Presuming uncompressed
            if (mustDo10bitUnpack)
            {
                byte* decodedOutputBuffer = new byte[width * height * 2];
                std::vector<byte>* srcData = &tagData[FrameData];
                unpack_10bit(&(*srcData)[0],srcData->size(),(uint16_t*) decodedOutputBuffer, 4,width, height,(ptrdiff_t)width);
                PixelFormat format = PF_R16G16B16;
                return new SimpleImage(width, height/4, format, decodedOutputBuffer);
            }
            else {
                // This is probably wrong and will conversion to 16 bit. Idk.
                std::vector<byte>* srcData = &tagData[FrameData];
                byte* srcDataForReturn = new byte[std::max((int)srcData->size(), width * height * 2)]; // Be safe. This will probably need some more fixing anyway, but at least avoid crashes.
                memcpy(srcDataForReturn,&(*srcData)[0],srcData->size());
                PixelFormat format = PF_R16G16B16;
                return new SimpleImage(width, height/4, format, srcDataForReturn);
            }
        }

        //File.WriteAllBytes("rawcri.jpg", tagData[(UInt32)Key.FrameData]);

    }
    else
    {
        COR_LOG("CRI file contains no image data apparently.");
    }

    return NULL;

    /*
    array2D<float>* red;
    array2D<float>* green;
    array2D<float>* blue;

    //abasc << "before read file" << "\n";
    //abasc.flush();

    file->seek(0, corona::File::END);
    uint64_t filesize = file->tell();
    file->seek(0, corona::File::BEGIN);

    byte* fileContents = new byte[filesize];

    file->read(fileContents, filesize);

    //rawspeed::FileReader reader(path.c_str());
    const rawspeed::Buffer* map = new rawspeed::Buffer(fileContents,filesize);

    rawspeed::RawParser parser(map);
    rawspeed::RawDecoder* decoder = parser.getDecoder().release();

    rawspeed::CameraMetaData* metadata = new rawspeed::CameraMetaData();

    //decoder->uncorrectedRawValues = true;
    decoder->decodeRaw();
    decoder->decodeMetaData(metadata);
    rawspeed::RawImage raw = decoder->mRaw;

    delete metadata;
    delete map;
    delete[] fileContents;

    int components_per_pixel = raw->getCpp();
    rawspeed::RawImageType type = raw->getDataType();
    bool is_cfa = raw->isCFA;

    int dcraw_filter = 0;
    if (true == is_cfa) {
        rawspeed::ColorFilterArray cfa = raw->cfa;
        dcraw_filter = cfa.getDcrawFilter();
        //rawspeed::CFAColor c = cfa.getColorAt(0, 0);
    }
    abasc << "dcraw filter " << dcraw_filter << "\n";

    unsigned char* data = raw->getData(0, 0);
    int rawwidth = raw->dim.x;
    int rawheight = raw->dim.y;
    int pitch_in_bytes = raw->pitch;

    unsigned int bpp = raw->getBpp();

    int width = rawwidth;
    int height = rawheight;


    uint16_t* dataAs16bit = reinterpret_cast<uint16_t*>(data);

    double maxValue = pow(2.0, 16.0) - 1;
    double tmp;

    abasc << "before rawimage" << "\n";
    abasc.flush();

    rtengine::RawImage* ri = new rtengine::RawImage();
    ri->filters = dcraw_filter;
    rtengine::RawImageSource* rawImageSource = new rtengine::RawImageSource();
    rawImageSource->ri = ri;


    array2D<float>* demosaicSrcData = new array2D<float>(width, height, 0U);
    red = new array2D<float>(width, height, 0U);
    green = new array2D<float>(width, height, 0U);
    blue = new array2D<float>(width, height, 0U);

#ifdef _OPENMP
#pragma omp for
#endif
    for (int y = 0; y < height; y++) {
#ifdef _OPENMP
#pragma omp simd
#endif
        for (size_t x = 0; x < width; x++) {
            //tmp = ;// maxValue;
            (*demosaicSrcData)[y][x] = dataAs16bit[y * pitch_in_bytes / 2 + x];
        }
    }

    delete decoder;

    rawImageSource->amaze_demosaic_RT(0, 0, width, height, *demosaicSrcData, *red, *green, *blue);

    delete ri;
    delete rawImageSource;
    demosaicSrcData->free();
    delete demosaicSrcData;

    abasc << "before deleting" << "\n";
    abasc.flush();



    uint16_t* pixels = new uint16_t[width * height * 3];  // allocate when we know the format
    PixelFormat format;

#ifdef _OPENMP
#pragma omp for
#endif
    for (int y = 0; y < height; y++) {
        uint16_t* pixelsHere = pixels + y * width *3;
#ifdef _OPENMP
#pragma omp simd
#endif
        for (int x = 0; x < width; x++) {
            pixelsHere[x * 3] = (*blue)[y][x];
            pixelsHere[x * 3 + 1] = (*green)[y][x];
            pixelsHere[x * 3 + 2] = (*red)[y][x];
        }
    }
    */



  }

  //////////////////////////////////////////////////////////////////////////////

}
