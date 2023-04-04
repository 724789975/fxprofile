
#ifndef BASE_PROFILEDATA_H_
#define BASE_PROFILEDATA_H_

#include <time.h>   // for time_t
#include <stdint.h>
#include <stdio.h>

class ProfileData {
public:
	struct State {
		bool     enabled;             // Is profiling currently enabled?
		time_t   start_time;          // If enabled, when was profiling started?
		char     profile_name[1024];  // Name of file being written, or '\0'
		int      samples_gathered;    // Number of samples gathered to far (or 0)
	};

	class Options {
	public:
		Options();

		// Get and set the sample frequency.
		int frequency() const {
			return frequency_;
		}
		void set_frequency(int frequency) {
			frequency_ = frequency;
		}

	private:
		int      frequency_;                  // Sample frequency.
	};

	static const int kMaxStackDepth = 64;  // Max stack depth stored in profile

	ProfileData();
	~ProfileData();

	bool Start(const char* fname, const Options& options);

	void Stop();

	void Reset();

	void Add(int depth, const void* const* stack);

	void FlushTable();

	bool enabled() const { return out_ >= 0; }

	void GetCurrentState(State* state) const;

private:
	static const int kAssociativity = 4;          // For hashtable
	static const int kBuckets = 1 << 10;          // For hashtable
	static const int kBufferLength = 1 << 18;     // For eviction buffer

	// Type of slots: each slot can be either a count, or a PC value
	typedef uintptr_t Slot;

	// Hash-table/eviction-buffer entry (a.k.a. a sample)
	struct Entry {
		int count;                  // Number of hits
		int depth;                  // Stack depth
		int stack[kMaxStackDepth];  // Stack contents
	};

	// Hash table bucket
	struct Bucket {
		Entry entry[kAssociativity];
	};

	Bucket* hash_;          // hash table
	Slot* evict_;         // evicted entries
	int           num_evicted_;   // how many evicted entries?
	FILE*           out_;           // fd for output file.
	int           count_;         // How many samples recorded
	int           evictions_;     // How many evictions
	size_t        total_bytes_;   // How much output

	//TODO ÐÞ¸ÄÃû×ÖÎª×Ö·û´®
	char* fname_;         // Profile file name
	time_t        start_time_;    // Start time, or 0

	// Move 'entry' to the eviction buffer.
	void Evict(const Entry& entry);

	// Write contents of eviction buffer to disk.
	void FlushEvicted();

	ProfileData(const ProfileData&);
	void operator=(const ProfileData&);
};

#endif  // BASE_PROFILEDATA_H_
