#ifndef _SYSINFO_H_
#define _SYSINFO_H_

#include <time.h>
#if (defined(_WIN32) || defined(__MINGW32__)) && (!defined(__CYGWIN__) && !defined(__CYGWIN32__))
#include <windows.h>   // for DWORD
#include <tlhelp32.h>  // for CreateToolhelp32Snapshot
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>    // for pid_t
#endif
#include <stddef.h>    // for size_t
#include <limits.h>    // for PATH_MAX

#ifndef HAVE_PID_T
typedef int pid_t;
#endif

typedef unsigned char            uint8;
typedef unsigned short           uint16;
typedef unsigned int           uint32;
typedef unsigned long long		uint64;
typedef long long		int64;

#ifndef _DEV_T_DEFINED
    #define _DEV_T_DEFINED

    typedef unsigned int _dev_t; // device code

    #if (defined _CRT_DECLARE_NONSTDC_NAMES && _CRT_DECLARE_NONSTDC_NAMES) || (!defined _CRT_DECLARE_NONSTDC_NAMES && !__STDC__)
        typedef _dev_t dev_t;
    #endif
#endif
#ifndef _DEV_T_DEFINED
    #define _DEV_T_DEFINED

    typedef unsigned int _dev_t; // device code

    #if (defined _CRT_DECLARE_NONSTDC_NAMES && _CRT_DECLARE_NONSTDC_NAMES) || (!defined _CRT_DECLARE_NONSTDC_NAMES && !__STDC__)
        typedef _dev_t dev_t;
    #endif
#endif

extern const char* GetenvBeforeMain(const char* name);

extern bool GetUniquePathFromEnv(const char* env_name, char* path);

extern int GetSystemCPUsCount();

void SleepForMilliseconds(int milliseconds);

bool HasPosixThreads();

#ifndef SWIG  // SWIG doesn't like struct Buffer and variable arguments.

class ProcMapsIterator {
 public:
  struct Buffer {
#ifdef __FreeBSD__
    // FreeBSD requires us to read all of the maps file at once, so
    // we have to make a buffer that's "always" big enough
    static const size_t kBufSize = 102400;
#else   // a one-line buffer is good enough
    static const size_t kBufSize = 256 + 1024;
#endif
    char buf_[kBufSize];
  };

  explicit ProcMapsIterator(pid_t pid);

  ProcMapsIterator(pid_t pid, Buffer *buffer);

  ProcMapsIterator(pid_t pid, Buffer *buffer,
                   bool use_maps_backing);

  bool Valid() const;

  const char *CurrentLine() const { return stext_; }

  static int FormatLine(char* buffer, int bufsize,
                        uint64 start, uint64 end, const char *flags,
                        uint64 offset, int64 inode, const char *filename,
                        dev_t dev);

  bool Next(uint64 *start, uint64 *end, char **flags,
            uint64 *offset, int64 *inode, char **filename);

  bool NextExt(uint64 *start, uint64 *end, char **flags,
               uint64 *offset, int64 *inode, char **filename,
               uint64 *file_mapping, uint64 *file_pages,
               uint64 *anon_mapping, uint64 *anon_pages,
               dev_t *dev);

  ~ProcMapsIterator();

 private:
  void Init(pid_t pid, Buffer *buffer, bool use_maps_backing);

  char *ibuf_;        // input buffer
  char *stext_;       // start of text
  char *etext_;       // end of text
  char *nextline_;    // start of next line
  char *ebuf_;        // end of buffer (1 char for a nul)
#if (defined(_WIN32) || defined(__MINGW32__)) && (!defined(__CYGWIN__) && !defined(__CYGWIN32__))
  HANDLE snapshot_;   // filehandle on dll info
  // In a change from the usual W-A pattern, there is no A variant of
  // MODULEENTRY32.  Tlhelp32.h #defines the W variant, but not the A.
  // We want the original A variants, and this #undef is the only
  // way I see to get them.  Redefining it when we're done prevents us
  // from affecting other .cc files.
# ifdef MODULEENTRY32  // Alias of W
#   undef MODULEENTRY32
  MODULEENTRY32 module_;   // info about current dll (and dll iterator)
#   define MODULEENTRY32 MODULEENTRY32W
# else  // It's the ascii, the one we want.
  MODULEENTRY32 module_;   // info about current dll (and dll iterator)
# endif
#elif defined(__MACH__)
  int current_image_; // dll's are called "images" in macos parlance
  int current_load_cmd_;   // the segment of this dll we're examining
#elif defined(__sun__)     // Solaris
  int fd_;
  char current_filename_[PATH_MAX];
#else
  int fd_;            // filehandle on /proc/*/maps
#endif
  pid_t pid_;
  char flags_[10];
  Buffer* dynamic_buffer_;  // dynamically-allocated Buffer
  bool using_maps_backing_; // true if we are looking at maps_backing instead of maps.
};

#endif  /* #ifndef SWIG */

// Helper routines

#ifdef _WIN32
#include <windows.h>
typedef HANDLE RawFD;
const RawFD kIllegalRawFD = INVALID_HANDLE_VALUE;
#else
typedef int RawFD;
const RawFD kIllegalRawFD = -1;   // what open returns if it fails
#endif  // defined(_WIN32)

namespace tcmalloc {
int FillProcSelfMaps(char buf[], int size, bool* wrote_all);
void DumpProcSelfMaps(RawFD fd);
}

#endif   /* #ifndef _SYSINFO_H_ */
