/* BEGIN A3 SETUP */
/*
 * File handles and file tables.
 * New for ASST3
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <file.h>
#include <syscall.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <synch.h>
#include <current.h>

/*** openfile functions ***/

/*
 * file_open
 * opens a file, places it in the filetable, sets RETFD to the file
 * descriptor. the pointer arguments must be kernel pointers.
 * NOTE -- the passed in filename must be a mutable string.
 * 
 * A3: As per the OS/161 man page for open(), you do not need 
 * to do anything with the "mode" argument.
 */
int
file_open(char *filename, int flags, int mode, int *retfd)
{
	struct vnode* vn;
	struct lock* filelock;
	
	if(curthread->t_filetable == NULL) {
		return EBADF;
	}
	
	if(filename == NULL ) {
		return EINVAL;
	}
	
	vn = kmalloc(sizeof(struct vnode*));
	if (vn == NULL){
		return ENOMEM;
	}
	
	//vfs_open will handle the flag check, so we don't need to check 
	// inside file_open
	int err = vfs_open(filename, flags, mode, &vn);
	if (err){
		return err;// error code
	}

	struct fd *fd = kmalloc(sizeof (struct fd));
	if(fd == NULL) {
		return ENOMEM;
	}


	fd->vn = vn;
	fd->flag = flags;
	fd->offset = 0;
	fd->ref_count = 1;
	filelock = lock_create("filelock");
	if (filelock == NULL) {
		vfs_close(vn);
		kfree(fd);
		return ENOMEM;
	}
	fd->lock = filelock;
	lock_acquire(curthread->t_filetable->t_fdlock);
	int i;
	for(i=0;i<__OPEN_MAX;i++) {
		if(curthread->t_filetable->t_fdtable[i] == NULL) {
			// found the empty space, put everything in
			// and break out the loop
			curthread->t_filetable->t_fdtable[i] = fd;
			break;
		} else if(i == __OPEN_MAX-1) {
			lock_destroy(filelock);
			kfree(fd);
			vfs_close(vn);
			lock_release(curthread->t_filetable->t_fdlock);
			return EMFILE;
		}
	}
	*retfd = i;
	lock_release(curthread->t_filetable->t_fdlock);	
	return 0;
}


/* 
 * file_close
 * When system call fork, it will duplicate the filetable 
 * However, for parent's filetable it will update the ref counter
 * It means, a child process is running. 
 * When a process close
 */
int
file_close(int fd)
{
	if(fd < 0 || fd >= __OPEN_MAX) {
		return EBADF;
	}

	if(curthread->t_filetable == NULL) {
		return EBADF;
	}
	if(curthread->t_filetable->t_fdtable[fd] == NULL ) {
		return EBADF; // error
	}
	lock_acquire(curthread->t_filetable->t_fdlock);
	lock_acquire(curthread->t_filetable->t_fdtable[fd]->lock);
	
	curthread->t_filetable->t_fdtable[fd]->ref_count--;
	if(curthread->t_filetable->t_fdtable[fd]->ref_count <= 0) {
		vfs_close(curthread->t_filetable->t_fdtable[fd]->vn);
		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
		lock_destroy(curthread->t_filetable->t_fdtable[fd]->lock);
		curthread->t_filetable->t_fdtable[fd] = NULL;
		lock_release(curthread->t_filetable->t_fdlock);
		

	} else {
		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
		lock_release(curthread->t_filetable->t_fdlock);
		return ENOTEMPTY;
	}

	return 0;
}

/*** filetable functions ***/

/* 
 * filetable_init
 * pretty straightforward -- allocate the space, set up 
 * first 3 file descriptors for stdin, stdout and stderr,
 * and initialize all other entries to NULL.
 * 
 * Should set curthread->t_filetable to point to the
 * newly-initialized filetable.
 * 
 * Should return non-zero error code on failure.  Currently
 * does nothing but returns success so that loading a user
 * program will succeed even if you haven't written the
 * filetable initialization yet.
 */

int
filetable_init(void)
{
	if(curthread->t_filetable != NULL) {
		return EBADF;
	}

	curthread->t_filetable = kmalloc(sizeof(struct filetable));
	if (curthread->t_filetable == NULL) {
			return ENOMEM;
	}
	struct lock* ftlock; //lock for filetable
	ftlock = lock_create("Filetale lock");
	if (ftlock == NULL){
		kfree(curthread->t_filetable);
		return ENOMEM;
	}
	
	curthread->t_filetable->t_fdlock = ftlock;
	
	int i;
	lock_acquire(curthread->t_filetable->t_fdlock);
	for(i = 0;i<__OPEN_MAX;i++) {
		curthread->t_filetable->t_fdtable[i] = NULL;
	}
	lock_release(curthread->t_filetable->t_fdlock);
	
	int result;
	char path[5];
	strcpy(path, "con:");
	if((result = file_open(path, O_RDONLY, 0, &i))) {
		return result;
	}
	strcpy(path, "con:");
	if((result = file_open(path, O_WRONLY, 0, &i))) {
		return result;
	}
	strcpy(path, "con:");
	if((result = file_open(path, O_WRONLY, 0, &i))) {
		return result;
	}

	return 0;
}	

/*
 * filetable_destroy
 * closes the files in the file table, frees the table.
 * This should be called as part of cleaning up a process (after kill
 * or exit).
 */
void
filetable_destroy(struct filetable *ft)
{
	if(ft == NULL) {
		return;
	}
	int i;
	for(i = 0; i<__OPEN_MAX;i++) {
		if(ft->t_fdtable[i]) {
			file_close(i);
		}
	}
	lock_destroy(ft->t_fdlock);
	kfree(ft);
}	


/* 
 * You should add additional filetable utility functions here as needed
 * to support the system calls.  For example, given a file descriptor
 * you will want some sort of lookup function that will check if the fd is 
 * valid and return the associated vnode (and possibly other information like
 * the current file position) associated with that open file.
 */
 
 /*
  * This will handle fork semantics
  */
int
filetable_copy(struct filetable *oldft, struct filetable **newft)
{
	struct filetable *newfiletable;
	
	if (oldft == NULL) {
		return EINVAL;
	}

	newfiletable = kmalloc(sizeof(struct filetable));
		
	if (newfiletable == NULL){
		kfree(newfiletable);
		return ENOMEM;
	}
	
	/* Just make sure we have the right parent file table 
	 * before we proceed */
	KASSERT(oldft == curthread->t_filetable);
	
	lock_acquire(oldft->t_fdlock);
	//Since the content might be change, so we need to lock the each fd
	// on the oldft and newft
	for (int fd = 0; fd < __OPEN_MAX; fd ++){
		if (oldft->t_fdtable[fd]){
			//lock the old one
			lock_acquire(oldft->t_fdtable[fd]->lock);
			oldft->t_fdtable[fd]->ref_count++;
		}
		//actually copy
		newfiletable->t_fdtable[fd] = oldft->t_fdtable[fd];
		//Since newfiletable is pointing to parent, need to increase the ref counter for parent
		//We done the copy, release the old one and proceed next fd
		if (oldft->t_fdtable[fd]){
			lock_release(oldft->t_fdtable[fd]->lock);
		}
	}
	lock_release(oldft->t_fdlock);
	
	*newft = newfiletable;
	return 0;
}

/* END A3 SETUP */
