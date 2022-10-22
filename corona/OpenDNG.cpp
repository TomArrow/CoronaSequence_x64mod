/**
 *  @todo  use our own longjmp instead of libpng's.  this way we don't need
 *         to use PNG_SETJMP_SUPPORTED in Windows, and don't depend on
 *         png_ptr->jmpbuf in older versions of libpng.
 */
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

#include "RawSpeed-API.h"
#include "rawspeedconfig.h"

 //#include "amaze/rtengine/array2D.h"
#include "amaze/rtengine/amaze_demosaic_RT.h"


#include "Debug.h"
#include "Open.h"
#include "SimpleImage.h"
#include "Utility.h"


#ifdef DEBUGOUTPUT
std::ofstream abasc;
#else
std::ofstream nothing;
//#define abasc if(0) nothing
#endif
#include "common.h"
#include <sysinfoapi.h>

int rawspeed_get_number_of_processor_cores() {
    int numberOfCores = 1;
    if (numberOfCores <= 1)
    {
/*#ifndef WIN32
        // get number of CPUs using Mach calls
        host_basic_info_data_t hostInfo;
        mach_msg_type_number_t infoCount;

        infoCount = HOST_BASIC_INFO_COUNT;
        host_info(mach_host_self(), HOST_BASIC_INFO,
            (host_info_t)&hostInfo, &infoCount);

        numberOfCores = hostInfo.max_cpus;*/
//#else
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);

        numberOfCores = systemInfo.dwNumberOfProcessors;
//#endif
    }
    return numberOfCores;
}


namespace corona {

  typedef unsigned char byte;

  

  //////////////////////////////////////////////////////////////////////////////

  Image* OpenDNG(File* file) {

    COR_GUARD("OpenDNG");

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

    /*if (rowBytes == 0) {
        // Seemingly must be 64-byte aligned from observation. Ideally we pass this info in here straight from Premiere/AE
        // But if we don't have the info (like when called from GetInfo function), we assume 64-byte alignment.
        rowBytes = width * sizeof(float) * 4;
        if (rowBytes % 64) {
            rowBytes = (rowBytes / 64 + 1) * 64;
        }
    }*/

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


    format = PF_R16G16B16;
    /*outputBufferLength = height * rowBytes / 4;
    outputBuffer = new float[outputBufferLength];

    int rowFloats = rowBytes / 4;
#ifdef _OPENMP
#pragma omp for
#endif
    for (int y = 0; y < height; y++) {
        float* outputBufferHere = outputBuffer + y * rowFloats;
#ifdef _OPENMP
#pragma omp simd
#endif
        for (int x = 0; x < width; x++) {
            outputBufferHere[x * 4] = (float)((*blue)[y][x] / maxValue);
            outputBufferHere[x * 4 + 1] = (float)((*green)[y][x] / maxValue);
            outputBufferHere[x * 4 + 2] = (float)((*red)[y][x] / maxValue);
            outputBufferHere[x * 4 + 3] = 1.0f;
        }
    }*/

    red->free();
    green->free();
    blue->free();
    delete red;
    delete green;
    delete blue;


    return new SimpleImage(width, height, format, (byte*)pixels);


    /*// verify PNG signature
    byte sig[8];
    file->read(sig, 8);
    if (png_sig_cmp(sig, 0, 8)) {
      return 0;
    }

    COR_LOG("Signature verified");

    // read struct
    png_structp png_ptr = png_create_read_struct(
      PNG_LIBPNG_VER_STRING,
      NULL, NULL, NULL);
    if (!png_ptr) {
      return 0;
    }

    COR_LOG("png struct created");

    // info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return 0;
    }

    COR_LOG("info struct created");

    // the PNG error function calls longjmp(png_ptr->jmpbuf)
    if (setjmp(png_jmpbuf(png_ptr))) {
      COR_LOG("Error loading PNG");
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 0;
    }

    COR_LOG("setjmp() succeeded");

    // set the error function
    png_set_error_fn(png_ptr, 0, PNG_error_function, PNG_warning_function);

    // read the image
    png_set_read_fn(png_ptr, file, PNG_read_function);
    png_set_sig_bytes(png_ptr, 8);  // we already read 8 bytes for the sig
    // always give us 8-bit samples (strip 16-bit and expand <8-bit)
    int png_transform = PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_EXPAND;
    png_read_png(png_ptr, info_ptr, png_transform, NULL);

    COR_LOG("PNG read");

    if (!png_get_rows(png_ptr, info_ptr)) {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 0;
    }

    int width  = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    byte* pixels = 0;  // allocate when we know the format
    PixelFormat format;
    byte* palette = 0;
    PixelFormat palette_format;

    // decode based on pixel format
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int num_channels = png_get_channels(png_ptr, info_ptr);
    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

    // 32-bit RGBA
    if (bit_depth == 8 && num_channels == 4) {
      COR_LOG("32-bit RGBA: bit_depth = 8 && num_channels = 4");

      format = PF_R8G8B8A8;
      pixels = new byte[width * height * 4];
      for (int i = 0; i < height; ++i) {
        memcpy(pixels + i * width * 4, row_pointers[i], width * 4);
      }

    // 24-bit RGB
    } else if (bit_depth == 8 && num_channels == 3) {
      COR_LOG("24-bit RGB: bit_depth = 8 && num_channels = 3");

      format = PF_R8G8B8;
      pixels = new byte[width * height * 3];
      for (int i = 0; i < height; ++i) {
        memcpy(pixels + i * width * 3, row_pointers[i], width * 3);
      }

    // palettized or greyscale with alpha
    } else if (bit_depth == 8 && (num_channels == 2 || num_channels == 1)) {
      png_color png_palette[256];
      fill_palette(png_ptr, info_ptr, png_palette);

      if (num_channels == 2) {
        COR_LOG("bit_depth = 8 && num_channels = 2");

	format = PF_R8G8B8A8;
	pixels = new byte[width * height * 4];
	byte* out = pixels;

        for (int i = 0; i < height; ++i) {
          byte* in = row_pointers[i];
          for (int j = 0; j < width; ++j) {
            byte c = *in++;
            *out++ = png_palette[c].red;
            *out++ = png_palette[c].green;
            *out++ = png_palette[c].blue;
            *out++ = *in++;  // alpha
          }
        }

      } else { // (num_channels == 1)
        COR_LOG("bit_depth = 8 && num_channels = 1");

        pixels = new byte[width * height];
        format = PF_I8;
        palette = new byte[256 * 4];
        palette_format = PF_R8G8B8A8;


        // get the transparent palette flags
        png_bytep trans;
        int num_trans = 0;
        png_color_16p trans_values; // XXX not used - should be?
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &trans_values);

        // copy the palette from the PNG
        for (int i = 0; i < 256; ++i) {
          palette[i * 4 + 0] = png_palette[i].red;
          palette[i * 4 + 1] = png_palette[i].green;
          palette[i * 4 + 2] = png_palette[i].blue;
          palette[i * 4 + 3] = 255;
        }
        // apply transparency to palette entries
        for (int i = 0; i < num_trans; ++i) {
          palette[trans[i] * 4 + 3] = 0;
        }

        byte* out = pixels;
        for (int i = 0; i < height; ++i) {
          memcpy(out, row_pointers[i], width);
          out += width;
        }
      }

    } else {  // unknown format
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 0;
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    if (palette) {
      return new SimpleImage(width, height, format, pixels,
			     palette, 256, palette_format);
    } else {
      return new SimpleImage(width, height, format, pixels);
    }*/
  }

  //////////////////////////////////////////////////////////////////////////////

}
