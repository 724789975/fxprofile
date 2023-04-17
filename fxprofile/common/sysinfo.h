#ifndef _SYSINFO_H_
#define _SYSINFO_H_

#include <time.h>
#ifdef _WIN32
#include <windows.h>   // for DWORD
#include <tlhelp32.h>  // for CreateToolhelp32Snapshot
#else
#include <sys/types.h>
#include <unistd.h>    // for pid_t
#endif
#include <stddef.h>    // for size_t
#include <limits.h>    // for PATH_MAX
#include <stdint.h>

#ifndef HAVE_PID_T
typedef int pid_t;
#endif

#ifdef _WIN32
typedef unsigned int _dev_t;
typedef _dev_t dev_t;
#endif // _WIN32

extern const char* GetenvBeforeMain(const char* name);

extern bool GetUniquePathFromEnv(const char* env_name, char* path);

extern int GetSystemCPUsCount();

void SleepForMilliseconds(int milliseconds);

bool HasPosixThreads();

#ifndef SWIG  // SWIG doesn't like struct Buffer and variable arguments.

class ProcMapsIterator {
public:
	struct Buffer {
		static const size_t kBufSize = 256 + 1024;
		char buf_[kBufSize];
	};

	explicit ProcMapsIterator(pid_t pid);

	ProcMapsIterator(pid_t pid, Buffer* buffer);

	ProcMapsIterator(pid_t pid, Buffer* buffer,
		bool use_maps_backing);

	bool Valid() const;

	const char* CurrentLine() const { return stext_; }

	static int FormatLine(char* buffer, int bufsize,
		uint64_t start, uint64_t end, const char* flags,
		uint64_t offset, int64_t inode, const char* filename,
		dev_t dev);

	bool Next(uint64_t* start, uint64_t* end, char** flags,
		uint64_t* offset, int64_t* inode, char** filename);

	bool NextExt(uint64_t* start, uint64_t* end, char** flags,
		uint64_t* offset, int64_t* inode, char** filename,
		uint64_t* file_mapping, uint64_t* file_pages,
		uint64_t* anon_mapping, uint64_t* anon_pages,
		dev_t* dev);

	~ProcMapsIterator();

private:
	void Init(pid_t pid, Buffer* buffer, bool use_maps_backing);

	char* ibuf_;        // input buffer
	char* stext_;       // start of text
	char* etext_;       // end of text
	char* nextline_;    // start of next line
	char* ebuf_;        // end of buffer (1 char for a nul)

#ifdef _WIN32
	HANDLE snapshot_;   // filehandle on dll info
#ifdef MODULEENTRY32  // Alias of W
#undef MODULEENTRY32
	MODULEENTRY32 module_;   // info about current dll (and dll iterator)
#define MODULEENTRY32 MODULEENTRY32W
#else  // It's the ascii, the one we want.
	MODULEENTRY32 module_;   // info about current dll (and dll iterator)
#endif
#else
	int fd_;            // filehandle on /proc/*/maps
#endif // _WIN32

	pid_t pid_;
	char flags_[10];
	Buffer* dynamic_buffer_;  // dynamically-allocated Buffer
	bool using_maps_backing_; // true if we are looking at maps_backing instead of maps.
};

#endif  /* #ifndef SWIG */

// Helper routines

#ifdef _WIN32
typedef HANDLE RawFD;
const RawFD kIllegalRawFD = INVALID_HANDLE_VALUE;
#else
typedef int RawFD;
const RawFD kIllegalRawFD = -1;   // what open returns if it fails
#endif  // defined(_WIN32)

#endif   /* #ifndef _SYSINFO_H_ */
