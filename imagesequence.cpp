//////////////////////////////////////////////////////////////////////
//
// Original by Bzzz - June 2002
// 
// 2003-03-01
// extensions by WarpEnterprises & sh0dan
// Corona added by WarpEnterprises - much faster !
//
// 2006-01-19: complete rework. compiled with corona, imglib removed
// 
// 2021-09-26: Recreated in VS2019 to build a x64 binary (by TomArrow)
//
//////////////////////////////////////////////////////////////////////


#include <stdlib.h>
#include <math.h>
#include <objbase.h>
#include <vfw.h>
#include <stdio.h>
#include <io.h>
#include "FCNTL.H"

#define AVS_LINKAGE_DLLIMPORT
#include "avs/avisynth.h"
#include "corona/corona.h"
#include "filesearch.h"

#define maxwidth 2880	//for RawSequence

class ImageSequence : public IClip  
{
	VideoInfo vi;
	int posx, posy;
	int textmode;
	int level;	//marker to use Invoke only in first call of GetFrame
	FileSearch fs;	//contains the found files
	char error_msg[256];	//contains an error message from contructor or Invoke
	char fullname[256], drive[3], dir[256], fname[256], ext[10];
	int size, text_color, halo_color;
	char expression[256];
	char font[256];
//only for RawSequence:
	char pixel_type[32];

public:
	ImageSequence(const char name[], const int _start, const int _stop, const int _sort, const bool _gapless, 
		const int _textmode, const int _posx, const int _posy,
		const char _font[], const int _size, const int _text_color, const int _halo_color,
		const char _pixel_type[], const int _width, const int _height, const char _expression[],
		IScriptEnvironment* env) {
			
		int i;
		corona::Image* pImage=0;

//default values
		vi.width = 512;
		vi.height = 24;
		vi.fps_numerator = 25;
		vi.fps_denominator = 1;
		vi.num_frames = _stop - _start + 1;
		vi.SetFieldBased(false);
		vi.pixel_type = VideoInfo::CS_BGR32;	//always with alpha
		vi.audio_samples_per_second=0;

		strcpy(error_msg, "");
		level = 0;

		posx = _posx;
		posy = _posy;
		textmode = _textmode;
		strcpy(font, _font);
		size = _size;
		halo_color = _halo_color;
		text_color = _text_color;
		strcpy(expression, _expression);

		_splitpath(name, drive, dir, fname, ext);
		strcpy(fullname, drive);
		strcat(fullname, dir);
		fs.AddDir(fullname);

		strcat(fname, ext);

		// printing and checking files
		if (strpbrk(fname, "*?")==NULL)  {
			fs.PrintFiles(fname, _start, _stop, _gapless);
		} else {
		//searching for files
			fs.SearchSubDirs();
			fs.SearchFiles(fname);
			if (fs.GetFilecount()==0) {
				strcpy(error_msg, "ImageSequence: no files found");
				return;
			}
		}

//1..path and name, 2..name, 3..time
		fs.BubbleSort(_sort);

//The first FOUND image is read to get the Video size
		for (i=0; i<fs.GetFilecount(); i++) {	// scan for first found image
			if (fs.GetFiledate(i) != NULL) {
				break;
			}
		};
		if (i>=fs.GetFilecount()) {
			strcpy(error_msg, "ImageSequence: no accessible files found");
			return;
		};

// --> source specific
		strcpy(pixel_type, _pixel_type);
		if (strlen(pixel_type)==0) {
// CORONA 
			pImage = corona::OpenImage(fs.GetFilename(i));// read image file
			if (!pImage) {	// not a single picture found - returning message to clip, don't raise an error
				strcpy(error_msg, "CoronaSequence: first picture found cannot be decoded");
				return;
			};
			vi.width = pImage->getWidth();
			vi.height = pImage->getHeight();
			vi.num_frames = fs.GetFilecount();
			delete pImage;
		} else {
// RawSequence
			if (_width > maxwidth) env->ThrowError("Width too big.");
			strcpy(pixel_type, _pixel_type);
			vi.width = _width;
			vi.height = _height;
			vi.num_frames = fs.GetFilecount();
	
			vi.pixel_type = VideoInfo::CS_UNKNOWN;

			if (!_stricmp(pixel_type,"RGB")) vi.pixel_type = VideoInfo::CS_BGR24;
			if (!_stricmp(pixel_type,"BGR")) vi.pixel_type = VideoInfo::CS_BGR24;
			if (!_stricmp(pixel_type,"RGBA")) vi.pixel_type = VideoInfo::CS_BGR32;
			if (!_stricmp(pixel_type,"BGRA")) vi.pixel_type = VideoInfo::CS_BGR32;
			if (!_stricmp(pixel_type,"YUYV")) vi.pixel_type = VideoInfo::CS_YUY2;
			if (!_stricmp(pixel_type,"UYVY")) vi.pixel_type = VideoInfo::CS_YUY2;
			if (!_stricmp(pixel_type,"YVYU")) vi.pixel_type = VideoInfo::CS_YUY2;
			if (!_stricmp(pixel_type,"VYUY")) vi.pixel_type = VideoInfo::CS_YUY2;
			if (!_stricmp(pixel_type,"YV16")) vi.pixel_type = VideoInfo::CS_YUY2;
			if (!_stricmp(pixel_type,"I420")) vi.pixel_type = VideoInfo::CS_YV12;
			if (!_stricmp(pixel_type,"YV12")) vi.pixel_type = VideoInfo::CS_YV12;

			if (vi.pixel_type == VideoInfo::CS_UNKNOWN) env->ThrowError("Invalid pixel type. Supported: RGB, RGBA, BGR, BGRA, YUYV, UYVY, YVYU, VYUY, YV16, YV12, I420");

		}
// <-- source specific
	}

	~ImageSequence(){
	}


	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env)
	{
		int viScanLine;
		corona::Image * pImage;
		byte * pPixels;
		unsigned char* pdst;
		PVideoFrame dst;
		PClip resultClip;
		char message[255];
		//RawSequence only
		unsigned char rawbuf[maxwidth*4];	//max linesize 4Bytes * maxwidth
		int h_rawfile;	//file handle for 64bit operations
		__int64 filelen;
		int framesize;
		int bytes_per_line;
		int mapping[4];
		int mapcnt;
		int i, j, k;
		int ret;

//don't recurse when using Invoke.Subtitle
		if (level==0) {
			strcpy(message, "");
			//args[] contains all necessary arguments for calling Subtitle
			const char* arg_names[9] = {   0,                 0,  "x", "y", "first_frame", "font", "size", "text_color", "halo_color"};
			AVSValue args[9]         = {this, AVSValue(message),posx,posy,            -1,  font,   size,   text_color,   halo_color};

			//error from constructor
			if (strcmp(error_msg, "")) {
				args[1] = AVSValue(error_msg);
				level = 1;
				resultClip = (env->Invoke("Subtitle",AVSValue(args, 9), arg_names )).AsClip();
				dst = resultClip->GetFrame(-1, env);	//get dummy frame
				level = 0;
				return dst;
			}

			//print filename
			if (textmode==2)
			{
				strcpy(message, fs.GetFilename(n));
			}
			else if (textmode==1)
			{
				_splitpath(fs.GetFilename(n), drive, dir, fname, ext);
				strcpy(message, fname);
				strcat(message, ext);
			}
			else if (textmode==3)
			{
				//eval parameter "text" using variable "filename"
				strcpy(message, fs.GetFilename(n));
				env->SetVar("filename", args[1]);
				args[1] = env->Invoke("Eval", expression, 0);
			}

			level = 1;
			strcpy(error_msg, "");
			resultClip = (env->Invoke("Subtitle", AVSValue(args, 9), arg_names )).AsClip();
			dst = resultClip->GetFrame(n, env);

			//create dummy frame for error display
			if ((textmode != -1) && (strcmp(error_msg, ""))) {
				strcpy(message, fs.GetFilename(n));
				strcat(message, ": ");
				strcat(message, error_msg);
				strcpy(error_msg, "");
				args[1] = AVSValue(message);
				resultClip = (env->Invoke("Subtitle",AVSValue(args, 9), arg_names )).AsClip();
				dst = resultClip->GetFrame(-1, env);	//get dummy frame
			}
			level = 0;
			return dst;
		}

		dst = env->NewVideoFrame(vi);
		pdst = dst->GetWritePtr();	//init frame
		memset(pdst, 0, dst->GetPitch() * dst->GetHeight());

		if (n>=0) {	//else return black frame

// --> source specific
			if (strlen(pixel_type)==0) {
// Corona 
				pImage = corona::OpenImage(fs.GetFilename(n), corona::PF_B8G8R8A8);
				// do nothing if file does not exist or is not readable
				if (pImage) {
					if ((pImage->getWidth() != vi.width) || (pImage->getHeight() != vi.height)) {
						strcpy(error_msg, "All images must have the same size !");
					} else {
						pPixels = (byte*)pImage->getPixels();
						viScanLine = dst->GetPitch();
						for (int i=0;i<vi.height;i++) {
							memcpy(pdst + (i*viScanLine), pPixels + (vi.height-i-1)*4*vi.width, 4*vi.width);
						}
					}
					delete pImage;
				} else {
					strcpy(error_msg, "Image could not be opened!");
				}
			} else {
// RawSequence
				h_rawfile = _open(fs.GetFilename(n), _O_BINARY | _O_RDONLY);
				if (h_rawfile==-1) {
					strcpy(error_msg, "Image could not be opened");
				} else {
					filelen = _filelengthi64(h_rawfile);
					bytes_per_line = vi.width * vi.BitsPerPixel() / 8;
					framesize = bytes_per_line * vi.height;
					if (filelen < (__int64)framesize) {
						strcpy(error_msg, "File too small.");
					} else {

						if (!_stricmp(pixel_type, "I420") || !_stricmp(pixel_type, "YV12")) {
	//planar formats. Assuming 4 luma pixels with 1 chroma pair
							pdst = dst->GetWritePtr(PLANAR_Y);
							for (i=0; i<vi.height; i++) {
								memset(rawbuf, 0, maxwidth * 4);
								ret = _read(h_rawfile, rawbuf, vi.width);	//luma bytes
								memcpy (pdst, rawbuf, vi.width);
								pdst = pdst + dst->GetPitch(PLANAR_Y);
							}

							//switching between UV and VU
							if (!_stricmp(pixel_type, "I420")) {
								pdst = dst->GetWritePtr(PLANAR_U);
							} else {
								pdst = dst->GetWritePtr(PLANAR_V);
							}
							for (i=0; i<vi.height/2; i++) {
								memset(rawbuf, 0, maxwidth * 4);
								ret = _read(h_rawfile, rawbuf, vi.width/2);	//chroma bytes
								memcpy (pdst, rawbuf, vi.width/2);
								pdst = pdst + dst->GetPitch(PLANAR_U);
							}

							if (!_stricmp(pixel_type, "I420")) {
								pdst = dst->GetWritePtr(PLANAR_V);
							} else {
								pdst = dst->GetWritePtr(PLANAR_U);
							}
							for (i=0; i<vi.height/2; i++) {
								memset(rawbuf, 0, maxwidth * 4);
								ret = _read(h_rawfile, rawbuf, vi.width/2);	//chroma bytes
								memcpy (pdst, rawbuf, vi.width/2);
								pdst = pdst + dst->GetPitch(PLANAR_V);
							}
						} else {
							if (!_stricmp(pixel_type,"YV16")) {
	// YB16 - convert to YUY2 by rearranging bytes
								pdst = dst->GetWritePtr();
								//read and copy all luma lines
								for (i=0; i < vi.height; i++) {
										memset(rawbuf, 0, maxwidth * 4);
										ret = _read(h_rawfile, rawbuf, bytes_per_line/2);
										for (j=0; j<(bytes_per_line/4); j++) {
												pdst[j*4] = rawbuf[j*2];
												pdst[j*4+2] = rawbuf[j*2+1];
										}
										pdst = pdst + dst->GetPitch();
								}
								pdst = dst->GetWritePtr();
								//read and copy all U lines
								for (i=0; i < vi.height; i++) {
										ret = _read(h_rawfile, rawbuf, bytes_per_line/4);
										for (j=0; j<(bytes_per_line/4); j++) {
												pdst[j*4+1] = rawbuf[j]; // u-samples
										}
										pdst = pdst + dst->GetPitch();
								}
								pdst = dst->GetWritePtr();
								//read and copy all V lines
								for (int i=0; i < vi.height; i++) {
										ret = _read(h_rawfile, rawbuf, bytes_per_line/4);
										for (j=0; j<(bytes_per_line/4); j++) {
												pdst[j*4+3] = rawbuf[j]; // v-samples
										}
										pdst = pdst + dst->GetPitch();
								}
							} else {
	//interleaved formats
								if (!_stricmp(pixel_type,"RGB")) {
									mapping[0] = 2;
									mapping[1] = 1;
									mapping[2] = 0;
									mapcnt = 3;
								}

								if (!_stricmp(pixel_type,"BGR")) {
									mapping[0] = 0;
									mapping[1] = 1;
									mapping[2] = 2;
									mapcnt = 3;
								}

								if (!_stricmp(pixel_type,"RGBA")) {
									mapping[0] = 2;
									mapping[1] = 1;
									mapping[2] = 0;
									mapping[3] = 3;
									mapcnt = 4;
								}
								
								if (!_stricmp(pixel_type,"BGRA")) {
									mapping[0] = 0;
									mapping[1] = 1;
									mapping[2] = 2;
									mapping[3] = 3;
									mapcnt = 4;
								}

								if (!_stricmp(pixel_type,"YUYV")) {
									mapping[0] = 0;
									mapping[1] = 1;
									mapcnt = 2;
								}
								
								if (!_stricmp(pixel_type,"UYVY")) {
									mapping[0] = 1;
									mapping[1] = 0;
									mapcnt = 2;
								}

								if (!_stricmp(pixel_type,"YVYU")) {
									mapping[0] = 0;
									mapping[1] = 3;
									mapping[2] = 2;
									mapping[3] = 1;
									mapcnt = 4;
								}
								
								if (!_stricmp(pixel_type,"VYUY")) {
									mapping[0] = 1;
									mapping[1] = 2;
									mapping[2] = 3;
									mapping[3] = 0;
									mapcnt = 4;
								}

								pdst = dst->GetWritePtr();

								//read and copy all lines
								for (i=0; i<vi.height; i++) {
									memset(rawbuf, 0, maxwidth * 4);
									ret = _read(h_rawfile, rawbuf, bytes_per_line);
									for (j=0; j<(bytes_per_line/mapcnt); j++) {
										for (k=0; k<mapcnt; k++) {
											pdst[j*mapcnt + k] = rawbuf[j*mapcnt + mapping[k]];
										}
									}
									pdst = pdst + dst->GetPitch();
								}
							}
						}
					}
				}
				_close(h_rawfile);//thanks to Rick!
			}
		}
// <-- source specific
		return dst;
	}

	// avisynth virtual functions
	bool __stdcall GetParity(int n) { return false; }
	void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {};	//do nothing
	const VideoInfo& __stdcall GetVideoInfo() { return vi;}
	//void __stdcall SetCacheHints(int cachehints,int frame_range) {};	//do nothing
	int __stdcall SetCacheHints(int cachehints, int frame_range) { return CACHE_NOTHING; };	//do nothing
};

AVSValue __cdecl Create_CoronaSequence(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new ImageSequence(args[0].AsString(),
		args[1].AsInt(0), args[2].AsInt(1500), args[3].AsInt(0), args[4].AsBool(false),
		args[5].AsInt(0), args[6].AsInt(8), args[7].AsInt(12),
		args[8].AsString("Arial"), args[9].AsInt(15), args[10].AsInt(0xFFFFFF), args[11].AsInt(0x000000), 
		"", 0, 0, args[12].AsString(""),
		env);
}

AVSValue __cdecl Create_RawSequence(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new ImageSequence(args[0].AsString(),
		args[1].AsInt(0), args[2].AsInt(1500), args[3].AsInt(0), args[4].AsBool(false),
		args[5].AsInt(0), args[6].AsInt(8), args[7].AsInt(12),
		args[8].AsString("Arial"), args[9].AsInt(15), args[10].AsInt(0xFFFFFF), args[11].AsInt(0x000000), 
		args[12].AsString("DUMMY"), args[13].AsInt(720), args[14].AsInt(576), "",
		env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
  env->AddFunction("CoronaSequence","[file]s[start]i[stop]i[sort]i[gapless]b[textmode]i[x]i[y]i[font]s[size]i[text_color]i[halo_color]i[expression]s",Create_CoronaSequence,0);
  env->AddFunction("RawSequence","[file]s[start]i[stop]i[sort]i[gapless]b[textmode]i[x]i[y]i[font]s[size]i[text_color]i[halo_color]i[pixel_type]s[width]i[height]i",Create_RawSequence,0);
  return 0;
}
