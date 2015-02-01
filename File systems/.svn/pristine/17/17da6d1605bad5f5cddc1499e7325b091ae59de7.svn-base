/* BEGIN A3 SETUP */
/* This file existed for A1 and A2, but has been completely replaced for A3.
 * We have kept the dumb versions of sys_read and sys_write to support early
 * testing, but they should be replaced with proper implementations that 
 * use your open file table to find the correct vnode given a file descriptor
 * number.  All the "dumb console I/O" code should be deleted.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <copyinout.h>
#include <synch.h>
#include <file.h>
#include <kern/seek.h>

/* This special-case global variable for the console vnode should be deleted 
 * when you have a proper open file table implementation.
 */
struct vnode *cons_vnode=NULL; 

/* This function should be deleted, including the call in main.c, when you
 * have proper initialization of the first 3 file descriptors in your 
 * open file table implementation.
 * You may find it useful as an example of how to get a vnode for the 
 * console device.
 */
void dumb_consoleIO_bootstrap()
{
  int result;
  char path[5];

  /* The path passed to vfs_open must be mutable.
   * vfs_open may modify it.
   */

  strcpy(path, "con:");
  result = vfs_open(path, O_RDWR, 0, &cons_vnode);

  if (result) {
    /* Tough one... if there's no console, there's not
     * much point printing a warning...
     * but maybe the bootstrap was just called in the wrong place
     */
    kprintf("Warning: could not initialize console vnode\n");
    kprintf("User programs will not be able to read/write\n");
    cons_vnode = NULL;
  }
}

/*
 * mk_useruio
 * sets up the uio for a USERSPACE transfer. 
 */
static
void
mk_useruio(struct iovec *iov, struct uio *u, userptr_t buf, 
	   size_t len, off_t offset, enum uio_rw rw)
{

	iov->iov_ubase = buf;
	iov->iov_len = len;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = offset;
	u->uio_resid = len;
	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}

/*
 * sys_open
 * just copies in the filename, then passes work to file_open.
 * You have to write file_open.
 * 
 */
int
sys_open(userptr_t filename, int flags, int mode, int *retval)
{
	char *fname;
	int result;

	if ( (fname = (char *)kmalloc(__PATH_MAX)) == NULL) {
		return ENOMEM;
	}

	result = copyinstr(filename, fname, __PATH_MAX, NULL);
	if (result) {
		kfree(fname);
		return result;
	}

	result =  file_open(fname, flags, mode, retval);
	if (result){
		return result;
	}
	kfree(fname);
	return result;
}

/* 
 * sys_close
 * Simply call file_close on the fd
 */
int
sys_close(int fd)
{
	return file_close(fd);
}

/* 
 * sys_dup2
 * Clone the oldfd onto newfd
 */
int
sys_dup2(int oldfd, int newfd, int *retval)
{
	// Checking valid oldfd and newfd
	if(oldfd < 0 || oldfd >= __OPEN_MAX
			|| newfd < 0 || newfd >= __OPEN_MAX) {
		return EBADF;
	}
	
	// Checking is it same fd for both old one and new one
	if(oldfd == newfd) {
		*retval = oldfd;
		return 0;
	}
	
	// Does the oldfd already got close or even exists?
	if (curthread->t_filetable->t_fdtable[oldfd] == NULL){
		return EBADF;
	}
	
	// Does someone open the newfd?
	if(curthread->t_filetable->t_fdtable[newfd] != NULL) {
		file_close(newfd);
	}
	
	// Cloning...
	lock_acquire(curthread->t_filetable->t_fdlock);
	lock_acquire(curthread->t_filetable->t_fdtable[oldfd]->lock);
	curthread->t_filetable->t_fdtable[newfd] = curthread->t_filetable->t_fdtable[oldfd];
	lock_release(curthread->t_filetable->t_fdtable[oldfd]->lock);
	
	lock_acquire(curthread->t_filetable->t_fdtable[newfd]->lock);
	// Since newfd is the copy of oldfd, we need to increment ref_count in newfd
	curthread->t_filetable->t_fdtable[newfd]->ref_count++;
	lock_release(curthread->t_filetable->t_fdtable[newfd]->lock);
	lock_release(curthread->t_filetable->t_fdlock);
	*retval = newfd;
	return 0;
}

/*
 * sys_read
 * calls VOP_READ.
 * Find out the file struct by providing fd in current filetable, 
 *  and make the uio, and called VOP_READ, passing the vnode and uio
 * From man page: read reads up to buflen bytes from the file specified 
 * by fd, at the location in the file specified by the current seek 
 * position of the file, and stores them in the space pointed to by buf. 
 * The file must be open for reading. 
 * 
 * 
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
	struct uio user_uio;
	struct iovec user_iov;
	int result;
	
	if(fd < 0 || fd >= __OPEN_MAX) {
		return EBADF;
	}

	if (curthread->t_filetable == NULL
			|| curthread->t_filetable->t_fdtable[fd] == NULL) {
		return EBADF;
	}
	
	lock_acquire(curthread->t_filetable->t_fdlock);
	lock_acquire(curthread->t_filetable->t_fdtable[fd]->lock);

	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&user_iov, &user_uio, buf, size, curthread->t_filetable->t_fdtable[fd]->offset, UIO_READ);

	/* does the read */
	result = VOP_READ(curthread->t_filetable->t_fdtable[fd]->vn, &user_uio);
	if (result) {
		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
		lock_release(curthread->t_filetable->t_fdlock);
		return result;
	}

	curthread->t_filetable->t_fdtable[fd]->offset += size;
	lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
	lock_release(curthread->t_filetable->t_fdlock);

	/*
	 * The amount read is the size of the buffer originally, minus
	 * how much is left in it.
	 */
	*retval = size - user_uio.uio_resid;


	return 0;
}

/*
 * sys_write
 * calls VOP_WRITE.
 * Almost same as sys_read, except we using VOP_WRITE
 */

int
sys_write(int fd, userptr_t buf, size_t len, int *retval) 
{
        struct uio user_uio;
        struct iovec user_iov;
        int result;

        if(fd < 0 || fd >= __OPEN_MAX) {
        	return EBADF;
        }

        if (curthread->t_filetable == NULL
        	|| curthread->t_filetable->t_fdtable[fd] == NULL) {
        	return EBADF;
        }
		lock_acquire(curthread->t_filetable->t_fdlock);
        lock_acquire(curthread->t_filetable->t_fdtable[fd]->lock);

        /* set up a uio with the buffer, its size, and the current offset */
        mk_useruio(&user_iov, &user_uio, buf, len, curthread->t_filetable->t_fdtable[fd]->offset, UIO_WRITE);

        /* does the write */
        result = VOP_WRITE(curthread->t_filetable->t_fdtable[fd]->vn, &user_uio);
        if (result) {
        	lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
        	lock_release(curthread->t_filetable->t_fdlock);
            return result;
        }

        curthread->t_filetable->t_fdtable[fd]->offset += len;

		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
        lock_release(curthread->t_filetable->t_fdlock);
        /*
         * the amount written is the size of the buffer originally,
         * minus how much is left in it.
         */
        *retval = len - user_uio.uio_resid;


        return 0;
}

/*
 * sys_lseek
 * Call VOP_TRYSEEK
 * If whence is SEEK_END, call VOP_STAT to get file information, and 
 * set position to st_size + offset
 * Once it finish VOP_TRYSEEK, we need to update the offset for the file
 * to newest position.
 * 
 */
int
sys_lseek(int fd, off_t offset, int whence, off_t *retval)
{
	
	if(fd < 0 || fd >= __OPEN_MAX) {
		return EBADF;
	}
        
	if(curthread->t_filetable == NULL || 
		curthread->t_filetable->t_fdtable[fd] == NULL ) {
		return EBADF;
	}
	struct stat st;
	off_t position;
	int result, err;
	lock_acquire(curthread->t_filetable->t_fdlock);
	lock_acquire(curthread->t_filetable->t_fdtable[fd]->lock);
	switch(whence) {
	case SEEK_SET:
		position = offset;
		break;
	case SEEK_CUR:
		position = curthread->t_filetable->t_fdtable[fd]->offset + offset;
		break;
	case SEEK_END:
		err = VOP_STAT(curthread->t_filetable->t_fdtable[fd]->vn, &st);
		if(err) {
			lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
			lock_release(curthread->t_filetable->t_fdlock);
			return err;
		}
		position = st.st_size + offset;
		break;
	default:
		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
		lock_release(curthread->t_filetable->t_fdlock);
		return EINVAL;
	}

	if(position < 0) {
		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
		lock_release(curthread->t_filetable->t_fdlock);
		return EINVAL;
	}
	result = VOP_TRYSEEK(curthread->t_filetable->t_fdtable[fd]->vn, position);
	*retval = position;
	curthread->t_filetable->t_fdtable[fd]->offset = position;
	lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
	lock_release(curthread->t_filetable->t_fdlock);
	return result;
}


/* really not "file" calls, per se, but might as well put it here */

/*
 * sys_chdir
 * 
 */
int
sys_chdir(userptr_t path)
{
	char *p;
	int err;
	if( ((p) = (char *)kmalloc(__PATH_MAX)) == NULL) {
		return ENOMEM;
	}
	if ((err = copyinstr(path, p, __PATH_MAX, NULL))) {
		kfree(p);
		return err;
	}

	int result = vfs_chdir(p);
	kfree(p);
	return(result);
}

/*
 * sys___getcwd
 * 
 * 
 */
int
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{
	struct uio user_uio;
	struct iovec user_iov;
	int result;

	mk_useruio(&user_iov, &user_uio, buf, buflen, 0, UIO_READ);
	result = vfs_getcwd(&user_uio);
	if(result) {
		return result;
	}
	*retval = buflen - user_uio.uio_resid;
	return 0;
}

/*
 * sys_fstat
 * 
 */
int
sys_fstat(int fd, userptr_t statptr)
{
	int result;
	struct stat buf;

	if(fd < 0 || fd >= __OPEN_MAX) {
		return EBADF;
	}

	if (curthread->t_filetable == NULL
			|| curthread->t_filetable->t_fdtable[fd] == NULL) {
		return EBADF;
	}

	lock_acquire(curthread->t_filetable->t_fdtable[fd]->lock);

	result = VOP_STAT(curthread->t_filetable->t_fdtable[fd]->vn, &buf);
	if(result) {
		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
		return result;
	}
	lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
	
	//Since fstat retrive the information to the user level, 
	// so we need to copy out all the information that in the kernel
	// to the userptr and return that result
	return copyout(&buf, statptr, sizeof (struct stat));
}

/*
 * sys_getdirentry
 */
int
sys_getdirentry(int fd, userptr_t buf, size_t buflen, int *retval)
{
	struct uio user_uio;
	struct iovec user_iov;
	int result;
	
	if(fd < 0 || fd >= __OPEN_MAX) {
		return EBADF;
	}

	if (curthread->t_filetable == NULL
			|| curthread->t_filetable->t_fdtable[fd] == NULL) {
		return EBADF;
	}

	lock_acquire(curthread->t_filetable->t_fdtable[fd]->lock);

	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&user_iov, &user_uio, buf, buflen, curthread->t_filetable->t_fdtable[fd]->offset, UIO_READ);

	/* does the read */
	result = VOP_GETDIRENTRY(curthread->t_filetable->t_fdtable[fd]->vn, &user_uio);
	if (result) {
		lock_release(curthread->t_filetable->t_fdtable[fd]->lock);
		return result;
	}

	curthread->t_filetable->t_fdtable[fd]->offset = user_uio.uio_offset;

	/*
	 * The amount read is the size of the buffer originally, minus
	 * how much is left in it.
	 */
	*retval = buflen - user_uio.uio_resid;

	lock_release(curthread->t_filetable->t_fdtable[fd]->lock);

	return 0;
}

/* END A3 SETUP */




