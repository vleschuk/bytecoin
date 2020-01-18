/* vim: set tabstop=4 shiftwidth=4 noexpandtab : */
#pragma once
#include <iostream>
#include <iomanip>
#include <mutex>
#include <thread>

#define BC_CREATE_LOCK(var, mtx, id) \
	common::Lock var(mtx, id, __PRETTY_FUNCTION__)

namespace common {

extern size_t nthreads;
extern bool debug_locks;

template<typename Mutex>
class LockGuard {
	static constexpr const char *unknown_id = "<unspecified>";
	static constexpr const char *unknown_location = "<unspecified>";
public:
	LockGuard(Mutex &mtx, const char *id = unknown_id,
			  const char *where = unknown_location)
		: mtx_(mtx)
		, id_(id)
		, where_(where)	{
		if (nthreads > 1) {
			if (debug_locks)
				std::cerr << "Thread " << std::this_thread::get_id()
					<< " locking " << id_ << "(" << std::hex << (void *)&mtx_
					<< ") in " << where_ << std::endl;
			mtx_.lock();
			if (debug_locks)
				std::cerr << "Thread " << std::this_thread::get_id()
					<< " locked " << id_ << "(" << std::hex << (void *)&mtx_
					<< ") in " << where_ << std::endl;
		}
	}
	~LockGuard() {
		if (nthreads > 1) {
			mtx_.unlock();
			if (debug_locks)
				std::cerr << "Thread " << std::this_thread::get_id()
					<< " unlocked " << id_ << "(" << std::hex << (void *)&mtx_
					<< ") in " << where_ << std::endl;
		}
	}
private:
	Mutex &mtx_;
	const char *id_;
	const char *where_;
};

typedef std::recursive_mutex Mutex;
typedef LockGuard<Mutex> Lock;

} // namespace common
