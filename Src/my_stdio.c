
#include "my_stdio.h"
#include "syscalls.h"

#include <string.h>

// reent and stdout objects
extern struct _reent *_impure_ptr;
struct _reent my_reent;

// stdout buffer
#define STDOUT_BUFFER_SIZE  128
#define STDIN_BUFFER_SIZE   32
static char stdout_buf[STDOUT_BUFFER_SIZE] = {0};
static char stdin_buf[STDIN_BUFFER_SIZE] = {0};

int my_stdout_write_r(struct _reent *r, void *file_obj, const char *buf, int len);
int my_stdin_read_r(struct _reent *r, void *file_obj, char *buf, int len);

void stdio_init(void) {
    _impure_ptr = &my_reent;
    _REENT_INIT_PTR(_impure_ptr);   // initialize the reent struct

    // stdin
    FILE *my_stdin_ptr = _impure_ptr->_stdin; 
    my_stdin_ptr->_flags = __SRD;
    my_stdin_ptr->_file  = 0;
    my_stdin_ptr->_read  = my_stdin_read_r;
    my_stdin_ptr->_cookie = my_stdin_ptr;

     // attach stdout buffer
    my_stdin_ptr->_bf._base = (unsigned char *)stdin_buf;
    my_stdin_ptr->_bf._size = sizeof(stdin_buf);
    my_stdin_ptr->_p        = my_stdin_ptr->_bf._base;  // current position
    my_stdin_ptr->_w        = 0;                        // initially no chars in the buffer 
    my_stdin_ptr->_lbfsize  = 0;                        // only for output streams

    // stdout
    FILE *my_stdout_ptr = _impure_ptr->_stdout;
    my_stdout_ptr->_flags = __SWR | __SLBF;
    my_stdout_ptr->_file  = 1;
    my_stdout_ptr->_write = my_stdout_write_r;
    my_stdout_ptr->_cookie = my_stdout_ptr;

    // attach stdout buffer
    my_stdout_ptr->_bf._base = (unsigned char *)stdout_buf;
    my_stdout_ptr->_bf._size = sizeof(stdout_buf);
    my_stdout_ptr->_p        = my_stdout_ptr->_bf._base; // current position
    my_stdout_ptr->_w        = my_stdout_ptr->_bf._size; // remaining space
    my_stdout_ptr->_lbfsize  = sizeof(stdout_buf);       // line-buffered size

    // stderr
    FILE *my_stderr_ptr = _impure_ptr->_stderr;
    my_stderr_ptr->_flags = __SWR | __SNBF;
    my_stderr_ptr->_file  = 2;
    my_stderr_ptr->_write = my_stdout_write_r; // can share with stdout
    my_stderr_ptr->_cookie = my_stderr_ptr;
}

// wrapper to match FILE->_write signature
int my_stdout_write_r(struct _reent *r, void *file_obj, const char *buf, int len)
{
    return _write_r(r, ((FILE *)file_obj)->_file, buf, len);
}

int my_stdin_read_r(struct _reent *r, void *file_obj, char *buf, int len)
{
    return _read_r(r, ((FILE *)file_obj)->_file, buf, len);
}

void float_to_str(char *buf, float val) {
    int sign = (val < 0) ? -1 : 1;
    val = val * sign;                     // make val positive
    int int_part = (int)val;
    int frac_part = (int)((val - int_part) * 10000 + 0.5f);  // 4 decimal places, rounded

    if (sign < 0) {
        sprintf(buf, "-%d.%04d", int_part, frac_part);
    } else {
        sprintf(buf, "%d.%04d", int_part, frac_part);
    }
}
