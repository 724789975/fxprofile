#include "profiledata.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <time.h>
#ifdef _WIN32
#else
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#endif //!_WIN32
#include <string.h>

#ifdef _WIN32
typedef intptr_t ssize_t;
#endif

// All of these are initialized in profiledata.h.
const int ProfileData::kMaxStackDepth;
const int ProfileData::kAssociativity;
const int ProfileData::kBuckets;
const int ProfileData::kBufferLength;

ProfileData::Options::Options()
	: frequency_(1)
{
}

// This function is safe to call from asynchronous signals (but is not
// re-entrant).  However, that's not part of its public interface.
void ProfileData::Evict(const Entry& entry)
{
	const int d = entry.depth;
	const int nslots = d + 2; // Number of slots needed in eviction buffer
	if (num_evicted_ + nslots > kBufferLength)
	{
		FlushEvicted();
		assert(num_evicted_ == 0);
		assert(nslots <= kBufferLength);
	}
	evict_[num_evicted_++] = entry.count;
	evict_[num_evicted_++] = d;
	memcpy(&evict_[num_evicted_], entry.stack, d * sizeof(uintptr_t));
	num_evicted_ += d;
}

ProfileData::ProfileData()
	: hash_(0)
	, evict_(0)
	, num_evicted_(0)
	, out_(0)
	, count_(0)
	, evictions_(0)
	, total_bytes_(0)
	, fname_(0)
	, start_time_(0)
{
}

bool ProfileData::Start(const char* fname,
	int frequency)
{
	if (enabled()) {
		return false;
	}

	FILE* fd = fopen(fname, "w");
	if (!fd) {
		return false;
	}

	start_time_ = time(NULL);
#ifdef _WIN32
	fname_ = _strdup(fname);
#else
	fname_ = strdup(fname);
#endif // _WIN32

	num_evicted_ = 0;
	count_ = 0;
	evictions_ = 0;
	total_bytes_ = 0;

	hash_ = new Bucket[kBuckets];
	evict_ = new uintptr_t[kBufferLength];
	memset(hash_, 0, sizeof(hash_[0]) * kBuckets);

	// Record special entries
	evict_[num_evicted_++] = 0;                     // count for header
	evict_[num_evicted_++] = 3;                     // depth for header
	evict_[num_evicted_++] = 0;                     // Version number
	int period = 1000000 / frequency;
	evict_[num_evicted_++] = period;                // Period (microseconds)
	evict_[num_evicted_++] = 0;                     // Padding

	out_ = fd;

	return true;
}

ProfileData::~ProfileData()
{
	Stop();
}

// Dump /proc/maps data to fd.  Copied from heap-profile-table.cc.
#define NO_INTR(fn) \
	do              \
	{               \
	} while ((fn) < 0 && errno == EINTR)

static void FDWrite(FILE* fd, const char* buf, size_t len)
{
	while (len > 0) {
		ssize_t r;
		NO_INTR(r = fwrite(buf, 1, len, fd));
		buf += r;
		len -= r;
	}
}

static void DumpProcSelfMaps(FILE* fd)
{
	//   ProcMapsIterator::Buffer iterbuf;
	//   ProcMapsIterator it(0, &iterbuf);   // 0 means "current pid"

	//   uint64 start, end, offset;
	//   int64 inode;
	//   char *flags, *filename;
	//   ProcMapsIterator::Buffer linebuf;
	//   while (it.Next(&start, &end, &flags, &offset, &inode, &filename)) {
	//     int written = it.FormatLine(linebuf.buf_, sizeof(linebuf.buf_),
	//                                 start, end, flags, offset, inode, filename,
	//                                 0);
	//     FDWrite(fd, linebuf.buf_, written);
	//   }
}

void ProfileData::Stop()
{
	if (!enabled())
	{
		return;
	}

	// Move data from hash table to eviction buffer
	for (int b = 0; b < kBuckets; b++)
	{
		Bucket* bucket = &hash_[b];
		for (int a = 0; a < kAssociativity; a++)
		{
			if (bucket->entry[a].count > 0)
			{
				Evict(bucket->entry[a]);
			}
		}
	}

	if (num_evicted_ + 3 > kBufferLength)
	{
		// Ensure there is enough room for end of data marker
		FlushEvicted();
	}

	// Write end of data marker
	evict_[num_evicted_++] = 0; // count
	evict_[num_evicted_++] = 1; // depth
	evict_[num_evicted_++] = 0; // end of data marker
	FlushEvicted();

	// Dump "/proc/self/maps" so we get list of mapped shared libraries
	DumpProcSelfMaps(out_);

	Reset();
}

void ProfileData::Reset()
{
	if (!enabled())
	{
		return;
	}

	// close(out_);
	// delete[] hash_;
	// hash_ = 0;
	// delete[] evict_;
	// evict_ = 0;
	// num_evicted_ = 0;
	// free(fname_);
	// fname_ = 0;
	// start_time_ = 0;

	// out_ = -1;
}

// This function is safe to call from asynchronous signals (but is not
// re-entrant).  However, that's not part of its public interface.
void ProfileData::GetCurrentState(State* state) const
{
	if (enabled())
	{
		state->enabled = true;
		state->start_time = start_time_;
		state->samples_gathered = count_;
		int buf_size = sizeof(state->profile_name);
		strncpy(state->profile_name, fname_, buf_size);
		state->profile_name[buf_size - 1] = '\0';
	}
	else
	{
		state->enabled = false;
		state->start_time = 0;
		state->samples_gathered = 0;
		state->profile_name[0] = '\0';
	}
}

// This function is safe to call from asynchronous signals (but is not
// re-entrant).  However, that's not part of its public interface.
void ProfileData::FlushTable()
{
	if (!enabled())
	{
		return;
	}

	// Move data from hash table to eviction buffer
	for (int b = 0; b < kBuckets; b++)
	{
		Bucket* bucket = &hash_[b];
		for (int a = 0; a < kAssociativity; a++)
		{
			if (bucket->entry[a].count > 0)
			{
				Evict(bucket->entry[a]);
				bucket->entry[a].depth = 0;
				bucket->entry[a].count = 0;
			}
		}
	}

	// Write out all pending data
	FlushEvicted();
}

void ProfileData::Add(int depth, const void* const* stack)
{
	if (!enabled())
	{
		return;
	}

	if (depth > kMaxStackDepth)
		depth = kMaxStackDepth;

	// Make hash-value
	uintptr_t h = 0;
	for (int i = 0; i < depth; i++)
	{
		uintptr_t slot = reinterpret_cast<uintptr_t>(stack[i]);
		h = (h << 8) | (h >> (8 * (sizeof(h) - 1)));
		h += (slot * 31) + (slot * 7) + (slot * 3);
	}

	count_++;

	// See if table already has an entry for this trace
	bool done = false;
	Bucket* bucket = &hash_[h % kBuckets];
	for (int a = 0; a < kAssociativity; a++)
	{
		Entry* e = &bucket->entry[a];
		if (e->depth == depth)
		{
			bool match = true;
			for (int i = 0; i < depth; i++)
			{
				if (e->stack[i] != reinterpret_cast<uintptr_t>(stack[i]))
				{
					match = false;
					break;
				}
			}
			if (match)
			{
				e->count++;
				done = true;
				break;
			}
		}
	}

	if (!done)
	{
		// Evict entry with smallest count
		Entry* e = &bucket->entry[0];
		for (int a = 1; a < kAssociativity; a++)
		{
			if (bucket->entry[a].count < e->count)
			{
				e = &bucket->entry[a];
			}
		}
		if (e->count > 0)
		{
			evictions_++;
			Evict(*e);
		}

		// Use the newly evicted entry
		e->depth = depth;
		e->count = 1;
		for (int i = 0; i < depth; i++)
		{
			e->stack[i] = reinterpret_cast<uintptr_t>(stack[i]);
		}
	}
}

// This function is safe to call from asynchronous signals (but is not
// re-entrant).  However, that's not part of its public interface.
void ProfileData::FlushEvicted()
{
	if (num_evicted_ > 0)
	{
		const char* buf = reinterpret_cast<char*>(evict_);
		size_t bytes = sizeof(evict_[0]) * num_evicted_;
		total_bytes_ += bytes;
		FDWrite(out_, buf, bytes);
	}
	num_evicted_ = 0;
}
