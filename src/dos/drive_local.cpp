/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// Uncomment to enable file-open diagnostic messages
// #define DEBUG 1

#include "drives.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef WIN32
#include <utime.h>
#include <sys/file.h>
#else
#include <fcntl.h>
#include <sys/utime.h>
#include <sys/locking.h>
#endif
#include <sys/stat.h>

#ifdef _MSC_VER
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include "dos_inc.h"
#include "dos_mscdex.h"
#include "fs_utils.h"
#include "string_utils.h"
#include "cross.h"
#include "inout.h"

#if defined(WIN32)
constexpr int file_access_tries = 3;
#endif

bool localDrive::FileCreate(DOS_File * * file,char * name,[[maybe_unused]]Bit16u attributes) {
//TODO Maybe care for attributes but not likely
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	char* temp_name = dirCache.GetExpandName(newname); //Can only be used in till a new drive_cache action is preformed */
	/* Test if file exists (so we need to truncate it). don't add to dirCache then */
	bool existing_file = false;
	
	FILE * test = fopen_wrap(temp_name,"rb+");
	if (test) {
		fclose(test);
		existing_file=true;

	}
	
	FILE * hand = nullptr;
    if (enable_share && !existing_file) {
#if defined(WIN32)
		int attribs = FILE_ATTRIBUTE_NORMAL;
		if (attributes&3) attribs = attributes&3;
		HANDLE handle = CreateFile(temp_name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, attribs, NULL);
		if (handle == INVALID_HANDLE_VALUE) return false;
		int nHandle = _open_osfhandle((intptr_t)handle, _O_RDONLY);
		if (nHandle == -1) {CloseHandle(handle);return false;}
		hand = fdopen(nHandle, "wb+");
#else
		int fd = open(temp_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (fd<0) {close(fd);return false;}
		hand = fdopen(fd, "wb+");
#endif
	} else {
		hand=fopen(temp_name,"wb+");
	}
	if (!hand) {
		LOG_MSG("Warning: file creation failed: %s",newname);
		return false;
	}
   
	if (!existing_file) dirCache.AddEntry(newname, true);
	/* Make the 16 bit device information */
	*file = new localFile(name, hand, basedir);
	(*file)->flags=OPEN_READWRITE;

	return true;
}

bool localDrive::IsFirstEncounter(const std::string& filename) {
	const auto ret = write_protected_files.insert(filename);
	const bool was_inserted = ret.second;
	return was_inserted;
}

// Search the Files[] inventory for an open file matching the requested
// local drive and file name. Returns nullptr if not found.
DOS_File *FindOpenFile(const DOS_Drive *drive, const char *name)
{
	if (!drive || !name)
		return nullptr;

	uint8_t drive_num = DOS_DRIVES; // default out range
	// Look for a matching drive mount
	for (uint8_t i = 0; i < DOS_DRIVES; ++i) {
		if (Drives[i] && Drives[i] == drive) {
			drive_num = i;
			break;
		}
	}
	if (drive_num == DOS_DRIVES) // still out of range, no matching mount
		return nullptr;

	// Look for a matching open filename on the same mount
	for (auto *file : Files)
		if (file && file->IsOpen() && file->GetDrive() == drive_num && file->IsName(name))
			return file; // drive + file path is unique

	return nullptr;
}

#ifndef WIN32
int lock_file_region(int fd, int cmd, struct flock *fl, long long start, unsigned long len)
{
  fl->l_whence = SEEK_SET;
  fl->l_pid = 0;
  //if (start == 0x100000000LL) start = len = 0; //first handle magic file lock value
#ifdef F_SETLK64
  if (cmd == F_SETLK64 || cmd == F_GETLK64) {
    struct flock64 fl64;
    int result;
    DEBUG_LOG_MSG("Large file locking start=%llx, len=%lx\n", start, len);
    fl64.l_type = fl->l_type;
    fl64.l_whence = fl->l_whence;
    fl64.l_pid = fl->l_pid;
    fl64.l_start = start;
    fl64.l_len = len;
    result = fcntl( fd, cmd, &fl64 );
    fl->l_type = fl64.l_type;
    fl->l_start = (long) fl64.l_start;
    fl->l_len = (long) fl64.l_len;
    return result;
  }
#endif
  if (start == 0x100000000LL)
    start = 0x7fffffff;
  fl->l_start = start;
  fl->l_len = len;
  return fcntl( fd, cmd, fl );
}

#define COMPAT_MODE	0x00
#define DENY_ALL	0x01
#define DENY_WRITE	0x02
#define DENY_READ	0x03
#define DENY_NONE	0x04
#define FCB_MODE	0x07
bool share(int fd, int mode, uint32_t flags) {
  struct flock fl;
  int ret;
  int share_mode = ( flags >> 4 ) & 0x7;
  fl.l_type = F_WRLCK;
  /* see whatever locks are possible */

#ifdef F_GETLK64
  ret = lock_file_region( fd, F_GETLK64, &fl, 0x100000000LL, 1 );
  if ( ret == -1 && errno == EINVAL )
#endif
  ret = lock_file_region( fd, F_GETLK, &fl, 0x100000000LL, 1 );
  if ( ret == -1 ) return true;

  /* file is already locked? then do not even open */
  /* a Unix read lock prevents writing;
     a Unix write lock prevents reading and writing,
     but for DOS compatibility we allow reading for write locks */
  if ((fl.l_type == F_RDLCK && mode != O_RDONLY) || (fl.l_type == F_WRLCK && mode != O_WRONLY))
    return false;

  switch ( share_mode ) {
  case COMPAT_MODE:
    if (fl.l_type == F_WRLCK) return false;
   [[fallthrough]];
  case DENY_NONE:
    return true;                   /* do not set locks at all */
  case DENY_WRITE:
    if (fl.l_type == F_WRLCK) return false;
    if (mode == O_WRONLY) return true; /* only apply read locks */
    fl.l_type = F_RDLCK;
    break;
  case DENY_READ:
    if (fl.l_type == F_RDLCK) return false;
    if (mode == O_RDONLY) return true; /* only apply write locks */
    fl.l_type = F_WRLCK;
    break;
  case DENY_ALL:
    if (fl.l_type == F_WRLCK || fl.l_type == F_RDLCK) return false;
    fl.l_type = mode == O_RDONLY ? F_RDLCK : F_WRLCK;
    break;
  case FCB_MODE:
    if ((flags & 0x8000) && (fl.l_type != F_WRLCK)) return true;
    [[fallthrough]];
  default:
    LOG(LOG_DOSMISC,LOG_WARN)("internal SHARE: unknown sharing mode %x\n", share_mode);
    return false;
    break;
  }
#ifdef F_SETLK64
  ret = lock_file_region(fd, F_SETLK64, &fl, 0x100000000LL, 1);
  if(ret == -1 && errno == EINVAL)
#endif
      lock_file_region(fd, F_SETLK, &fl, 0x100000000LL, 1);
  DEBUG_LOG_MSG("internal SHARE: locking: fd %d, type %d whence %d pid %d\n", fd, fl.l_type, fl.l_whence, fl.l_pid);

  return true;
}
#endif

bool localDrive::FileOpen(DOS_File **file, char *name, Bit32u flags)
{
	const char *type = nullptr;
	switch (flags&0xf) {
	case OPEN_READ:        type = "rb" ; break;
	case OPEN_WRITE:       type = "rb+"; break;
	case OPEN_READWRITE:   type = "rb+"; break;
	case OPEN_READ_NO_MOD: type = "rb" ; break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	// If the file's already open then flush it before continuing
	// (Betrayal in Antara)
	auto open_file = dynamic_cast<localFile *>(FindOpenFile(this, name));
	if (open_file)
		open_file->Flush();

	FILE* fhandle;
	if (enable_share) {
#if defined(WIN32)
		int ohFlag = (flags&0xf)==OPEN_READ||(flags&0xf)==OPEN_READ_NO_MOD?GENERIC_READ:((flags&0xf)==OPEN_WRITE?GENERIC_WRITE:GENERIC_READ|GENERIC_WRITE);
		int shhFlag = (flags&0x70)==0x10?0:((flags&0x70)==0x20?FILE_SHARE_READ:((flags&0x70)==0x30?FILE_SHARE_WRITE:FILE_SHARE_READ|FILE_SHARE_WRITE));
		HANDLE handle = CreateFile(newname, ohFlag, shhFlag, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (handle == INVALID_HANDLE_VALUE) return false;
		int nHandle = _open_osfhandle((intptr_t)handle, _O_RDONLY);
		if (nHandle == -1) {CloseHandle(handle);return false;}
		fhandle = fdopen(nHandle, (flags&0xf)==OPEN_WRITE?"wb":type);
#else
		uint16_t unix_mode = (flags&0xf)==OPEN_READ||(flags&0xf)==OPEN_READ_NO_MOD?O_RDONLY:((flags&0xf)==OPEN_WRITE?O_WRONLY:O_RDWR);
		int fd = open(newname, unix_mode);
		if (fd<0 || !share(fd, unix_mode & O_ACCMODE, flags)) {close(fd);return false;}
		fhandle = fdopen(fd, (flags&0xf)==OPEN_WRITE?"wb":type);
#endif
	} else {
		fhandle=fopen(newname,type);
	}

#ifdef DEBUG
	std::string open_msg;
	std::string flags_str;
	switch (flags & 0xf) {
		case OPEN_READ:        flags_str = "R";  break;
		case OPEN_WRITE:       flags_str = "W";  break;
		case OPEN_READWRITE:   flags_str = "RW"; break;
		case OPEN_READ_NO_MOD: flags_str = "RN"; break;
		default:               flags_str = "--";
	}
#endif

	// If we couldn't open the file, then it's possible that
	// the file is simply write-protected and the flags requested
	// RW access.  So check if this is the case:
	if (!fhandle && flags & (OPEN_READWRITE | OPEN_WRITE)) {
		// If yes, check if the file can be opened with Read-only access:
		fhandle = fopen_wrap(newname, "rb");
		if (fhandle) {

#ifdef DEBUG
			open_msg = "wanted writes but opened read-only";
#else
			// Inform the user that the file is being protected against modification.
			// If the DOS program /really/ needs to write to the file, it will
			// crash/exit and this will be one of the last messages on the screen,
			// so the user can decide to un-write-protect the file if they wish.
			// We only print one message per file to eliminate redundant messaging.
			if (IsFirstEncounter(newname)) {
				// For brevity and clarity to the user, we show just the
				// filename instead of the more cluttered absolute path.
				LOG_MSG("FILESYSTEM: protected from modification: %s",
				        get_basename(newname).c_str());
			}
#endif
		}

#ifdef DEBUG
		else {
			open_msg += "failed desired and with read-only";
		}
#endif
	}

#ifdef DEBUG
	else {
		open_msg = "succeeded with desired flags";
	}
	LOG_MSG("FILESYSTEM: flags=%2s, %-12s %s",
	        flags_str.c_str(),
	        get_basename(newname).c_str(),
	        open_msg.c_str());
#endif

	if (fhandle) {
		*file = new localFile(name, fhandle, basedir);
		(*file)->flags = flags;  // for the inheritance flag and maybe check for others.
	} else {
		// Otherwise we really can't open the file.
		DOS_SetError(DOSERR_INVALID_HANDLE);
	}
	return (fhandle != NULL);
}

FILE * localDrive::GetSystemFilePtr(char const * const name, char const * const type) {

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	return fopen_wrap(newname,type);
}

bool localDrive::GetSystemFilename(char *sysName, char const * const dosName) {

	strcpy(sysName, basedir);
	strcat(sysName, dosName);
	CROSS_FILENAME(sysName);
	dirCache.ExpandName(sysName);
	return true;
}

// Attempt to delete the file name from our local drive mount
bool localDrive::FileUnlink(char * name) {
	if (!FileExists(name)) {
		DEBUG_LOG_MSG("FS: Skipping removal of %s because it doesn't exist",
		              name);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	const char *fullname = dirCache.GetExpandName(newname);

	// Can we remove the file without issue?
	if (remove(fullname) == 0) {
		dirCache.DeleteEntry(newname);
		return true;
	}

	// Otherwise maybe the file's opened within our mount ...
	DOS_File *open_file = FindOpenFile(this, name);
	if (open_file) {
		size_t max = DOS_FILES;
		// then close and remove references (as many times as needed),
		while (open_file->IsOpen() && --max) {
			open_file->Close();
			if (open_file->RemoveRef() <= 0) {
				break;
			}
		}
		// and try removing it again.
		if (remove(fullname) == 0) {
			dirCache.DeleteEntry(newname);
			return true;
		}
	}
	DEBUG_LOG_MSG("FS: Unable to remove file %s", fullname);
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool localDrive::FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	char tempDir[CROSS_LEN];
	safe_strcpy(tempDir, basedir);
	safe_strcat(tempDir, _dir);
	CROSS_FILENAME(tempDir);

	if (allocation.mediaid==0xF0) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}
    
	char end[2]={CROSS_FILESPLIT,0};
	if (tempDir[strlen(tempDir) - 1] != CROSS_FILESPLIT)
		safe_strcat(tempDir, end);

	Bit16u id;
	if (!dirCache.FindFirst(tempDir,id)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	safe_strcpy(srchInfo[id].srch_dir, tempDir);
	dta.SetDirID(id);
	
	Bit8u sAttr;
	dta.GetSearchParams(sAttr,tempDir);

	if (this->isRemote() && this->isRemovable()) {
		// cdroms behave a bit different than regular drives
		if (sAttr == DOS_ATTR_VOLUME) {
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	} else {
		if (sAttr == DOS_ATTR_VOLUME) {
			if (is_empty(dirCache.GetLabel())) {
//				LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
//				dta.SetResult("NO_LABEL",0,0,0,DOS_ATTR_VOLUME);
//				return true;
				DOS_SetError(DOSERR_NO_MORE_FILES);
				return false;
			}
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		} else if ((sAttr & DOS_ATTR_VOLUME)  && (*_dir == 0) && !fcb_findfirst) { 
		//should check for a valid leading directory instead of 0
		//exists==true if the volume label matches the searchmask and the path is valid
			if (WildFileCmp(dirCache.GetLabel(),tempDir)) {
				dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
				return true;
			}
		}
	}
	return FindNext(dta);
}

bool localDrive::FindNext(DOS_DTA & dta) {

	char * dir_ent;
	struct stat stat_block;
	char full_name[CROSS_LEN];
	char dir_entcopy[CROSS_LEN];

	Bit8u srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	Bit8u find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern);
	Bit16u id = dta.GetDirID();

again:
	if (!dirCache.FindNext(id,dir_ent)) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	if (!WildFileCmp(dir_ent,srch_pattern)) goto again;

	safe_strcpy(full_name, srchInfo[id].srch_dir);
	safe_strcat(full_name, dir_ent);

	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory 
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	safe_strcpy(dir_entcopy, dir_ent);
	char *temp_name = dirCache.GetExpandName(full_name);
	if (stat(temp_name, &stat_block) != 0) {
		goto again;//No symlinks and such
	}

	if (stat_block.st_mode & S_IFDIR)
		find_attr = DOS_ATTR_DIRECTORY;
	else
		find_attr = 0;
#if defined(WIN32)
	constexpr int8_t maximum_attribs = 0x3f;
	Bitu attribs = GetFileAttributes(temp_name);
	if (attribs != INVALID_FILE_ATTRIBUTES)
		find_attr |= attribs & maximum_attribs;
#else
	if (!(find_attr & DOS_ATTR_DIRECTORY))
		find_attr |= DOS_ATTR_ARCHIVE;
	if (!(stat_block.st_mode & S_IWUSR))
		find_attr |= DOS_ATTR_READ_ONLY;
#endif
	if (~srch_attr & find_attr &
	    (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM))
		goto again;

	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII] = "";
	uint16_t find_date;
	uint16_t find_time;
	uint32_t find_size;

	if (safe_strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII) {
		safe_strcpy(find_name, dir_entcopy);
		upcase(find_name);
	} 

	find_size=(Bit32u) stat_block.st_size;
	struct tm datetime;
	if (cross::localtime_r(&stat_block.st_mtime, &datetime)) {
		find_date = DOS_PackDate(datetime);
		find_time = DOS_PackTime(datetime);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool localDrive::GetFileAttr(char *name, uint16_t *attr)
{
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

#if defined(WIN32)
	Bitu attribs = GetFileAttributes(newname);
	if (attribs == INVALID_FILE_ATTRIBUTES) {
		DOS_SetError((uint16_t)GetLastError());
		return false;
	}
	*attr = attribs & 0x3f;
	return true;
#else
	struct stat status;
	if (stat(newname, &status) == 0) {
		*attr = status.st_mode & S_IFDIR ? 0 : DOS_ATTR_ARCHIVE;
		if (status.st_mode & S_IFDIR)
			*attr |= DOS_ATTR_DIRECTORY;
		if (!(status.st_mode & S_IWUSR))
			*attr |= DOS_ATTR_READ_ONLY;
		return true;
	}
	*attr = 0;
	return false;
#endif
}

bool localDrive::SetFileAttr(const char *name, const uint16_t attr)
{
	char newname[CROSS_LEN];
	strcpy(newname, basedir);
	strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

#if defined(WIN32)
	if (!SetFileAttributes(newname, attr)) {
		DOS_SetError((uint16_t)GetLastError());
		return false;
	}
#else
	const auto f = std_fs::path(newname);

	if (!path_exists(f)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	if (attr & (DOS_ATTR_SYSTEM | DOS_ATTR_HIDDEN))
		LOG_WARNING("FILESYSTEM: Application attempted to set system or hidden"
		            " attributes for '%s', which is ignored for local drives",
		            newname);

	const auto result = attr & DOS_ATTR_READ_ONLY ? make_readonly(f)
	                                              : make_writable(f);
	if (!result) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
#endif

	// If we made it here, the attributes were applied successfully
	dirCache.EmptyCache();
	return true;
}

bool localDrive::MakeDir(char * dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	const int temp = create_dir(dirCache.GetExpandName(newdir), 0775);
	if (temp == 0)
		dirCache.CacheOut(newdir, true);
	return (temp==0);// || ((temp!=0) && (errno==EEXIST));
}

bool localDrive::RemoveDir(char * dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	int temp=rmdir(dirCache.GetExpandName(newdir));
	if (temp==0) dirCache.DeleteEntry(newdir,true);
	return (temp==0);
}

bool localDrive::TestDir(char * dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	// Skip directory test, if "\"
	size_t len = safe_strlen(newdir);
	if (len && (newdir[len-1]!='\\')) {
		// It has to be a directory !
		struct stat test;
		if (stat(newdir,&test))			return false;
		if ((test.st_mode & S_IFDIR)==0)	return false;
	};
	return path_exists(newdir);
}

bool localDrive::Rename(char * oldname,char * newname) {
	char newold[CROSS_LEN];
	safe_strcpy(newold, basedir);
	safe_strcat(newold, oldname);
	CROSS_FILENAME(newold);
	dirCache.ExpandName(newold);
	
	char newnew[CROSS_LEN];
	safe_strcpy(newnew, basedir);
	safe_strcat(newnew, newname);
	CROSS_FILENAME(newnew);
	int temp=rename(newold,dirCache.GetExpandName(newnew));
	if (temp==0) dirCache.CacheOut(newnew);
	return (temp==0);

}

bool localDrive::AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters) {
	*_bytes_sector=allocation.bytes_sector;
	*_sectors_cluster=allocation.sectors_cluster;
	*_total_clusters=allocation.total_clusters;
	*_free_clusters=allocation.free_clusters;
	return true;
}

bool localDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	struct stat temp_stat;
	if (stat(newname,&temp_stat)!=0) return false;
	if (temp_stat.st_mode & S_IFDIR) return false;
	return true;
}

bool localDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	struct stat temp_stat;
	if (stat(newname,&temp_stat)!=0) return false;
	/* Convert the stat to a FileStat */
	struct tm datetime;
	if (cross::localtime_r(&temp_stat.st_mtime, &datetime)) {
		stat_block->time = DOS_PackTime(datetime);
		stat_block->date = DOS_PackDate(datetime);
	} else {
		LOG_MSG("FS: error while converting date in: %s", name);
	}
	stat_block->size=(Bit32u)temp_stat.st_size;
	return true;
}


Bit8u localDrive::GetMediaByte(void) {
	return allocation.mediaid;
}

bool localDrive::isRemote(void) {
	return false;
}

bool localDrive::isRemovable(void) {
	return false;
}

Bits localDrive::UnMount(void) { 
	delete this;
	return 0; 
}

localDrive::localDrive(const char * startdir,
                       Bit16u _bytes_sector,
                       Bit8u _sectors_cluster,
                       Bit16u _total_clusters,
                       Bit16u _free_clusters,
                       Bit8u _mediaid)
	: write_protected_files{},
	  allocation{_bytes_sector,
	             _sectors_cluster,
	             _total_clusters,
	             _free_clusters,
	             _mediaid}
{
	safe_strcpy(basedir, startdir);
	sprintf(info,"local directory %s",startdir);
	dirCache.SetBaseDir(basedir);
}

// Updates the internal file's current position
bool localFile::ftell_and_check()
{
	if (!fhandle)
		return false;

	stream_pos = ftell(fhandle);
	if (stream_pos >= 0)
		return true;

	DEBUG_LOG_MSG("FS: Failed obtaining position in file %s", name.c_str());
	return false;
}

// Seeks the internal file to the specified position relative to whence
bool localFile::fseek_to_and_check(long pos, int whence)
{
	if (!fhandle)
		return false;

	if (fseek(fhandle, pos, whence) == 0) {
		stream_pos = pos;
		return true;
	}
	DEBUG_LOG_MSG("FS: Failed seeking to byte %ld in file %s", stream_pos, name.c_str());
	return false;
}

// Seeks the internal file to the internal position relative to whence
void localFile::fseek_and_check(int whence)
{
	static_cast<void>(fseek_to_and_check(stream_pos, whence));
}

//TODO Maybe use fflush, but that seemed to fuck up in visual c
bool localFile::Read(uint8_t *data, uint16_t *size)
{
	// check if the file is opened in write-only mode
	if ((this->flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

#if defined(WIN32)
    if (file_access_tries>0) {
        HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
        uint32_t bytesRead;
        for (int tries = file_access_tries; tries; tries--) {							// Try specified number of times
            if (ReadFile(hFile,static_cast<LPVOID>(data), (uint32_t)*size, (LPDWORD)&bytesRead, NULL)) {
                *size = (uint16_t)bytesRead;
                return true;
            }
            Sleep(25);																	// If failed (should be region locked), wait 25 millisecs
        }
        DOS_SetError((uint16_t)GetLastError());
        *size = 0;
        return false;
    }
#endif
	// Seek if we last wrote
	if (last_action == WRITE)
		if (ftell_and_check())
			fseek_and_check(SEEK_SET);

	last_action = READ;
	const auto requested = *size;
	const auto actual = static_cast<uint16_t>(fread(data, 1, requested, fhandle));
	*size = actual; // always save the actual

	// if (actual != requested)
	//	DEBUG_LOG_MSG("FS: Only read %u of %u requested bytes from file %s",
	//	              actual, requested, name.c_str());

	/* Fake harddrive motion. Inspector Gadget with soundblaster compatible */
	/* Same for Igor */
	/* hardrive motion => unmask irq 2. Only do it when it's masked as
	 * unmasking is realitively heavy to emulate */
	Bit8u mask = IO_Read(0x21);
	if (mask & 0x4)
		IO_Write(0x21, mask & 0xfb);
	return true;
}

bool localFile::Write(uint8_t *data, uint16_t *size)
{
	Bit32u lastflags = this->flags & 0xf;
	if (lastflags == OPEN_READ || lastflags == OPEN_READ_NO_MOD) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
#if defined(WIN32)
    if (file_access_tries>0) {
        HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
        if (*size == 0) {
            if (SetEndOfFile(hFile)) {
                return true;
            } else {
                DOS_SetError((uint16_t)GetLastError());
                return false;
            }
        }
        uint32_t bytesWritten;
        for (int tries = file_access_tries; tries; tries--) {							// Try three times
            if (WriteFile(hFile, data, (uint32_t)*size, (LPDWORD)&bytesWritten, NULL)) {
                *size = (uint16_t)bytesWritten;
                return true;
            }
            Sleep(25);																	// If failed (should be region locked? (not documented in MSDN)), wait 25 millisecs
        }
        DOS_SetError((uint16_t)GetLastError());
        *size = 0;
        return false;
    }
#endif

	// Seek if we last read
	if (last_action == READ)
		if (ftell_and_check())
			fseek_and_check(SEEK_SET);

	last_action = WRITE;

	// Truncate the file
	if (*size == 0) {
		const auto file = cross_fileno(fhandle);
		if (file == -1) {
			DEBUG_LOG_MSG("FS: Could not resolve file number for %s", name.c_str());
			return false;
		}
		if (!ftell_and_check()) {
			return false;
		}
		if (ftruncate(file, stream_pos) != 0) {
			DEBUG_LOG_MSG("FS: Failed truncating file %s", name.c_str());
			return false;
		}
		// Truncation succeeded if we made it here
		return true;
	}

	// Otherwise we have some data to write
	const auto requested = *size;
	const auto actual = static_cast<uint16_t>(fwrite(data, 1, requested, fhandle));
	if (actual != requested) {
		DEBUG_LOG_MSG("FS: Only wrote %u of %u requested bytes to file %s",
		              actual, requested, name.c_str());
	}
	*size = actual; // always save the actual
	return true;    // always return true, even if partially written
}

#ifndef WIN32
bool toLock(int fd, bool is_lock, uint32_t pos, uint16_t size) {
    struct flock larg;
    unsigned long mask = 0xC0000000;
    int flag = fcntl(fd, F_GETFL);
    larg.l_type = is_lock ? (flag & O_RDWR || flag & O_WRONLY ? F_WRLCK : F_RDLCK) : F_UNLCK;
    larg.l_start = pos;
    larg.l_len = size;
    larg.l_len &= ~mask;
    if ((larg.l_start & mask) != 0)
        larg.l_start = (larg.l_start & ~mask) | ((larg.l_start & mask) >> 2);
    int ret;
#ifdef F_SETLK64
    ret = lock_file_region (fd,F_SETLK64,&larg,pos,size);
    if (ret == -1 && errno == EINVAL)
#endif
    ret = lock_file_region (fd,F_SETLK,&larg,larg.l_start,larg.l_len);
    return ret != -1;
}
#endif

bool localFile::LockFile(uint8_t mode, uint32_t pos, uint16_t size) {
#if defined(WIN32)
    static bool lockWarn = true;
	HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
    if (file_access_tries>0) {
        if (mode > 1) {
            DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
            return false;
        }
        if (!mode)																		// Lock, try 3 tries
            for (int tries = file_access_tries; tries; tries--) {
                if (::LockFile(hFile, pos, 0, size, 0)) {
                    if (lockWarn && ::LockFile(hFile, pos, 0, size, 0)) {
                        lockWarn = false;
                        char caption[512];
                        strcat(strcpy(caption, "Windows reference: "), dynamic_cast<localDrive*>(Drives[GetDrive()])->GetBasedir());
                        MessageBox(NULL, "Record locking seems incorrectly implemented!\nConsult ...", caption, MB_OK|MB_ICONSTOP);
                    }
                    return true;
                }
                Sleep(25);																// If failed, wait 25 millisecs
            }
        else if (::UnlockFile(hFile, pos, 0, size, 0))									// This is a somewhat permanet condition!
            return true;
        DOS_SetError((uint16_t)GetLastError());
        return false;
    }
	BOOL bRet;
#else
	bool bRet;
#endif

	switch (mode)
	{
#if defined(WIN32)
	case 0: bRet = ::LockFile (hFile, pos, 0, size, 0); break;
	case 1: bRet = ::UnlockFile(hFile, pos, 0, size, 0); break;
#else
	case 0: bRet = toLock(fileno(fhandle), true, pos, size); break;
	case 1: bRet = toLock(fileno(fhandle), false, pos, size); break;
#endif
	default:
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	}
	//LOG_MSG("::LockFile %s", name);

	if (!bRet)
	{
#if defined(WIN32)
		switch (GetLastError())
		{
		case ERROR_ACCESS_DENIED:
		case ERROR_LOCK_VIOLATION:
		case ERROR_NETWORK_ACCESS_DENIED:
		case ERROR_DRIVE_LOCKED:
		case ERROR_SEEK_ON_DEVICE:
		case ERROR_NOT_LOCKED:
		case ERROR_LOCK_FAILED:
			DOS_SetError(0x21);
			break;
		case ERROR_INVALID_HANDLE:
			DOS_SetError(DOSERR_INVALID_HANDLE);
			break;
		case ERROR_INVALID_FUNCTION:
		default:
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			break;
		}
#else
		switch (errno)
		{
		case EINTR:
		case ENOLCK:
		case EAGAIN:
			DOS_SetError(0x21);
			break;
		case EBADF:
			DOS_SetError(DOSERR_INVALID_HANDLE);
			break;
		case EINVAL:
		default:
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			break;
		}
#endif
	}
	return bRet;
}

extern const char* RunningProgram;
bool localFile::Seek(uint32_t *pos_addr, uint32_t type)
{
	int seektype;
	switch (type) {
	case DOS_SEEK_SET:seektype=SEEK_SET;break;
	case DOS_SEEK_CUR:seektype=SEEK_CUR;break;
	case DOS_SEEK_END:seektype=SEEK_END;break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}

#if defined(WIN32)
    if (file_access_tries>0) {
        HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
        int32_t dwPtr = SetFilePointer(hFile, *pos_addr, NULL, type);
        if (dwPtr == (int32_t)INVALID_SET_FILE_POINTER && !strcmp(RunningProgram, "BTHORNE"))	// Fix for Black Thorne
            dwPtr = SetFilePointer(hFile, 0, NULL, DOS_SEEK_END);
        if (dwPtr != (int32_t)INVALID_SET_FILE_POINTER) {										// If success
            *pos_addr = (uint32_t)dwPtr;
            return true;
        }
        DOS_SetError((uint16_t)GetLastError());
        return false;
    }
#endif
	// The inbound position is actually an int32_t being passed through a
	// uint32_t* pointer (pos_addr), so reinterpret the underlying memory as
	// such to prevent rollover into the unsigned range.
	const auto pos = *reinterpret_cast<int32_t *>(pos_addr);
	if (!fseek_to_and_check(pos, seektype)) {
		// Failed to seek, but try again this time seeking to
		// the end of file, which satisfies Black Thorne.
		stream_pos = 0;
		fseek_and_check(SEEK_END);
	}
#if 0
	fpos_t temppos;
	fgetpos(fhandle,&temppos);
	Bit32u * fake_pos=(Bit32u*)&temppos;
	*pos_addr = *fake_pos;
#endif
	static_cast<void>(ftell_and_check());

	// The inbound position is actually an int32_t being passed through a
	// uint32_t* pointer (pos_addr), so before we save the seeked position
	// back into it we first ensure the current long stream_pos (which is a
	// signed 64-bit on some platforms + OSes) can fit within the int32_t
	// range before assigning it.
	assert(stream_pos >= std::numeric_limits<int32_t>::min() &&
	       stream_pos <= std::numeric_limits<int32_t>::max());
	*reinterpret_cast<int32_t *>(pos_addr) = static_cast<int32_t>(stream_pos);

	last_action = NONE;
	return true;
}

bool localFile::Close() {
	// only close if one reference left
	if (refCtr==1) {
		if (fhandle) fclose(fhandle);
		fhandle = 0;
		open = false;
	};

	if (newtime) {
		// backport from DOS_PackDate() and DOS_PackTime()
		struct tm tim = {};
		tim.tm_sec = (time & 0x1f) * 2;
		tim.tm_min = (time >> 5) & 0x3f;
		tim.tm_hour = (time >> 11) & 0x1f;
		tim.tm_mday = date & 0x1f;
		tim.tm_mon = ((date >> 5) & 0x0f) - 1;
		tim.tm_year = (date >> 9) + 1980 - 1900;
		//  have the C run-time library code compute whether standard
		//  time or daylight saving time is in effect.
		tim.tm_isdst = -1;
		// serialize time
		mktime(&tim);

		utimbuf ftim;
		ftim.actime = ftim.modtime = mktime(&tim);

		char fullname[CROSS_LEN];
		safe_sprintf(fullname, "%s%s", basedir, name.c_str());
		CROSS_FILENAME(fullname);

		// FIXME: utime is deprecated, need a modern cross-platform
		// implementation.
		if (utime(fullname, &ftim)) {
			return false;
		}
	}

	return true;
}

Bit16u localFile::GetInformation(void) {
	return read_only_medium?0x40:0;
}

localFile::localFile(const char *_name, FILE *handle, const char *_basedir)
        : fhandle(handle),
          basedir(_basedir),
          read_only_medium(false),
          last_action(NONE)
{
	open=true;
	UpdateDateTimeFromHost();

	attr=DOS_ATTR_ARCHIVE;

	SetName(_name);
}

bool localFile::UpdateDateTimeFromHost()
{
	if (!open)
		return false;

	// Legal defaults if we're unable to populate them
	time = 1;
	date = 1;

	const auto file = cross_fileno(fhandle);
	if (file == -1)
		return true; // use defaults

	struct stat temp_stat;
	if (fstat(file, &temp_stat) == -1)
		return true; // use defaults

	struct tm datetime;
	if (!cross::localtime_r(&temp_stat.st_mtime, &datetime))
		return true; // use defaults

	time = DOS_PackTime(datetime);
	date = DOS_PackDate(datetime);
	return true;
}

void localFile::Flush()
{
#if defined(WIN32)
    if (file_access_tries>0) return;
#endif
	if (last_action != WRITE)
		return;

	if (ftell_and_check())
		fseek_and_check(SEEK_SET);

	// Always reset the state even if the file is broken
	last_action = NONE;
}

// ********************************************
// CDROM DRIVE
// ********************************************

cdromDrive::cdromDrive(const char _driveLetter,
                       const char * startdir,
                       Bit16u _bytes_sector,
                       Bit8u _sectors_cluster,
                       Bit16u _total_clusters,
                       Bit16u _free_clusters,
                       Bit8u _mediaid,
                       int& error)
	: localDrive(startdir,
	             _bytes_sector,
	             _sectors_cluster,
	             _total_clusters,
	             _free_clusters,
	             _mediaid),
	  subUnit(0),
	  driveLetter(_driveLetter)
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	safe_strcpy(info, "CDRom ");
	safe_strcat(info, startdir);
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
}

bool cdromDrive::FileOpen(DOS_File * * file,char * name,Bit32u flags) {
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~static_cast<unsigned>(OPEN_READWRITE);
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool success = localDrive::FileOpen(file, name, flags);
	if (success)
		(*file)->SetFlagReadOnlyMedium();
	return success;
}

bool cdromDrive::FileCreate(DOS_File * * /*file*/,char * /*name*/,Bit16u /*attributes*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::FileUnlink(char * /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::RemoveDir(char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::MakeDir(char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::Rename(char * /*oldname*/,char * /*newname*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::GetFileAttr(char * name, uint16_t * attr) {
	bool result = localDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
}

bool cdromDrive::FindFirst(char * _dir,DOS_DTA & dta,bool /*fcb_findfirst*/) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return localDrive::FindFirst(_dir,dta);
}

void cdromDrive::SetDir(const char* path) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	localDrive::SetDir(path);
}

bool cdromDrive::isRemote(void) {
	return true;
}

bool cdromDrive::isRemovable(void) {
	return true;
}

Bits cdromDrive::UnMount(void) {
	if (MSCDEX_RemoveDrive(driveLetter)) {
		delete this;
		return 0;
	}
	return 2;
}
