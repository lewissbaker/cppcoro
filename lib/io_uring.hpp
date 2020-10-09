///////////////////////////////////////////////////////////////////////////////
// Copyright (c) GIG <bigig@live.ru>
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef IO_URING_HPP_INCLUDED
#define IO_URING_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if !CPPCORO_OS_LINUX
# error
#endif

#include <linux/io_uring.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline int io_uring_setup(unsigned entries, struct io_uring_params* p)
{
	return syscall(__NR_io_uring_setup, entries, p);
}

static inline int io_uring_enter(int fd, unsigned to_submit,
	unsigned min_complete, unsigned flags, sigset_t* sig)
{
	return syscall(__NR_io_uring_enter, fd, to_submit, min_complete,
		flags, sig, _NSIG / 8);
}

static inline int io_uring_register(int fd, unsigned opcode, const void* arg,
	unsigned nr_args)
{
	return syscall(__NR_io_uring_register, fd, opcode, arg, nr_args);
}

#endif
