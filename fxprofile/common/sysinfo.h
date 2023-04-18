#ifndef _SYSINFO_H_
#define _SYSINFO_H_

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>  // for CreateToolhelp32Snapshot
#else
#include <sys/types.h>
#include <unistd.h>    // for pid_t
#endif

#ifdef _WIN32
typedef int pid_t;
typedef unsigned int _dev_t;
typedef _dev_t dev_t;
#endif // _WIN32

extern int GetSystemCPUsCount();

class ProcMapsIterator
{
public:
	struct Buffer
	{
		static const size_t kBufSize = 256 + 1024;
		char buf_[kBufSize];
	};

	explicit ProcMapsIterator(pid_t pid);

	ProcMapsIterator(pid_t pid, Buffer* buffer);

	ProcMapsIterator(pid_t pid, Buffer* buffer,
		bool use_maps_backing);

	bool Valid() const;

	const char* CurrentLine() const
	{
		return stext_;
	}

	static int FormatLine(char* buffer, int bufsize,
		unsigned long long start, unsigned long long end, const char* flags,
		unsigned long long offset, long long inode, const char* filename,
		dev_t dev);

	bool Next(unsigned long long* start, unsigned long long* end, char** flags,
		unsigned long long* offset, long long* inode, char** filename);

	bool NextExt(unsigned long long* start, unsigned long long* end, char** flags,
		unsigned long long* offset, long long* inode, char** filename,
		unsigned long long* file_mapping, unsigned long long* file_pages,
		unsigned long long* anon_mapping, unsigned long long* anon_pages,
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


#endif   /* #ifndef _SYSINFO_H_ */
