#include	"dat.h"
#include	"fns.h"

/*
 * General OS interface to errors
 */
void
kwerrstr(char *fmt, ...)
{
	va_list arg;
	char buf[ERRMAX];

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	kstrcpy(up->env->errstr, buf, ERRMAX);
}

void
kerrstr(char *err, uint size)
{
	char tmp[ERRMAX];

	kstrcpy(tmp, up->env->errstr, sizeof(tmp));
	kstrcpy(up->env->errstr, err, ERRMAX);
	kstrcpy(err, tmp, size);
}


/* Read the current error without consuming output bytes or changing the process state. */
void
kgerrstr(char *err, uint size)
{
	char *source;

	source = "<no-up>";
	if(up != nil)
		source = up->env->errstr;
	if(size > ERRMAX)
		size = ERRMAX;
	kstrcpy(err, source, size);
}
