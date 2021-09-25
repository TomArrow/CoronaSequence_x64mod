//
// search for files, generate file-list from pattern
//

#include "filesearch.h"

FileSearch::FileSearch() {
	int i;

	maxfiles = 1024;
	maxdirs = 256;

	searchdirs = (p_char*) malloc(maxdirs * sizeof(p_char));
	for (i=0; i<maxdirs; i++) {
		searchdirs[i] = (p_char) malloc(255);
		searchdirs[i][0] = 0;
	}
	dircount = 0;

	foundfiles = (p_filedata*) malloc(maxfiles * sizeof(p_filedata));
	for (i=0; i<maxfiles; i++) {
		foundfiles[i] = (p_filedata) malloc(sizeof(filedata));
		foundfiles[i]->name[0] = 0;
		foundfiles[i]->path[0] = 0;
		foundfiles[i]->time = 0;
	}
	filecount = 0;
};

FileSearch::~FileSearch() {
	//free all resources
		int i;
		i=1;

		for (i=0; i<maxfiles; i++) {
			free(foundfiles[i]);
		}
		free(foundfiles);
		for (i=0; i<maxdirs; i++) {
			free(searchdirs[i]);
		}
	};


	void FileSearch::AddDir(char* path)
	//add a dir and expand buffer if necessary
	{
		int i;
		
		//don't add if already in list
		for (i=0; i<dircount; i++) {
			if (!strcmp(path, searchdirs[i])) {
				return;
			}
		}
		strcpy(searchdirs[dircount], path);
		dircount++;
		if (dircount>=maxdirs) {	//then double buffer size
			searchdirs = (p_char*) realloc(searchdirs, 2 * maxdirs * sizeof(p_char));
			for (i=maxdirs; i<2*maxdirs; i++) {
				searchdirs[i] = (p_char) malloc(255);
				searchdirs[i][0] = 0;
			}
			maxdirs = maxdirs * 2;
		}
	}
	
	void FileSearch::AddFile(filedata* file)
	//add a file and expand buffer if necessary
	{
		int i;
		strcpy(foundfiles[filecount]->path, file->path);
		strcpy(foundfiles[filecount]->name, file->name);
		foundfiles[filecount]->time = file->time;
		filecount++;
		if (filecount>=maxfiles) {
			foundfiles = (p_filedata*) realloc(foundfiles, 2 * maxfiles * sizeof(p_filedata));
			for (i=maxfiles; i<2*maxfiles; i++) {
				foundfiles[i] = (p_filedata) malloc(sizeof(filedata));
				foundfiles[i]->name[0] = 0;
				foundfiles[i]->path[0] = 0;
				foundfiles[i]->time = 0;
			}
			maxfiles = maxfiles * 2;
		}
	}

	void FileSearch::SearchSubDirs()
	// search for all subdirs in all dirs already in list
	{
		_finddata_t fileinfo;
		long hFile;
		int act_root = 0;

		while (act_root<dircount) {	//scan all dirs in the list
			strcpy(fullname, searchdirs[act_root]);
			strcat(fullname, "*");	//search for all items but use only dirs later
			hFile = _findfirst(fullname, &fileinfo);
			if (hFile != -1) {
				if ((fileinfo.attrib & _A_SUBDIR) && (strcmp(fileinfo.name,"..")) && (strcmp(fileinfo.name,"."))) {
					strcpy(dir, searchdirs[act_root]);
					strcat(dir, fileinfo.name);
					strcat(dir, "\\");
					AddDir(dir);
				}
				while ( _findnext(hFile, &fileinfo) == 0) {
					if ((fileinfo.attrib & _A_SUBDIR) && (strcmp(fileinfo.name,"..")) && (strcmp(fileinfo.name,"."))) {
						strcpy(dir, searchdirs[act_root]);
						strcat(dir, fileinfo.name);
						strcat(dir, "\\");
						AddDir(dir);
					}
				}
			}
			_findclose(hFile);
			act_root++;
		}
	};



	void FileSearch::DirSearchFiles(char* path, char* pattern)
	//searches one directory
	{
		_finddata_t fileinfo;
		long hFile;
		filedata file;

		// *.htm finds *.html too !!
		strcpy(fullname, path);
		strcat(fullname, pattern);
		hFile = _findfirst(fullname, &fileinfo);
		if (hFile == -1) {
			return;	//nothing found
		}
		_splitpath(fullname, drive, dir, fname, ext);

		if (!(fileinfo.attrib & _A_SUBDIR)) {
			strcpy(file.path, drive);
			strcat(file.path, dir);
			strcpy(file.name, fileinfo.name);
			file.time = fileinfo.time_write;
			AddFile(&file);
		}

		while (_findnext(hFile, &fileinfo) == 0) {
			if (!(fileinfo.attrib & _A_SUBDIR)) {
				strcpy(file.path, drive);
				strcat(file.path, dir);
				strcpy(file.name, fileinfo.name);
				file.time = fileinfo.time_write;
				AddFile(&file);
			}
		}
		_findclose(hFile);
	}

	void FileSearch::DirPrintFiles(char* path, char* pattern, int min, int max, bool gapless)
	// expands one directory
	{
		char resolvedname[256];
		int ret;
		int i;
		filedata file;
		struct _stat statusbuffer;

		i = min;
		while (i<=max) {
			sprintf(resolvedname, pattern, i);	//replace templates
			strcpy(fullname, path);
			strcat(fullname, resolvedname);
			ret = _stat(fullname, &statusbuffer);	//check if file is present
			if (ret==0) {
				file.time = statusbuffer.st_mtime;
				strcpy(file.path, path);
				strcpy(file.name, resolvedname);
				AddFile(&file);
			} else {
				if (!gapless) {	//don't add files that are not valid / do not exist
					file.time = 0;
					strcpy(file.path, path);
					strcpy(file.name, resolvedname);
					AddFile(&file);
				}
			}
			i++;
		}
	}


	void FileSearch::BubbleSort(int sortmode)
	{
		// 1=path & name, root files first (path is "" here)
		// 2=name
		// 3=filetime
		int i, j;
		p_filedata tmp;
		bool swap;
		bool any_swap;

		if (filecount<=1) return;	//at least two entries necessary

		for (i=filecount-2; i>=0; i--) {
			any_swap = false;
				for (j=0; j<=i; j++) {
				swap = false;
				switch (sortmode) {	//default: do nothing
				case 1:
							if (_stricmp(foundfiles[j]->path, foundfiles[j+1]->path)>0) swap = true;
							if (_stricmp(foundfiles[j]->path, foundfiles[j+1]->path)==0) {
								if (_stricmp(foundfiles[j]->name, foundfiles[j+1]->name)>0) swap = true;
					}
					break;
				case 2:
							if (_stricmp(foundfiles[j]->name, foundfiles[j+1]->name)>0) swap = true;
					break;
				case 3:
							if (foundfiles[j]->time > foundfiles[j+1]->time) swap = true;
					break;
				}
				if (swap) {
						any_swap = true;
						tmp = foundfiles[j];
						foundfiles[j] = foundfiles[j+1];
						foundfiles[j+1] = tmp;
				}
			}
			if (!any_swap) break;
		}
		return; 
	}

	void FileSearch::PrintFiles(char* pattern, int min, int max, bool gapless)
	//expands ALL directories
	{
		int i;
		for (i=0; i<dircount; i++) {
			DirPrintFiles(searchdirs[i], pattern, min, max, gapless);
		}
	}

	void FileSearch::SearchFiles(char* pattern)
	//search ALL directories 
	{
		int i;
		for (i=0; i<dircount; i++) {
			DirSearchFiles(searchdirs[i], pattern);
		}
	}

	int FileSearch::GetFilecount()
	{
		return filecount;
	}

	char* FileSearch::GetFilename(int i)
	//returns path and name
	{
		if (i>=filecount) return NULL;
		strcpy(fullname, foundfiles[i]->path);
		strcat(fullname, foundfiles[i]->name);
		return fullname;
	}

	char* FileSearch::GetFiledate(int i)
	//return NULL if file was not found
	{
		if (i>=filecount) return NULL;
		if (foundfiles[i]->time == 0) return NULL;
		tm *time = localtime(&(foundfiles[i]->time));
		strftime(timebuf, 25, "%Y-%m-%d %H:%M:%S", time);
		return timebuf;
	}
;
