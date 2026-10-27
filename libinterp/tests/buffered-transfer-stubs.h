

/* The accepted buffered path performs no scheduler lookup. */
Prog*
currun(void)
{
	abort();
	return nil;
}


/* A complete native transfer cannot block its program. */
Prog*
delrun(int state)
{
	(void)state;
	abort();
	return nil;
}


/* The accepted path cannot allocate a sender or receiver queue node. */
void
cqadd(Progq **queue, Prog *program)
{
	(void)queue;
	(void)program;
	abort();
}


/* Queued peers always decline in the native geometry helper. */
void
cqdel(Progq **queue)
{
	(void)queue;
	abort();
}


/* ALT selection is handled by the existing interpreter path. */
void
altdone(Alt *selection, Prog *program, Channel *channel, int sending)
{
	(void)selection;
	(void)program;
	(void)channel;
	(void)sending;
	abort();
}


/* A buffered transfer with no peers never wakes another program. */
void
addrun(Prog *program)
{
	(void)program;
	abort();
}


/* Pointer-free buffers never need pointer-slot reinitialization. */
void
initmem(Type *type, void *value)
{
	(void)type;
	(void)value;
	abort();
}


/* Consistent ring metadata never produces a queue diagnostic. */
int
print(char *format, ...)
{
	(void)format;
	abort();
	return 0;
}
