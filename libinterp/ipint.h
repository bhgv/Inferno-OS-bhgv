typedef struct IPint IPint;

#pragma incomplete IPint

/* Takes ownership of the magnitude on success and VM allocation failure. */
void*	newIPint(mpint*);
/* Validate the VM object and borrow its non-nil native magnitude. */
mpint*	checkIPint(void*);
void	freeIPint(Heap*, int);
void	ipintsmodinit(void);

extern	Type*	TIPint;
