#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <sys/reent.h>
#include <sys/types.h>  // for _ssize_t

_ssize_t _write_r(struct _reent *r, int file, const void *ptr, size_t len);
_ssize_t _read_r(struct _reent *r, int file, void *ptr, size_t len);

#endif
