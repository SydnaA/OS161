/*
 * Process-related syscalls.
 * New for ASST1.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/wait.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <pid.h>
#include <machine/trapframe.h>
#include <syscall.h>
#include <limits.h>
#include <copyinout.h>
#include <kern/signal.h>

/*
 * sys_fork
 * 
 * create a new process, which begins executing in md_forkentry().
 */


int
sys_fork(struct trapframe *tf, pid_t *retval)
{
	struct trapframe *ntf; /* new trapframe, copy of tf */
	int result;

	/*
	 * Copy the trapframe to the heap, because we might return to
	 * userlevel and make another syscall (changing the trapframe)
	 * before the child runs. The child will free the copy.
	 */

	ntf = kmalloc(sizeof(struct trapframe));
	if (ntf==NULL) {
		return ENOMEM;
	}
	*ntf = *tf; /* copy the trapframe */

	result = thread_fork(curthread->t_name, enter_forked_process, 
			     ntf, 0, retval);
	if (result) {
		kfree(ntf);
		return result;
	}

	return 0;
}

/*
 * sys_getpid
 * return the process id of the current process and stored into *retval
 */
int 
sys_getpid(pid_t *retval){
	*retval = curthread->t_pid;
	return 0;
}

/*
 * sys_waitpid
 * Placeholder comment to remind you to implement this.
 */
int
sys_waitpid(pid_t pid, int *status, int options, pid_t *retval) {
	int result;

	//  The options argument requested invalid or unsupported options.
	if (options != 0 && options != WNOHANG) {
		return EINVAL;
	}
	
	// check if valid pid
	if(pid == INVALID_PID || pid == BOOTUP_PID) {
		return ESRCH;
	}
	
	if (pid < PID_MIN || pid > PID_MAX){
		return EINVAL;
	}
		
	if (options == WNOHANG){
		return 0;
	}
	
	vaddr_t copy_status; // use for checking the status is valid or not
	
	// check for invalid pointer
	if(status == NULL) {
		return EFAULT;
	}
	
	if(copyout(status, (userptr_t) copy_status, sizeof(userptr_t))){
		return EFAULT;
	}
	
	result = pid_join(pid, status, options);
	if (result < 0){
		*retval = -result;
		return -1;
	}
	else{
		*retval = pid;
		return 0;
	}
}

/*
 * sys_kill
 * Placeholder comment to remind you to implement this.
 */
int
sys_kill(pid_t targetpid, int sig) {
	
	if(targetpid == INVALID_PID || targetpid == BOOTUP_PID || 
		targetpid < PID_MIN || targetpid > PID_MAX) {
		return EINVAL;
	}


	if(sig < 0 || sig > 31) {
		return EINVAL;
	}
	switch(sig) {
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGABRT:
		case SIGEMT:
		case SIGFPE:
		case SIGBUS:
		case SIGSEGV:
		case SIGSYS:
		case SIGPIPE:
		case SIGALRM:
		case SIGURG:
		case SIGTSTP:
		case SIGCHLD:
		case SIGTTIN:
		case SIGTTOU:
		case SIGIO:
		case SIGXCPU:
		case SIGXFSZ:
		case SIGVTALRM:
		case SIGPROF:
		case SIGUSR1:
		case SIGUSR2:
			return EUNIMP;
	}
	
	int result;
	result = pid_set_flag(targetpid, sig);
	if (result)
		return -1;
	else 
		return 0;
}


