#include "sysinfo.h"

#include <stdint.h>
#include <ctype.h>    // for isspace()
#include <stdlib.h>   // for getenv()
#include <stdio.h>    // for snprintf(), sscanf()
#include <string.h>   // for memmove(), memchr(), etc.
#include <fcntl.h>    // for open()
#include <errno.h>    // for errno
#include <assert.h>
#ifdef _WIN32
#include <process.h>          // for getpid() (actually, _getpid())
#include <shlwapi.h>          // for SHGetValueA()
#include <tlhelp32.h>         // for Module32First()
#else
#include <unistd.h>   // for read()
#endif // _WIN32

#ifdef _WIN32
#ifdef MODULEENTRY32

#undef MODULEENTRY32
#undef Module32First
#undef Module32Next
#undef PMODULEENTRY32
#undef LPMODULEENTRY32
#endif  /* MODULEENTRY32 */
// MinGW doesn't seem to define this, perhaps some windowsen don't either.
#ifndef TH32CS_SNAPMODULE32
#define TH32CS_SNAPMODULE32  0
#endif  /* TH32CS_SNAPMODULE32 */
#endif // _WIN32

// Re-run fn until it doesn't cause EINTR.
#define NO_INTR(fn)  do {} while ((fn) < 0 && errno == EINTR)

int GetSystemCPUsCount()
{
#ifdef _WIN32
	// Get the number of processors.
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return  info.dwNumberOfProcessors;
#else
	long rv = sysconf(_SC_NPROCESSORS_ONLN);
	if (rv < 0)
	{
		return 1;
	}
	return static_cast<int>(rv);
#endif // _WIN32
}

// ----------------------------------------------------------------------

#ifndef _WIN32
static void ConstructFilename(const char* spec, pid_t pid,
	char* buf, int buf_size)
{
	if (snprintf(buf, buf_size,
		spec,
		static_cast<int>(pid ? pid : getpid())) < buf_size)
	{
		assert(false);
	}
}
#endif // !_WIN32

static bool ExtractUntilChar(char* text, int c, char** endptr)
{
	assert(text);
	assert(endptr);
	char* found;
	found = strchr(text, c);
	if (found == NULL)
	{
		*endptr = NULL;
		return false;
	}

	*endptr = found;
	*found = '\0';
	return true;
}

static void SkipWhileWhitespace(char** text_pointer, int c)
{
	if (isspace(c))
	{
		while (isspace(**text_pointer) && isspace(*((*text_pointer) + 1)))
		{
			++(*text_pointer);
		}
	}
}

template<class T>
static T StringToInteger(char* text, char** endptr, int base)
{
	assert(false);
	return T();
}

template<>
int StringToInteger<int>(char* text, char** endptr, int base)
{
	return strtol(text, endptr, base);
}

template<>
long long StringToInteger<long long>(char* text, char** endptr, int base)
{
	return strtoll(text, endptr, base);
}

template<>
unsigned long long StringToInteger<unsigned long long>(char* text, char** endptr, int base)
{
	return strtoull(text, endptr, base);
}

template<typename T>
static T StringToIntegerUntilChar(
	char* text, int base, int c, char** endptr_result)
{
	T result;
	assert(endptr_result);
	*endptr_result = NULL;

	char* endptr_extract;
	if (!ExtractUntilChar(text, c, &endptr_extract))
		return 0;

	char* endptr_strto;
	result = StringToInteger<T>(text, &endptr_strto, base);
	*endptr_extract = c;

	if (endptr_extract != endptr_strto)
		return 0;

	*endptr_result = endptr_extract;
	SkipWhileWhitespace(endptr_result, c);

	return result;
}

static char* CopyStringUntilChar(
	char* text, unsigned out_len, int c, char* out)
{
	char* endptr;
	if (!ExtractUntilChar(text, c, &endptr))
		return NULL;

	strncpy(out, text, out_len);
	out[out_len - 1] = '\0';
	*endptr = c;

	SkipWhileWhitespace(&endptr, c);
	return endptr;
}

template<typename T>
static bool StringToIntegerUntilCharWithCheck(
	T* outptr, char* text, int base, int c, char** endptr)
{
	*outptr = StringToIntegerUntilChar<T>(*endptr, base, c, endptr);
	if (*endptr == NULL || **endptr == '\0') return false;
	++(*endptr);
	return true;
}

static bool ParseProcMapsLine(char* text, unsigned long long* start, unsigned long long* end
	, char* flags, unsigned long long* offset
	, int* major, int* minor, long long* inode
	, unsigned* filename_offset)
{
#ifdef _WIN32
	return false;
#else
	char* endptr = text;
	if (endptr == NULL || *endptr == '\0')  return false;

	if (!StringToIntegerUntilCharWithCheck(start, endptr, 16, '-', &endptr))
		return false;

	if (!StringToIntegerUntilCharWithCheck(end, endptr, 16, ' ', &endptr))
		return false;

	endptr = CopyStringUntilChar(endptr, 5, ' ', flags);
	if (endptr == NULL || *endptr == '\0')  return false;
	++endptr;

	if (!StringToIntegerUntilCharWithCheck(offset, endptr, 16, ' ', &endptr))
		return false;

	if (!StringToIntegerUntilCharWithCheck(major, endptr, 16, ':', &endptr))
		return false;

	if (!StringToIntegerUntilCharWithCheck(minor, endptr, 16, ' ', &endptr))
		return false;

	if (!StringToIntegerUntilCharWithCheck(inode, endptr, 10, ' ', &endptr))
		return false;

	*filename_offset = (endptr - text);
	return true;
#endif // _WIN32
}

ProcMapsIterator::ProcMapsIterator(pid_t pid)
{
	Init(pid, NULL, false);
}

ProcMapsIterator::ProcMapsIterator(pid_t pid, Buffer* buffer)
{
	Init(pid, buffer, false);
}

ProcMapsIterator::ProcMapsIterator(pid_t pid, Buffer* buffer, bool use_maps_backing)
{
	Init(pid, buffer, use_maps_backing);
}

void ProcMapsIterator::Init(pid_t pid, Buffer* buffer, bool use_maps_backing)
{
	pid_ = pid;
	using_maps_backing_ = use_maps_backing;
	dynamic_buffer_ = NULL;
	if (!buffer)
	{
		buffer = dynamic_buffer_ = new Buffer;
	}
	else
	{
		dynamic_buffer_ = NULL;
	}

	ibuf_ = buffer->buf_;

	stext_ = etext_ = nextline_ = ibuf_;
	ebuf_ = ibuf_ + Buffer::kBufSize - 1;
	nextline_ = ibuf_;

#ifdef _WIN32
	snapshot_ = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE |
		TH32CS_SNAPMODULE32,
		GetCurrentProcessId());
	memset(&module_, 0, sizeof(module_));
#else
	if (use_maps_backing)
	{  // don't bother with clever "self" stuff in this case
		ConstructFilename("/proc/%d/maps_backing", pid, ibuf_, Buffer::kBufSize);
	}
	else if (pid == 0)
	{
		ConstructFilename("/proc/self/maps", 1, ibuf_, Buffer::kBufSize);
	}
	else
	{
		ConstructFilename("/proc/%d/maps", pid, ibuf_, Buffer::kBufSize);
	}
	NO_INTR(fd_ = open(ibuf_, O_RDONLY));
#endif // _WIN32
}

ProcMapsIterator::~ProcMapsIterator()
{
#ifdef _WIN32
	if (snapshot_ != INVALID_HANDLE_VALUE) CloseHandle(snapshot_);
#else
	if (fd_ >= 0) NO_INTR(close(fd_));
#endif // _WIN32
	delete dynamic_buffer_;
}

bool ProcMapsIterator::Valid() const
{
#ifdef _WIN32
	return snapshot_ != INVALID_HANDLE_VALUE;
#else
	return fd_ != -1;
#endif // _WIN32
}

bool ProcMapsIterator::Next(unsigned long long* start, unsigned long long* end
	, char** flags, unsigned long long* offset, long long* inode, char** filename)
{
	return NextExt(start, end, flags, offset, inode, filename
		, NULL, NULL, NULL, NULL, NULL);
}

bool ProcMapsIterator::NextExt(unsigned long long* start, unsigned long long* end, char** flags
	, unsigned long long* offset, long long* inode, char** filename
	, unsigned long long* file_mapping, unsigned long long* file_pages
	, unsigned long long* anon_mapping, unsigned long long* anon_pages
	, dev_t* dev)
{
#ifdef _WIN32
	static char kDefaultPerms[5] = "r-xp";
	BOOL ok;
	if (module_.dwSize == 0)
	{  // only possible before first call
		module_.dwSize = sizeof(module_);
		ok = Module32First(snapshot_, &module_);
	}
	else
	{
		ok = Module32Next(snapshot_, &module_);
	}
	if (ok)
	{
		unsigned long long base_addr = reinterpret_cast<DWORD_PTR>(module_.modBaseAddr);
		if (start) *start = base_addr;
		if (end) *end = base_addr + module_.modBaseSize;
		if (flags) *flags = kDefaultPerms;
		if (offset) *offset = 0;
		if (inode) *inode = 0;
		if (filename) *filename = module_.szExePath;
		if (file_mapping) *file_mapping = 0;
		if (file_pages) *file_pages = 0;
		if (anon_mapping) *anon_mapping = 0;
		if (anon_pages) *anon_pages = 0;
		if (dev) *dev = 0;
		return true;
	}
#else
	do
	{
		// Advance to the start of the next line
		stext_ = nextline_;

		// See if we have a complete line in the buffer already
		nextline_ = static_cast<char*>(memchr(stext_, '\n', etext_ - stext_));
		if (!nextline_)
		{
			// Shift/fill the buffer so we do have a line
			int count = etext_ - stext_;

			// Move the current text to the start of the buffer
			memmove(ibuf_, stext_, count);
			stext_ = ibuf_;
			etext_ = ibuf_ + count;

			int nread = 0;            // fill up buffer with text
			while (etext_ < ebuf_)
			{
				NO_INTR(nread = read(fd_, etext_, ebuf_ - etext_));
				if (nread > 0)
					etext_ += nread;
				else
					break;
			}

			// Zero out remaining characters in buffer at EOF to avoid returning
			// garbage from subsequent calls.
			if (etext_ != ebuf_ && nread == 0)
			{
				memset(etext_, 0, ebuf_ - etext_);
			}
			*etext_ = '\n';   // sentinel; safe because ibuf extends 1 char beyond ebuf
			nextline_ = static_cast<char*>(memchr(stext_, '\n', etext_ + 1 - stext_));
		}
		*nextline_ = 0;                // turn newline into nul
		nextline_ += ((nextline_ < etext_) ? 1 : 0);  // skip nul if not end of text
		// stext_ now points at a nul-terminated line
		unsigned long long tmpstart, tmpend, tmpoffset;
		long long tmpinode;
		int major, minor;
		unsigned filename_offset = 0;

		// for now, assume all linuxes have the same format
		if (!ParseProcMapsLine(stext_
			, start ? start : &tmpstart
			, end ? end : &tmpend
			, flags_
			, offset ? offset : &tmpoffset
			, &major, &minor
			, inode ? inode : &tmpinode, &filename_offset)) continue;

		size_t stext_length = strlen(stext_);
		if (filename_offset == 0 || filename_offset > stext_length)
			filename_offset = stext_length;

		// We found an entry
		if (flags) *flags = flags_;
		if (filename) *filename = stext_ + filename_offset;
		if (dev) *dev = minor | (major << 8);

		if (using_maps_backing_)
		{
			// Extract and parse physical page backing info.
			char* backing_ptr = stext_ + filename_offset +
				strlen(stext_ + filename_offset);

			// find the second '('
			int paren_count = 0;
			while (--backing_ptr > stext_)
			{
				if (*backing_ptr == '(')
				{
					++paren_count;
					if (paren_count >= 2)
					{
						unsigned long long tmp_file_mapping;
						unsigned long long tmp_file_pages;
						unsigned long long tmp_anon_mapping;
						unsigned long long tmp_anon_pages;

						sscanf(backing_ptr + 1, "F %" "llx" " %" "llx" ") (A %" "llx" " %" "llx" ")",
							file_mapping ? file_mapping : &tmp_file_mapping,
							file_pages ? file_pages : &tmp_file_pages,
							anon_mapping ? anon_mapping : &tmp_anon_mapping,
							anon_pages ? anon_pages : &tmp_anon_pages);
						// null terminate the file name (there is a space
						// before the first (.
						backing_ptr[-1] = 0;
						break;
					}
				}
			}
		}

		return true;
	} while (etext_ > ibuf_);
#endif // _WIN32

	return false;
}

int ProcMapsIterator::FormatLine(char* buffer, int bufsize,
	unsigned long long start, unsigned long long end, const char* flags,
	unsigned long long offset, long long inode,
	const char* filename, dev_t dev)
{
	// We assume 'flags' looks like 'rwxp' or 'rwx'.
	char r = (flags && flags[0] == 'r') ? 'r' : '-';
	char w = (flags && flags[0] && flags[1] == 'w') ? 'w' : '-';
	char x = (flags && flags[0] && flags[1] && flags[2] == 'x') ? 'x' : '-';
	// p always seems set on linux, so we set the default to 'p', not '-'
	char p = (flags && flags[0] && flags[1] && flags[2] && flags[3] != 'p')
		? '-' : 'p';

	const int rc = snprintf(buffer, bufsize,
		"%08" "llx" "-%08" "llx" " %c%c%c%c %08" "llx" " %02x:%02x %-11" "lld" " %s\n",
		start, end, r, w, x, p, offset,
		static_cast<int>(dev / 256), static_cast<int>(dev % 256),
		inode, filename);
	return (rc < 0 || rc >= bufsize) ? 0 : rc;
}
