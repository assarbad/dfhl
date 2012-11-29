/* DFHL.cpp : Defines the entry point for the console application.

DFHL - Duplicate File Hard Linker, a small tool to gather some space
    from duplicate files on your hard disk
Copyright (C) 2004, 2005 Jens Scheffler & Oliver Schneider

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <stdarg.h>

// Global Definitions
// *******************************************
#define LOG_ERROR			1
#define LOG_INFO			0
#define LOG_VERBOSE			-1
#define LOG_DEBUG			-2
#define MAX_PATH_LENGTH		32768
#define TEMP_BUFFER_LENGTH	65536
#define FIRST_BLOCK_SIZE	65536 // Smallerblock size
#define BLOCK_SIZE			4194304 // 4MB seems to be a good value for performance without too much memory load
#define MIN_FILE_SIZE		1024 // Minimum file size so that hard linking will be checked...

#define PROGRAM_NAME		L"Duplicate File Hard Linker"
#define PROGRAM_VERSION     L"Version 1.2a"
#define PROGRAM_AUTHOR      L"Jens Scheffler and Oliver Schneider, http://www.jensscheffler.de"

enum CompareResult {
	EQUAL,			// File compare was successful and content is matching
	SAME,			// Files are already hard linked
	SKIP,			// Files should not be processed (filter!)
	DIFFERENT		// Files differ
};

namespace
{
	// Global Variables
	// *******************************************
	/** Logging Level for the console output */
	int logLevel = LOG_INFO;
	/** Flag if list should be displayed */
	bool outputList = false;
	/** Flag if running in real or test mode */
	bool reallyLink = false;

	// Global Code
	// *******************************************
	/**
	* Method to log a message to stdout/stderr
	*/
	void logError(LPCWSTR message, ...) {
		va_list argp;
		fwprintf(stderr, L"ERROR: ");
		va_start(argp, message);
		vfwprintf(stderr, message, argp);
		va_end(argp);
		fwprintf(stderr, L"\n");
	}

	void logError(DWORD errNumber, LPCWSTR message, ...) {
		va_list argp;
		fwprintf(stderr, L"ERROR: ");
		va_start(argp, message);
		vfwprintf(stderr, message, argp);
		va_end(argp);
		LPWSTR msgBuffer = NULL;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			errNumber,
			GetSystemDefaultLangID(),
			(LPWSTR)(&msgBuffer),
			0,
			NULL);
		fwprintf(stderr, L" -> [%i] %s\n", errNumber, msgBuffer);
		LocalFree(msgBuffer);
	}

	void logInfo(LPCWSTR message, ...) {
		if (logLevel <= LOG_INFO) {
			va_list argp;
			va_start(argp, message);
			vwprintf(message, argp);
			va_end(argp);
			wprintf(L"\n");
		}
	}

	void logVerbose(LPCWSTR message, ...) {
		if (logLevel <= LOG_VERBOSE) {
			va_list argp;
			wprintf(L"  ");
			va_start(argp, message);
			vwprintf(message, argp);
			va_end(argp);
			wprintf(L"\n");
		}
	}
	void logDebug(LPCWSTR message, ...) {
		if (logLevel <= LOG_DEBUG) {
			va_list argp;
			wprintf(L"    ");
			va_start(argp, message);
			vwprintf(message, argp);
			va_end(argp);
			wprintf(L"\n");
		}
	}

	// We ignore the third parameter
	inline BOOL MyCreateHardLink(LPCTSTR lpFileName, LPCTSTR lpExistingFileName, LPSECURITY_ATTRIBUTES)
	{
		return MoveFileEx(lpExistingFileName, lpFileName, MOVEFILE_CREATE_HARDLINK);
	}
}

// Global Classes
// *******************************************

class Collection {
private:
	class Item {
	public:
		void* data;

		Item* next;

		Item(void* defaultData) {
			data = defaultData;
			next = NULL;
		}

		~Item() {
		}
	};

	int itemCount;
	Item* root;
	Item* last;
	Item* nextItem;
public:
	void append(void* data) {
		Item* newItem = new Item(data);
		if (root != NULL) {
			last->next = newItem;
			last = newItem;
		} else {
			root = last = newItem;
		}
		itemCount++;
		nextItem = root;
	}

	void push(void* data) {
		append(data);
	}

	void* pop() {
		if (root != NULL) {
			Item* temp = root;
			nextItem = root = root->next;
			if (last == temp) {
				last = root;
			}
			itemCount--;
			void* result = temp->data;
			delete temp;
			return result;
		} else {
			return NULL;
		}
	}

	void* next() {
		if (nextItem != NULL) {
			Item* temp = nextItem;
			nextItem = nextItem->next;
			return temp->data;
		} else {
			return NULL;
		}
	}

	void* item(int index) {
		if (itemCount -1 >= index) {
			Item* temp = root;
			for (int i=0; i<index; i++) {
				temp = temp->next;
			}
			nextItem = temp->next;
			return temp->data;
		} else {
			return NULL;
		}
	}

	int getSize() {
		return itemCount;
	}

	Collection() {
		itemCount = 0;
		root = last= nextItem = NULL;
	}

	~Collection() {
		while (itemCount > 0)
			pop();
	}
};

class Paths {
private:
	class PathItem {
	public:
		LPWSTR path;

		PathItem(LPCWSTR newPath) {
			path = new wchar_t[wcslen(newPath)+1];
			wcscpy(path, newPath);
		}

		~PathItem() {
			delete path;
		}
	};
	Collection* col;
public:
	void add(LPCWSTR item) {
		PathItem* p = new PathItem(item);
		col->push(p);
	}

	bool pop(LPWSTR item) {
		if (col->getSize() > 0) {
			PathItem* p = (PathItem*)col->pop();
			wcscpy(item, p->path);
			delete p;
			return true;
		} else {
			return false;
		}
	}

	Paths() {
		col = new Collection();
	}

	~Paths() {
		delete col;
	}
};

class Files {
private:
	class FileItem {
	public:
		LPWSTR name;
		INT64 size;

		FileItem(LPCWSTR newName, INT64 newSize) {
			name = new wchar_t[wcslen(newName)+1];
			wcscpy(name, newName);
			size = newSize;
		}

		~FileItem() {
			delete name;
		}
	};
	Collection* col;
public:
	void add(LPCWSTR item, INT64 size) {
		FileItem* f = new FileItem(item, size);
		col->push(f);
	}

	bool pop(LPWSTR item, INT64& size) {
		if (col->getSize() > 0) {
			FileItem* f = (FileItem*)col->pop();
			wcscpy(item, f->name);
			size = f->size;
			delete f;
			return true;
		} else {
			return false;
		}
	}

	void next(LPWSTR item, INT64& size) {
		FileItem* f = (FileItem*)col->next();
		wcscpy(item, f->name);
		size = f->size;
	}

	void item(int index, LPWSTR item, INT64& size) {
		FileItem* f = (FileItem*)col->item(index);
		wcscpy(item, f->name);
		size = f->size;
	}

	int getSize() {
		return col->getSize();
	}

	Files() {
		col = new Collection();
	}

	~Files() {
		delete col;
	}
};


class Duplicates {
private:
	class DuplicateItem {
	public:
		LPWSTR name1;
		LPWSTR name2;
		INT64 size;

		DuplicateItem(LPCWSTR newName1, LPCWSTR newName2, INT64 newSize) {
			name1 = new wchar_t[wcslen(newName1)+1];
			wcscpy(name1, newName1);
			name2 = new wchar_t[wcslen(newName2)+1];
			wcscpy(name2, newName2);
			size = newSize;
		}

		~DuplicateItem() {
			delete name1;
			delete name2;
		}
	};
	Collection* col;
	INT64 byteSum;
	int fileCount;
public:
	void add(LPCWSTR item1, LPCWSTR item2, INT64 size) {
		DuplicateItem* d = new DuplicateItem(item1, item2, size);
		col->push(d);
		fileCount++;
		byteSum += size;
	}

	bool pop(LPWSTR item1, LPWSTR item2, INT64& size) {
		if (col->getSize() > 0) {
			DuplicateItem* d = (DuplicateItem*)col->pop();
			wcscpy(item1, d->name1);
			wcscpy(item2, d->name2);
			size = d->size;
			delete d;
			return true;
		} else {
			return false;
		}
	}

	bool next(LPWSTR item1, LPWSTR item2, INT64& size) {
		DuplicateItem* d = (DuplicateItem*)col->next();
		if (d != NULL) {
			wcscpy(item1, d->name1);
			wcscpy(item2, d->name2);
			size = d->size;
			return true;
		} else {
			return false;
		}
	}

	int getSize() {
		return col->getSize();
	}

	int getFileCount() {
		return fileCount;
	}

	INT64 getByteSum() {
		return byteSum;
	}

	Duplicates() {
		col = new Collection();
		byteSum = 0;
		fileCount = 0;
	}

	~Duplicates() {
		delete col;
	}
};

/**
* Duplicate File Linker Class
*/
class DuplicateFileHardLinker {

private:
	/** Collection of paths to process */
	Paths* p;
	/** Collection of files to check */
	Files* f;
	/** List of duplicates to process */
	Duplicates* d;
	/** buffer variables for file compare */
	LPBYTE block1;
	LPBYTE block2;
	/** Flag if attributes of file need to match */
	bool attributeMustMatch;
	/** Flag if hidden files should be processed */
	bool hiddenFiles;
	/** Flag if junctions should be followed */
	bool followJunctions;
	/** Flag if small files (<1024 bytes) should be processed */
	bool smallFiles;
	/** Flag if recursive processing should be enabled */
	bool recursive;
	/** Flag if system files should be processed */
	bool systemFiles;
	/** Flag if timestamps of file need to match */
	bool dateTimeMustMatch;

	/**
	* Logs a found file to debug
	*/
	void logFile(WIN32_FIND_DATA FileData) {
		INT64 size = FileData.nFileSizeLow + ((INT64)MAXDWORD + 1) * FileData.nFileSizeHigh;
		logDebug(L"Found file \"%s\" (Size=%I64i,%s%s%s%s%s%s%s%s%s%s%s%s)", 
			FileData.cFileName,
			size,
			FILE_ATTRIBUTE_ARCHIVE      &FileData.dwFileAttributes?L"ARCHIVE ":L"",
			FILE_ATTRIBUTE_COMPRESSED   &FileData.dwFileAttributes?L"COMPRESSED ":L"",
			FILE_ATTRIBUTE_DIRECTORY    &FileData.dwFileAttributes?L"DIRECTORY ":L"",
			FILE_ATTRIBUTE_ENCRYPTED    &FileData.dwFileAttributes?L"ENCRYPTED ":L"",
			FILE_ATTRIBUTE_HIDDEN       &FileData.dwFileAttributes?L"HIDDEN ":L"",
			FILE_ATTRIBUTE_NORMAL       &FileData.dwFileAttributes?L"NORMAL ":L"",
			FILE_ATTRIBUTE_OFFLINE      &FileData.dwFileAttributes?L"OFFLINE ":L"",
			FILE_ATTRIBUTE_READONLY     &FileData.dwFileAttributes?L"READONLY ":L"",
			FILE_ATTRIBUTE_REPARSE_POINT&FileData.dwFileAttributes?L"REPARSE_POINT ":L"",
			FILE_ATTRIBUTE_SPARSE_FILE  &FileData.dwFileAttributes?L"SPARSE ":L"",
			FILE_ATTRIBUTE_SYSTEM       &FileData.dwFileAttributes?L"SYSTEM ":L"",
			FILE_ATTRIBUTE_TEMPORARY    &FileData.dwFileAttributes?L"TEMP ":L"");
	}

	/**
	* Compares the given 2 files content
	*/
	CompareResult compareFiles(LPCWSTR file1, LPCWSTR file2, INT64 size) {
		// doublecheck data consistency!
		if (wcscmp(file1, file2) == 0) {
			logError(L"Same file \"%s\"found as duplicate, ignoring!", file1);
			return DIFFERENT;
		}

		// Open File 1
		HANDLE hFile1 = CreateFile(file1, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (hFile1 == INVALID_HANDLE_VALUE) {
			logError(L"Unable to open file \"%s\"", file1);
			return DIFFERENT;
		}

		// Open File 2
		HANDLE hFile2 = CreateFile(file2, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (hFile2 == INVALID_HANDLE_VALUE) {
			logError(L"Unable to open file \"%s\"", file2);
			CloseHandle(hFile1);
			return DIFFERENT;
		}

		// Check file system information details...
		BY_HANDLE_FILE_INFORMATION info1;
		BY_HANDLE_FILE_INFORMATION info2;
		if (GetFileInformationByHandle(hFile1, &info1) && GetFileInformationByHandle(hFile2, &info2)) {
			// First check if the files are already hard-linked...
			if (info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber &&
				info1.nFileIndexHigh == info2.nFileIndexHigh &&
				info1.nFileIndexLow == info2.nFileIndexLow) {

					logVerbose(L"Files are already hard linked, skipping.");
					CloseHandle(hFile1);
					CloseHandle(hFile2);
					return SAME;
			}

			// check for attributes matching
			if (attributeMustMatch && info1.dwFileAttributes != info2.dwFileAttributes) {
				logVerbose(L"Attributes of files do not match, skipping.");
				CloseHandle(hFile1);
				CloseHandle(hFile2);
				return SKIP;
			}

			// check for time stamp matching
			if (dateTimeMustMatch && (
				info1.ftLastWriteTime.dwHighDateTime != info2.ftLastWriteTime.dwHighDateTime ||
				info1.ftLastWriteTime.dwLowDateTime != info2.ftLastWriteTime.dwLowDateTime
				)) {
					logVerbose(L"Modification timestamps of files do not match, skipping.");
					CloseHandle(hFile1);
					CloseHandle(hFile2);
					return SKIP;
			}
		} else {
			logInfo(L"Unable to read further file information, skipping.");
			CloseHandle(hFile1);
			CloseHandle(hFile2);
			return SKIP;
		}

		// Read File Content and compare
		if (block1 == NULL) {
			block1 = new BYTE[BLOCK_SIZE];
		}
		if (block2 == NULL) {
			block2 = new BYTE[BLOCK_SIZE];
		}
		DWORD blockSize = FIRST_BLOCK_SIZE; // Note: For the first block read smaller amount to speed up...
		DWORD read1;
		DWORD read2;
		DWORD result1;
		DWORD result2;
		INT64 bytesToRead = size;
		bool switcher = true; // helper variable for performance optimization
		while (bytesToRead > 0) {

			// Read Blocks - Performance boosted: read alternating to mimimize head shifts... ;-)
			if (switcher) {
				result1 = ReadFile(hFile1, block1, blockSize, &read1, NULL);
				result2 = ReadFile(hFile2, block2, blockSize, &read2, NULL);
			} else {
				result2 = ReadFile(hFile2, block2, blockSize, &read2, NULL);
				result1 = ReadFile(hFile1, block1, blockSize, &read1, NULL);
			}

			// change the state for the next read operation
			switcher = !switcher; // alternate read
			blockSize = BLOCK_SIZE; // use bigger block size

			// Compare Data
			bytesToRead -= read1;
			if (read1 != read2 || read1 == 0) {
				logError(L"File length differ or read error! This _should_ not happen!?!?");
				CloseHandle(hFile2);
				CloseHandle(hFile1);
				return DIFFERENT;
			}

			for (DWORD i = 0; i < read1; i++) {
				if (block1[i] != block2[i]) {
					logVerbose(L"Files differ in content.");
					CloseHandle(hFile2);
					CloseHandle(hFile1);
					return DIFFERENT;
				}
			}
		}

		// Close File 2
		CloseHandle(hFile2);

		// Close File 1
		CloseHandle(hFile1);

		logVerbose(L"Files are equal, hard link possible.");
		return EQUAL;
	}

	/**
	* Adds a file to the collection of files to process
	* @param file FindFile Structure of further file information
	*/
	void addFile(LPCWSTR file, WIN32_FIND_DATA details) {
		f->add(file, details.nFileSizeLow + ((INT64)MAXDWORD + 1) * details.nFileSizeHigh);
	}

	/**
	* Compares the given 2 files content
	*/
	void addDuplicate(LPCWSTR file1, LPCWSTR file2, INT64 size) {
		d->add(file1, file2, size);
	}

	/**
	* Adds a found entry in the file system into the collection iof items to be processed
	* This function also applies all selected filters of the user
	* @param item FindFile Structure of further file information
	*/
	void addItem(LPCWSTR base, WIN32_FIND_DATA item) {
		// check if this is a valid file and not a dummy like "." or ".."
		if (wcscmp(item.cFileName, L".") == 0 || wcscmp(item.cFileName, L"..") == 0) {
			// just ignore these entries
			return;
		}

		logFile(item);

		// check if it's a directory entry...
		LPWSTR fullPath = new wchar_t[MAX_PATH_LENGTH];
		wsprintf(fullPath, L"%s\\%s", base, item.cFileName);
		if (item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) {

			// check for recursice setting
			if (!recursive) {
				logDebug(L"skipping folder, not running recursive");
				delete fullPath;
				return;
			}

			// check for junction
			if (!followJunctions && item.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) {
				logDebug(L"ignoring junction");
				delete fullPath;
				return;
			}

			// add the path to the collection
			addPath(fullPath);

		} else {

			// check if this is a hidden file
			if (!hiddenFiles && item.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) {
				logDebug(L"ignoring file, hidden attribute is set");
				delete fullPath;
				return;
			}

			// check if the file is "big" enough
			if (!smallFiles && (item.nFileSizeLow > 0) && (item.nFileSizeLow < MIN_FILE_SIZE) && (item.nFileSizeHigh == 0)) {
				logDebug(L"ignoring file, is too small.");
				delete fullPath;
				return;
			}

			// check if this is a system file
			if (!systemFiles && (item.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
				logDebug(L"ignoring file, system attribute is set");
				delete fullPath;
				return;
			}

			// add the file only if it contains data!
			if ((item.nFileSizeLow > 0) || (item.nFileSizeHigh > 0)) {
				addFile(fullPath, item);
			}
		}
		delete fullPath;
	}

	/**
	* Links two files on hard disk by deleting one and linking the name of this to the other
	* @param file1 File name of the first file
	* @param file2 File name of the second file to link to
	* @return boolean value if operation was successful
	*/
	bool hardLinkFiles(LPCWSTR file1, LPCWSTR file2) {
		LPWSTR file2Backup = new wchar_t[MAX_PATH_LENGTH];

		logInfo(L"Linking %s and %s", file1, file2);

		// Step 1: precaution - rename original file
		wsprintf(file2Backup, L"%s_backup", file2);
		if (!MoveFile(file2, file2Backup)) {
			logError(L"Unable to move file to backup: %i", GetLastError());
			delete file2Backup;
			return false;
		}

		// Step 2: create hard link
		if (!MyCreateHardLink(file2, file1, NULL)) {
			logError(L"Unable to create hard link: %i", GetLastError());
			delete file2Backup;
			return false;
		}

		// Step 3: remove backup file (orphan)
		if (!DeleteFile(file2Backup)) {
			logError(L"Unable to delete file: %i, trying to change attribute...", GetLastError());

			if (!SetFileAttributes(file2Backup, FILE_ATTRIBUTE_NORMAL) ||
				!DeleteFile(file2Backup)) {

					logError(L"Finally unable to delete file: %i", GetLastError());
					delete file2Backup;
					return false;
			}
		}

		return true;
	}

public:

	/**
	* Constructor for the Object
	*/
	DuplicateFileHardLinker() {
		p = new Paths();
		f = new Files();
		d = new Duplicates();
		block1 = block2 = NULL;
		attributeMustMatch = false;
		hiddenFiles = false;
		followJunctions = false;
		smallFiles = false;
		recursive = false;
		systemFiles = false;
		dateTimeMustMatch = false;
	}

	~DuplicateFileHardLinker() {
		delete p;
		delete f;
		delete d;
		if (block1 != NULL) {
			delete block1;
		}
		if (block2 != NULL) {
			delete block2;
		}
	}

	/**
	* Setter for the attriute match flag
	*/
	void setAttributeMustMatch(bool newValue) {
		attributeMustMatch = newValue;
	}

	/**
	* Setter for the hidden files flag
	*/
	void setHiddenFiles(bool newValue) {
		hiddenFiles = newValue;
	}

	/**
	* Setter for the junctions follow flag
	*/
	void setFollowJunctions(bool newValue) {
		followJunctions = newValue;
	}

	/**
	* Setter for the small files flag
	*/
	void setSmallFiles(bool newValue) {
		smallFiles = newValue;
	}

	/**
	* Setter for the small files flag
	*/
	void setRecursive(bool newValue) {
		recursive = newValue;
	}

	/**
	* Setter for the system files flag
	*/
	void setSystemFiles(bool newValue) {
		systemFiles = newValue;
	}

	/**
	* Setter for the attriute match flag
	*/
	void setDateMatch(bool newValue) {
		dateTimeMustMatch = newValue;
	}

	/**
	* Adds a path to the collection of path's to process
	* @param path Path to add to the collection
	*/
	void addPath(LPCWSTR path) {
		p->add(path);
	}

	/**
	* Starts the search for duplicate files
	*/
	void findDuplicates() {
		// Step 1: Walk through the directory tree
		logInfo(L"Parsing Directory Tree...");
		LPWSTR folder = new wchar_t[MAX_PATH_LENGTH];
		while (p->pop(folder)) {
			logVerbose(L"Parsing Folder %s", folder);

			WIN32_FIND_DATA FindFileData;
			HANDLE hFind = INVALID_HANDLE_VALUE;
			wchar_t DirSpec[MAX_PATH_LENGTH];  // directory specification
			DWORD dwError;

			size_t len = wcslen(wcsncpy(DirSpec, folder, wcslen(folder)+1));
			// Do not append backslash if this is already the last character!
			if(DirSpec[len] != L'\\')
				wcsncat(DirSpec, L"\\", 2);
			wcsncat(DirSpec, L"*", 2);

			hFind = FindFirstFile(DirSpec, &FindFileData);

			if (hFind == INVALID_HANDLE_VALUE) {
				// Accessing "<drive>:\System Volume Information\*" gives an
				// ERROR_ACCESS_DENIED. So this has to be fixed to scan whole
				// volumes! Also can happen on folders with no access permissions.
				logError(GetLastError(), L"Unable to read folder content.");
			} else {
				addItem(folder, FindFileData);
				while (FindNextFile(hFind, &FindFileData) != 0) {
					addItem(folder, FindFileData);
				}

				dwError = GetLastError();
				FindClose(hFind);
				if (dwError != ERROR_NO_MORE_FILES) {
					LPWSTR buffer = new wchar_t[TEMP_BUFFER_LENGTH];
					wsprintf(buffer, L"FindNextFile error. Error is %u\n", dwError);
					throw buffer;
				}
			}
		}

		// Step 2: Walk over all relevant files
		logInfo(L"Found %i Files in folders, comparing relevant files.", f->getSize());
		LPWSTR file1 = new wchar_t[MAX_PATH_LENGTH];
		INT64 size1;
		LPWSTR file2 = new wchar_t[MAX_PATH_LENGTH];
		INT64 size2;
		bool duplicateChecked;
		while (f->pop(file1, size1)) {
			duplicateChecked = false;
			for (int i = 0; i < f->getSize() && !duplicateChecked; i++) {
				f->next(file2, size2);
				if (size1 == size2) {
					logVerbose(L"File \"%s\" and \"%s\" have both size of %I64i comparing...", file1, file2, size1);

					// Compare the both files with same size
					DWORD start = GetTickCount();
					DWORD time = 0;
					switch (compareFiles(file1, file2, size1))
					{
					case EQUAL:
						time = GetTickCount() - start;
						logDebug(L"file compare took %ims, %I64i KB/s", time, time>0?size1*2*1000 / time / 1024:0);

						// Files seem to be equal, marking them for later processing...
						addDuplicate(file1, file2, size1);
						duplicateChecked = true;
						break;
					case SAME:
						// In case the files are already hard linked, we skip further checks and check the second file later...
						duplicateChecked = true;
						break;
					case SKIP:
						// okay, it seems that this pair should not be processed...
						duplicateChecked = true;
						break;
					case DIFFERENT:
						// yeah, just do nothing.
						break;
					}
				}
			}
		}

		// Step 3: Show search results
		logInfo(L"Found %i duplicate files, savings of %I64i bytes possible.", d->getFileCount(), d->getByteSum());

		delete file2;
		delete file1;
		delete folder;
	}

	/**
	* Processes all duplicates and crestes hard links of the files
	*/
	void linkAllDuplicates() {
		LPWSTR file1 = new wchar_t[MAX_PATH_LENGTH];
		LPWSTR file2 = new wchar_t[MAX_PATH_LENGTH];
		INT64 size;
		INT64 sumSize = 0;

		if (d->getSize() > 0) {
			// Loop over all found files...
			logInfo(L"Hard linking %i duplicate files", d->getSize());
			while (d->pop(file1, file2, size)) {
				sumSize += size;
				if (!hardLinkFiles(file1, file2)) {
					logInfo(L"Unable to process links for \"%s\" and \"%s\"", file1, file2);
				}
			}
			logInfo(L"Hard linking done, %I64i bytes saved.", sumSize);
		} else {
			logInfo(L"No files found for linking");
		}

		delete file2;
		delete file1;
	}

	/**
	* Displays the result duplicate list to stdout
	*/
	void listDuplicates() {
		LPWSTR file1 = new wchar_t[MAX_PATH_LENGTH];
		LPWSTR file2 = new wchar_t[MAX_PATH_LENGTH];
		INT64 size;

		if (d->next(file1, file2, size)) {
			logInfo(L"Result of duplicate analysis:");
			do {
				logInfo(L"%I64i bytes: %s = %s", size, file1, file2);
			} while (d->next(file1, file2, size));
		} else {
			logInfo(L"No duplicates to list.");
		}

		delete file2;
		delete file1;
	}
};

/**
* Helper function to parse the command line
* @param argc Argument Counter
* @param argv Argument Vector
* @param prog Program Instance Reference to fill with options
*/
bool parseCommandLine(int argc, char* argv[], DuplicateFileHardLinker* prog) {
	bool pathAdded = false;

	// iterate over all arguments...
	for (int i=1; i<argc; i++) {

		// first check if command line option
		if (argv[i][0] == '-' || argv[i][0] == '/') {

			if (strlen(argv[i]) == 2) {
				switch (argv[i][1]) {
				case '?':
					// Show program usage
					wchar_t programName[MAX_PATH_LENGTH];
					mbstowcs(programName,argv[0],sizeof(programName));
					logInfo(PROGRAM_NAME);
					logInfo(L"Program to link duplicate files in several paths on one disk.");
					logInfo(L"%s - %s", PROGRAM_VERSION, PROGRAM_AUTHOR);
					logInfo(L"");
					logInfo(L"NOTE: Use this tool on your own risk!");
					logInfo(L"");
					logInfo(L"Usage:");
					logInfo(L"%s [options] [path] [...]", programName);
					logInfo(L"Options:");
					logInfo(L"/?\tShows this help screen");
					logInfo(L"/a\tFile attributes must match for linking");
					logInfo(L"/d\tDebug Mode");
					logInfo(L"/h\tProcess hidden files");
					logInfo(L"/j\tAlso follow junctions (=reparse points) in filesystem");
					logInfo(L"/l\tHard links for files. If not specified, tool will just read (test) for duplicates");
					logInfo(L"/m\tAlso Process small files <1024 bytes, they are skipped by default");
					logInfo(L"/o\tList duplicate file result to stdout");
					logInfo(L"/q\tSilent Mode");
					logInfo(L"/r\tRuns recursively through the given folder list");
					logInfo(L"/s\tProcess system files");
					logInfo(L"/t\tTime + Date of files must match");
					logInfo(L"/v\tVerbose Mode");
					throw L""; //just to terminate the program...
					break;
				case 'a':
					prog->setAttributeMustMatch(true);
					break;
				case 'd':
					logLevel = LOG_DEBUG;
					break;
				case 'h':
					prog->setHiddenFiles(true);
					break;
				case 'j':
					prog->setFollowJunctions(true);
					break;
				case 'l':
					reallyLink = true;
					break;
				case 'm':
					prog->setSmallFiles(true);
					break;
				case 'o':
					outputList = true;
					break;
				case 'q':
					logLevel = LOG_ERROR;
					break;
				case 'r':
					prog->setRecursive(true);
					break;
				case 's':
					prog->setSystemFiles(true);
					break;
				case 't':
					prog->setDateMatch(true);
					break;
				case 'v':
					logLevel = LOG_VERBOSE;
					break;
				default:
					logError(L"Illegal Command line option! Use /? to see valid options!");
					return false;
				}
			} else {
				logError(L"Illegal Command line option! Use /? to see valid options!");
				return false;
			}
		} else {
			// the command line options seems to be a path...
			wchar_t tmpPath[MAX_PATH_LENGTH];
			mbstowcs(tmpPath,argv[i],sizeof(tmpPath));

			// check if the path is existing!
			wchar_t DirSpec[MAX_PATH_LENGTH];  // directory specification
			wcsncpy(DirSpec, tmpPath, wcslen(tmpPath)+1);
			wcsncat(DirSpec, L"\\*", 3);
			WIN32_FIND_DATA FindFileData;
			HANDLE hFind = FindFirstFile(DirSpec, &FindFileData);
			if (hFind == INVALID_HANDLE_VALUE) {
				logError(L"Specified directory \"%s\" does not exist", tmpPath);
				return false;
			}

			prog->addPath(tmpPath);
			pathAdded = true;
		}
	}

	// check for parameters
	if (!pathAdded) {
		logError(L"You need to specify at least one folder to process!\nUse /? to see valid options!");
		return false;
	}

	return true;
}

/**
* Main runnable and entry point for executing the application
* @param argc Argument Counter
* @param argv Argument Vector
* @return Application Return code
*/
int __cdecl main(int argc, char* argv[])
{
	int result = 0;

	DuplicateFileHardLinker* prog = new DuplicateFileHardLinker();

	try {
		// parse the command line
		if (!parseCommandLine(argc, argv, prog)) {
			return -1;
		}

		// show desired option info
		logInfo(PROGRAM_NAME);
		logInfo(L"%s - %s", PROGRAM_VERSION, PROGRAM_AUTHOR);
		logInfo(L"");

		// find duplicates
		prog->findDuplicates();

		if (outputList) {
			prog->listDuplicates();
		}

		if (reallyLink) {
			// link duplicates
			prog->linkAllDuplicates();
		} else {
			logInfo(L"Skipping real linking. To really create hard links, use the /l switch.");
		}
	} catch (LPCWSTR err) {
		DWORD dwError = GetLastError();
		if (wcslen(err) > 0) {
			if (dwError != 0) {
				logError(dwError, err);
			} else {
				logError(err);
			}
		}
		result = -1;
	}

	delete prog;
	return result;
}
