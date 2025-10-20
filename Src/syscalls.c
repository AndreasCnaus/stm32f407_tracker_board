#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/reent.h>
#include "uart.h"   // Your UART output function: __io_putchar

// Symbols from linker script
extern char end;           // End of static data / start of heap
extern char __heap_start__;
extern char __heap_end__;

static char *heap_ptr = NULL;

void __malloc_lock(struct _reent *r)   { (void)r; }
void __malloc_unlock(struct _reent *r) { (void)r; }

// =================== Heap ===================
// _sbrk_r is required for malloc/calloc/free in Newlib-nano.
// Provides the heap memory to dynamic allocation functions.
__attribute__((used))
caddr_t _sbrk_r(struct _reent *r, ptrdiff_t increment) {
    char *prev_heap;

    if (heap_ptr == NULL) {
        heap_ptr = &__heap_start__;  // initialize on first call
    }

    prev_heap = heap_ptr;

    if ((heap_ptr + increment) > &__heap_end__) {
        r->_errno = ENOMEM;
        return (caddr_t) -1;
    }

    heap_ptr += increment;
    return (caddr_t) prev_heap;
}

__attribute__((used))
caddr_t _sbrk(ptrdiff_t increment) {
    return _sbrk_r(_impure_ptr, increment);
}

// =================== Write ===================
// _write_r is used by printf and other output functions (stdout/stderr).
// This implementation is **blocking**: uart2_write() waits until
// the UART transmit register is ready for each character.
// Sends `len` bytes from `ptr` to UART.
__attribute__((used))
_ssize_t _write_r(struct _reent *r, int file, const void *ptr, size_t len)
{
    (void)r;
    (void)file;  // stdout/stderr typically

    const char *cptr = ptr;
    for (size_t i = 0; i < len; i++)
        uart2_write(*cptr++);  // Blocking: waits if TX register is not ready
    return len;  // Number of characters actually written
}

// =================== Read ===================
// _read_r is used by input functions (scanf, getchar, etc.).
// This implementation is **blocking** and waits for characters
// from UART one by one until either:
//   - a newline ('\n') or carriage return ('\r') is received, OR
//   - the buffer limit (len) is reached.
// Returns the number of characters read.
__attribute__((used))
_ssize_t _read_r(struct _reent *r, int file, char *ptr, size_t len)
{
    (void)r;
    (void)file;

    size_t i = 0;
    while ( i < len) {
       char ch = uart2_read();

       // Convert CR to LF
       if (ch == '\r') {
            ch = '\n';
        }

       ptr[i++] = ch;

        if (ch == '\n') {   // only need to check for LF now
            break;  // line based read
        }
    }

   return i;
}

// =================== File operations ===================
// Newlib expects these functions even on bare-metal systems.
// They are used internally by system calls like fopen/fclose, printf, etc.

// Close a file descriptor
// Used by fclose() and other functions that close a file handle.
// For bare-metal, no real files exist, so just return -1.
__attribute__((used))
int _close_r(struct _reent *r, int file)
{
    (void)r;
    (void)file;
    return -1;
}

// Get status information about a file
// Used by functions like fstat(), stat(), and file I/O functions to check file type.
// Here, we return S_IFCHR to indicate a character device (e.g., UART).
__attribute__((used))
int _fstat_r(struct _reent *r, int file, struct stat *st)
{
    (void)r;
    (void)file;
    st->st_mode = S_IFCHR; // Character device
    return 0;
}

// Check if a file descriptor refers to a terminal
// Used by isatty() to decide if output is a terminal device.
// Always return 1 for UART/console.
__attribute__((used))
int _isatty_r(struct _reent *r, int file)
{
    (void)r;
    (void)file;
    return 1;
}

// Set or get the position in a file
// Used by lseek() and functions that seek within a file (fseek, ftell, etc.).
// For bare-metal, just return 0.
__attribute__((used))
_off_t _lseek_r(struct _reent *r, int file, _off_t ptr, int dir)
{
    (void)r;
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}
