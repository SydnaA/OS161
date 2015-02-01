/* BEGIN A3 SETUP */
/*
 * Declarations for file handle and file table management.
 * New for A3.
 */

#ifndef _FILE_H_
#define _FILE_H_

#include <kern/limits.h>

struct fd{
	// open flag, passed by open so that we can check permission on
	// read and write
	int flag; 
	// Ref_count, mainly for dup2 and fork system call
	int ref_count;
	off_t offset;
	// lock for protect file descriptor
	struct lock* lock;
	//actual pointer to the file's vnode
	struct vnode* vn;
};


/*
 * filetable struct
 * just an array, nice and simple.  
 * It is up to you to design what goes into the array.  The current
 * array of ints is just intended to make the compiler happy.
 */
struct filetable {
	struct fd* t_fdtable[__OPEN_MAX]; /* dummy type */
	struct lock* t_fdlock; /*lock the filetable for synchronization*/
};

/* these all have an implicit arg of the curthread's filetable */
int filetable_init(void);
void filetable_destroy(struct filetable *ft);

/* opens a file (must be kernel pointers in the args) */
int file_open(char *filename, int flags, int mode, int *retfd);

/* closes a file */
int file_close(int fd);

/* A3: You should add additional functions that operate on
 * the filetable to help implement some of the filetable-related
 * system calls.
 */
/* copy the filetable to handle the fork */
int filetable_copy(struct filetable *oldft, struct filetable **newft);
#endif /* _FILE_H_ */

/* END A3 SETUP */
