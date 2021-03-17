#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <synch.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <limits.h>
#include <test.h>
#include <array.h>
#include "opt-A2.h" 



#if OPT_A2
// implement sys_fork
int sys_fork(struct trapframe *tf, pid_t *retval){
  // create a new process structure for the child process
  KASSERT(curproc!=NULL);
  struct proc *child = proc_create_runprogram(curproc->p_name);
  KASSERT(child != NULL); // check error for proc_create_runprogram
  KASSERT(child->pid >= 1);
  // create and copy the address space
  int err = as_copy(curproc_getas(), &(child->p_addrspace));
  if (err != 0 || child->p_addrspace == NULL) { //check error
    panic("Failed to assigned addrespace for child"); 
  }
  
  // child-parent relationship
 // lock_acquire(child->live_lock);
  //child->parent = curproc;
  proc_addChild(curproc,child);
  //lock_release(child->live_lock);


  // create a thread for the child process
  struct trapframe *ctf = kmalloc(sizeof(struct trapframe));
	if (ctf == NULL) {
		panic("No memory error"); 
	}

  err = set_trapframe(tf, ctf);
  if (err!= 0 || ctf == NULL) { //check error
    panic("Failed to create trapframe for child"); 
  }

  err = thread_fork("child's thread",
   child, &enter_forked_process,
   (void *)ctf, 1);
  if (err!= 0) { //check error
    proc_destroy(child);
    kfree(ctf);
    panic("Failed to create thread for child"); 
  }


  //set return value to parent proc
  *retval = child->pid;
  return 0;
}
#endif












/* this implementation of sys__exit does not do anything with the exit code */
/* this needs to be fixed to get exit() and waitpid() working properly */
void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  #if OPT_A2
  KASSERT(p->pid > 0);

//  lock_acquire(p->parent->children_lock);

  if (p->parent != NULL && p->parent->exit == false) { 
  //parent is still alive (not NULL or zombie)
  //p become zombie
  //wake up cv
  lock_acquire(p->live_lock);
  cv_signal(p->live, p->live_lock);
  lock_release(p->live_lock);

  lock_acquire(p->children_lock);
  p->exit = true;
  p->exitcode = _MKWAIT_EXIT(exitcode);
  lock_release(p->children_lock);

  } else {
  //the parent is dead
  //p can be fully deleted
  //delete all p's zombie children

  
  //wake up cv
  lock_acquire(p->live_lock);
  cv_signal(p->live, p->live_lock);
  lock_release(p->live_lock);


  lock_acquire(p->children_lock);
  p->exit = true;
  p->exitcode = _MKWAIT_EXIT(exitcode);

  for (unsigned int i = 0; i < array_num(p->children); i++) {
		struct proc * child = array_get(p->children, i);
		lock_acquire(child->children_lock);
		if (child->exit == true) {
      // child is zombie, need to be deleted
			lock_release(child->children_lock);
			proc_destroy(child);
		} else {
      //otherwise, do not delete
      child->parent = NULL;
			lock_release(child->children_lock);
		}
	}
  lock_release(p->children_lock);
  }

 // lock_release(p->parent->children_lock);



  #else
  (void)exitcode;
  #endif

  //DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}









/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
  struct proc *p = curproc;
  KASSERT(p != NULL);
  KASSERT(p->pid > 0);
  *retval = p->pid;
  return (0);
  #else
  *retval=1;
  return(0);
  #endif
}










/* stub handler for waitpid() system call                */
int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */


 
  if (options != 0) {
    return(EINVAL);
  }

  #if OPT_A2
  KASSERT(pid > 0);
  KASSERT(curproc != NULL);
  lock_acquire(wait_lock);
  //struct proc *child = proc_inChildren(pid, curproc);
  //if (pproc != NULL){
	//lock_acquire(pproc->children_lock);
  struct proc * child = NULL;
    for (unsigned i = 0; i < array_num(curproc->children); i++) {
		struct one_child *oc = array_get(curproc->children, i);
		if (pid == oc->pid) {
			child = oc->cproc; //is in the children array
			break;
		}
  //  }
	//lock_release(pproc->children_lock);
	}
  if (child == NULL) { //unable to find child
    panic("Only the parent can call waitpid on its children"); 
  }

  //check if the child is still alive
  //lock_acquire(curproc->live_lock);
  while (child->exit == false) { 
    //child still alive, parent need to wait
    cv_wait(child->live, wait_lock);
  }
  // now child is dead
  KASSERT(child->exit == true);
  exitstatus = _MKWAIT_EXIT(child->exitcode);
  lock_release(wait_lock);
#else


/* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;


 
 #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


