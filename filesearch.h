//
// search for files, generate file-list from pattern
//

#include <malloc.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

class FileSearch {

	struct filedata {
		char name[256];
		char path[256];
		time_t time;
	};

	typedef filedata* p_filedata;
	typedef char* p_char;

private:
	p_char * searchdirs;
	p_filedata * foundfiles;

	int filecount;	//and the next free position
	int dircount;	//and the next free position
	int maxdirs;
	int maxfiles;

	char timebuf[25];
	char fullname[256], drive[3], dir[256], fname[256], ext[10];

private:
	void DirSearchFiles(char* path, char* pattern);
	void DirPrintFiles(char* path, char* pattern, int min, int max, bool gapless);

public:
	FileSearch();
	~FileSearch();
	void AddDir(char* path);
	void AddFile(filedata* file);
	void SearchSubDirs();
	void BubbleSort(int sortmode);
	void PrintFiles(char* pattern, int min, int max, bool gapless);
	void SearchFiles(char* pattern);
	int GetFilecount();
	char* GetFilename(int i);
	char* GetFiledate(int i);
};
