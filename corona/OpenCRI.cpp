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


//#include "common.h"
//#include <sysinfoapi.h>



namespace corona {

  //typedef unsigned char byte;

  

  //////////////////////////////////////////////////////////////////////////////

  Image* OpenCRI(File* file) {

    COR_GUARD("OpenCRI");


    PixelFormat format;
    format = PF_R16G16B16;



    //return new SimpleImage(width, height, format, (byte*)pixels);
    return new SimpleImage(100, 100, format, (byte*)0);


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
