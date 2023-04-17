#if (defined(_WIN32) || defined(__MINGW32__)) && !defined(__CYGWIN__) && !defined(__CYGWIN32)
# define PLATFORM_WINDOWS 1
#endif

#include "sysinfo.h"

#include <stdint.h>
#include <ctype.h>    // for isspace()
#include <stdlib.h>   // for getenv()
#include <stdio.h>    // for snprintf(), sscanf()
#include <string.h>   // for memmove(), memchr(), etc.
#include <fcntl.h>    // for open()
#include <errno.h>    // for errno
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

// open/read/close can set errno, which may be illegal at this
// time, so prefer making the syscalls directly if we can.
#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

#ifdef SYS_open   // solaris 11, at least sometimes, only defines SYS_openat
# define safeopen(filename, mode)  syscall(SYS_open, filename, mode)
#else
# define safeopen(filename, mode)  open(filename, mode)
#endif

#ifdef SYS_read
# define saferead(fd, buffer, size)  syscall(SYS_read, fd, buffer, size)
#else
# define saferead(fd, buffer, size)  read(fd, buffer, size)
#endif
#ifdef SYS_close
# define safeclose(fd)  syscall(SYS_close, fd)
#else
# define safeclose(fd)  close(fd)
#endif

// ----------------------------------------------------------------------
// GetenvBeforeMain()
// GetUniquePathFromEnv()
//    Some non-trivial getenv-related functions.
// ----------------------------------------------------------------------

// we reimplement memcmp and friends to avoid depending on any glibc
// calls too early in the process lifetime. This allows us to use
// GetenvBeforeMain from inside ifunc handler
static int slow_memcmp(const void* _a, const void* _b, size_t n) {
	const uint8_t* a = reinterpret_cast<const uint8_t*>(_a);
	const uint8_t* b = reinterpret_cast<const uint8_t*>(_b);
	while (n-- != 0) {
		uint8_t ac = *a++;
		uint8_t bc = *b++;
		if (ac != bc) {
			if (ac < bc) {
				return -1;
			}
			return 1;
		}
	}
	return 0;
}

static const char* slow_memchr(const char* s, int c, size_t n) {
	uint8_t ch = static_cast<uint8_t>(c);
	while (n--) {
		if (*s++ == ch) {
			return s - 1;
		}
	}
	return 0;
}

static size_t slow_strlen(const char* s) {
	const char* s2 = slow_memchr(s, '\0', static_cast<size_t>(-1));
	return s2 - s;
}

const char* GetenvBeforeMain(const char* name) {
	//  const int namelen = slow_strlen(name);
	//#if defined(HAVE___ENVIRON)   // if we have it, it's declared in unistd.h
	//  if (__environ) {            // can exist but be NULL, if statically linked
	//    for (char** p = __environ; *p; p++) {
	//      if (!slow_memcmp(*p, name, namelen) && (*p)[namelen] == '=')
	//        return *p + namelen+1;
	//    }
	//    return NULL;
	//  }
	//#endif
	//#if defined(PLATFORM_WINDOWS)
	//  // TODO(mbelshe) - repeated calls to this function will overwrite the
	//  // contents of the static buffer.
	//  static char envvar_buf[1024];  // enough to hold any envvar we care about
	//  if (!GetEnvironmentVariableA(name, envvar_buf, sizeof(envvar_buf)-1))
	//    return NULL;
	//  return envvar_buf;
	//#endif
	//  // static is ok because this function should only be called before
	//  // main(), when we're single-threaded.
	//  static char envbuf[16<<10];
	//  if (*envbuf == '\0') {    // haven't read the environ yet
	//    int fd = safeopen("/proc/self/environ", O_RDONLY);
	//    // The -2 below guarantees the last two bytes of the buffer will be \0\0
	//    if (fd == -1 ||           // unable to open the file, fall back onto libc
	//        saferead(fd, envbuf, sizeof(envbuf) - 2) < 0) { // error reading file
	//      if (fd != -1) safeclose(fd);
	//      return getenv(name);
	//    }
	//    safeclose(fd);
	//  }
	//  const char* p = envbuf;
	//  while (*p != '\0') {    // will happen at the \0\0 that terminates the buffer
	//    // proc file has the format NAME=value\0NAME=value\0NAME=value\0...
	//    const char* endp = (char*)slow_memchr(p, '\0',
	//                                          sizeof(envbuf) - (p - envbuf));
	//    if (endp == NULL)            // this entry isn't NUL terminated
	//      return NULL;
	//    else if (!slow_memcmp(p, name, namelen) && p[namelen] == '=')    // it's a match
	//      return p + namelen+1;      // point after =
	//    p = endp + 1;
	//  }
	return NULL;                   // env var never found
}

const char* TCMallocGetenvSafe(const char* name) {
	return GetenvBeforeMain(name);
}

bool GetUniquePathFromEnv(const char* env_name, char* path) {
	//char* envval = getenv(env_name);
	//if (envval == NULL || *envval == '\0')
	//  return false;
	//if (envval[0] & 128) {                  // high bit is set
	//  snprintf(path, PATH_MAX, "%c%s_%u",   // add pid and clear high bit
	//           envval[0] & 127, envval+1, (unsigned int)(getpid()));
	//} else {
	//  snprintf(path, PATH_MAX, "%s", envval);
	//  envval[0] |= 128;                     // set high bit for kids to see
	//}
	return true;
}

void SleepForMilliseconds(int milliseconds) {
	//#ifdef PLATFORM_WINDOWS
	//	_sleep(milliseconds);   // Windows's _sleep takes milliseconds argument
	//#else
	//	// Sleep for a few milliseconds
	//	struct timespec sleep_time;
	//	sleep_time.tv_sec = milliseconds / 1000;
	//	sleep_time.tv_nsec = (milliseconds % 1000) * 1000000;
	//	while (nanosleep(&sleep_time, &sleep_time) != 0 && errno == EINTR)
	//		;  // Ignore signals and wait for the full interval to elapse.
	//#endif
}

int GetSystemCPUsCount()
{
#ifdef _WIN32
	// Get the number of processors.
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return  info.dwNumberOfProcessors;
#else
	long rv = sysconf(_SC_NPROCESSORS_ONLN);
	if (rv < 0) {
		return 1;
	}
	return static_cast<int>(rv);
#endif // _WIN32
}

// ----------------------------------------------------------------------

#ifndef _WIN32
static void ConstructFilename(const char* spec, pid_t pid,
	char* buf, int buf_size) {
	if (snprintf(buf, buf_size,
		spec,
		static_cast<int>(pid ? pid : getpid())) < buf_size)
	{
		//TODO error
	}
}
#endif // !_WIN32

static bool ExtractUntilChar(char* text, int c, char** endptr) {
	//CHECK_NE(text, NULL);
	//CHECK_NE(endptr, NULL);
	//char *found;
	//found = strchr(text, c);
	//if (found == NULL) {
	//  *endptr = NULL;
	//  return false;
	//}

	//*endptr = found;
	//*found = '\0';
	return true;
}

static void SkipWhileWhitespace(char** text_pointer, int c) {
	if (isspace(c)) {
		while (isspace(**text_pointer) && isspace(*((*text_pointer) + 1))) {
			++(*text_pointer);
		}
	}
}

template<class T>
static T StringToInteger(char* text, char** endptr, int base) {
	//assert(false);
	return T();
}

template<>
int StringToInteger<int>(char* text, char** endptr, int base) {
	return strtol(text, endptr, base);
}

template<>
long long StringToInteger<long long>(char* text, char** endptr, int base) {
	return strtoll(text, endptr, base);
}

template<>
uint64_t StringToInteger<uint64_t>(char* text, char** endptr, int base) {
	return strtoull(text, endptr, base);
}

template<typename T>
static T StringToIntegerUntilChar(
	char* text, int base, int c, char** endptr_result) {
	T result;
	//CHECK_NE(endptr_result, NULL);
	//*endptr_result = NULL;

	//char* endptr_extract;
	//if (!ExtractUntilChar(text, c, &endptr_extract))
	//	return 0;

	//char* endptr_strto;
	//result = StringToInteger<T>(text, &endptr_strto, base);
	//*endptr_extract = c;

	//if (endptr_extract != endptr_strto)
	//	return 0;

	//*endptr_result = endptr_extract;
	//SkipWhileWhitespace(endptr_result, c);

	return result;
}

static char* CopyStringUntilChar(
	char* text, unsigned out_len, int c, char* out) {
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
	T* outptr, char* text, int base, int c, char** endptr) {
	*outptr = StringToIntegerUntilChar<T>(*endptr, base, c, endptr);
	if (*endptr == NULL || **endptr == '\0') return false;
	++(*endptr);
	return true;
}

static bool ParseProcMapsLine(char* text, uint64_t* start, uint64_t* end,
	char* flags, uint64_t* offset,
	int* major, int* minor, int64_t* inode,
	unsigned* filename_offset)
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

ProcMapsIterator::ProcMapsIterator(pid_t pid, Buffer* buffer) {
	Init(pid, buffer, false);
}

ProcMapsIterator::ProcMapsIterator(pid_t pid, Buffer* buffer,
	bool use_maps_backing) {
	Init(pid, buffer, use_maps_backing);
}

void ProcMapsIterator::Init(pid_t pid, Buffer* buffer, bool use_maps_backing)
{
	pid_ = pid;
	using_maps_backing_ = use_maps_backing;
	dynamic_buffer_ = NULL;
	if (!buffer) {
		buffer = dynamic_buffer_ = new Buffer;
	}
	else {
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
	if (use_maps_backing) {  // don't bother with clever "self" stuff in this case
		ConstructFilename("/proc/%d/maps_backing", pid, ibuf_, Buffer::kBufSize);
	}
	else if (pid == 0) {
		ConstructFilename("/proc/self/maps", 1, ibuf_, Buffer::kBufSize);
	}
	else {
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

bool ProcMapsIterator::Valid() const {
#if defined(PLATFORM_WINDOWS)
	return snapshot_ != INVALID_HANDLE_VALUE;
#elif defined(__MACH__)
	return 1;
#else
	return fd_ != -1;
#endif
}

bool ProcMapsIterator::Next(uint64_t* start, uint64_t* end, char** flags,
	uint64_t* offset, int64_t* inode, char** filename) {
	return NextExt(start, end, flags, offset, inode, filename, NULL, NULL,
		NULL, NULL, NULL);
}

bool ProcMapsIterator::NextExt(uint64_t* start, uint64_t* end, char** flags,
	uint64_t* offset, int64_t* inode, char** filename,
	uint64_t* file_mapping, uint64_t* file_pages,
	uint64_t* anon_mapping, uint64_t* anon_pages,
	dev_t* dev)
{
#ifdef _WIN32
	static char kDefaultPerms[5] = "r-xp";
	BOOL ok;
	if (module_.dwSize == 0) {  // only possible before first call
		module_.dwSize = sizeof(module_);
		ok = Module32First(snapshot_, &module_);
	}
	else {
		ok = Module32Next(snapshot_, &module_);
	}
	if (ok) {
		uint64_t base_addr = reinterpret_cast<DWORD_PTR>(module_.modBaseAddr);
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
	do {
		// Advance to the start of the next line
		stext_ = nextline_;

		// See if we have a complete line in the buffer already
		nextline_ = static_cast<char*>(memchr(stext_, '\n', etext_ - stext_));
		if (!nextline_) {
			// Shift/fill the buffer so we do have a line
			int count = etext_ - stext_;

			// Move the current text to the start of the buffer
			memmove(ibuf_, stext_, count);
			stext_ = ibuf_;
			etext_ = ibuf_ + count;

			int nread = 0;            // fill up buffer with text
			while (etext_ < ebuf_) {
				NO_INTR(nread = read(fd_, etext_, ebuf_ - etext_));
				if (nread > 0)
					etext_ += nread;
				else
					break;
			}

			// Zero out remaining characters in buffer at EOF to avoid returning
			// garbage from subsequent calls.
			if (etext_ != ebuf_ && nread == 0) {
				memset(etext_, 0, ebuf_ - etext_);
			}
			*etext_ = '\n';   // sentinel; safe because ibuf extends 1 char beyond ebuf
			nextline_ = static_cast<char*>(memchr(stext_, '\n', etext_ + 1 - stext_));
		}
		*nextline_ = 0;                // turn newline into nul
		nextline_ += ((nextline_ < etext_) ? 1 : 0);  // skip nul if not end of text
		// stext_ now points at a nul-terminated line
		uint64_t tmpstart, tmpend, tmpoffset;
		int64_t tmpinode;
		int major, minor;
		unsigned filename_offset = 0;
#if defined(__linux__)
		// for now, assume all linuxes have the same format
		if (!ParseProcMapsLine(
			stext_,
			start ? start : &tmpstart,
			end ? end : &tmpend,
			flags_,
			offset ? offset : &tmpoffset,
			&major, &minor,
			inode ? inode : &tmpinode, &filename_offset)) continue;
#endif
		size_t stext_length = strlen(stext_);
		if (filename_offset == 0 || filename_offset > stext_length)
			filename_offset = stext_length;

		// We found an entry
		if (flags) *flags = flags_;
		if (filename) *filename = stext_ + filename_offset;
		if (dev) *dev = minor | (major << 8);

		if (using_maps_backing_) {
			// Extract and parse physical page backing info.
			char* backing_ptr = stext_ + filename_offset +
				strlen(stext_ + filename_offset);

			// find the second '('
			int paren_count = 0;
			while (--backing_ptr > stext_) {
				if (*backing_ptr == '(') {
					++paren_count;
					if (paren_count >= 2) {
						uint64_t tmp_file_mapping;
						uint64_t tmp_file_pages;
						uint64_t tmp_anon_mapping;
						uint64_t tmp_anon_pages;

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
	uint64_t start, uint64_t end, const char* flags,
	uint64_t offset, int64_t inode,
	const char* filename, dev_t dev) {
	// We assume 'flags' looks like 'rwxp' or 'rwx'.
	char r = (flags && flags[0] == 'r') ? 'r' : '-';
	char w = (flags && flags[0] && flags[1] == 'w') ? 'w' : '-';
	char x = (flags && flags[0] && flags[1] && flags[2] == 'x') ? 'x' : '-';
	// p always seems set on linux, so we set the default to 'p', not '-'
	char p = (flags && flags[0] && flags[1] && flags[2] && flags[3] != 'p')
		? '-' : 'p';

	const int rc = snprintf(buffer, bufsize,
		"%08" "llx" "-%08" "llx" " %c%c%c%c %08" "llx" " %02x:%02x %-11" "llx" " %s\n",
		start, end, r, w, x, p, offset,
		static_cast<int>(dev / 256), static_cast<int>(dev % 256),
		inode, filename);
	return (rc < 0 || rc >= bufsize) ? 0 : rc;
}
