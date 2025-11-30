#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

extern int errno;
register char * stack_ptr asm("sp");
extern uint32_t serial_write(const uint8_t *buf, uint32_t len);

caddr_t _sbrk(int incr)
{
	extern char end asm("end");
	static char *heap_end;
	char *prev_heap_end;

	if (heap_end == 0)
		heap_end = &end;

	prev_heap_end = heap_end;
	if (heap_end + incr > stack_ptr)
	{
		errno = ENOMEM;
		return (caddr_t) -1;
	}

	heap_end += incr;

	return (caddr_t) prev_heap_end;
}

__attribute__((weak)) int _read(int file, char *ptr, int len)
{
    return len;
}

__attribute__((weak)) int _write(int file, char *ptr, int len)
{
    if(file == 1){
        return serial_write((uint8_t *)ptr, len);
    }
	return 0;
}

int _close(int file) { return -1; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _fstat(int file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { return 1; }
