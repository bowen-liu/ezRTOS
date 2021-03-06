/*
 * Kernel variables and declarations that are used among all other kernel modules
 */ 

#ifndef KERNEL_SHARED_H_
#define KERNEL_SHARED_H_

#include "../os.h"			//will also include kernel.h
#include "kernel_errors.h"
#include "others/PtrList.h"
#include <stdio.h>
#include <string.h>


#define MAX_KERNEL_ARGS		5


/************************************************************************/
/*					KERNEL DATA TYPE DECLARATIONS                       */
/************************************************************************/
 
 /*Error codes generated by the kernel. Formally declared in kernel_errors.h*/
typedef enum ERROR_CODE ERROR_CODE;


/*Definitions for all possible states a process can be in*/
typedef enum
{
	DEAD = 0,
	READY,
	RUNNING,
	SLEEPING,
	SUSPENDED,
	WAIT_EVENT,
	WAIT_EVENTG,
	WAIT_MUTEX,
	WAIT_SEMAPHORE,
	WAIT_MAILBOX
	
} PROCESS_STATE;


/*Definitions for all available kernel requests.*/
typedef enum 
{
	NONE = 0,
	
	/*TASK*/
	TASK_CREATE,
	TASK_YIELD,
	TASK_TERMINATE,
	TASK_SUSPEND,
	TASK_RESUME,
	TASK_SLEEP,
	
	/*EVENT*/
	#ifdef EVENT_ENABLED
	E_CREATE,
	E_WAIT,
	E_SIGNAL,
	#endif
	
	/*EVENT GROUP*/
	#ifdef EVENT_GROUP_ENABLED
	EG_CREATE,
	EG_DESTROY,
	EG_SETBITS,
	EG_CLEARBITS,
	EG_WAITBITS,
	EG_GETBITS,
	#endif
	
	/*MUTEX*/
	#ifdef MUTEX_ENABLED
	MUT_CREATE,
	MUT_DESTROY,
	MUT_LOCK,
	MUT_UNLOCK,
	#endif
	
	/*SEMAPHORE*/
	#ifdef SEMAPHORE_ENABLED
	SEM_CREATE,
	SEM_DESTROY,
	SEM_GIVE,
	SEM_GET,  
	#endif
	
	/*MAILBOX*/
	#ifdef MAILBOX_ENABLED
	MB_CREATE,
	MB_DESTROY,
	MB_DESTROYM,
	MB_CHECKMAIL,
	MB_SENDMAIL,
	MB_RECVMAIL,
	#endif
	
	INVALID					//Not an actual request. do not use!
	
} KERNEL_REQUEST;


/************************************************************************/
/*                         Process Descriptor                           */
/************************************************************************/

typedef union {
	int val;
	void* ptr;
} Kernel_Arg;

typedef struct ProcessDescriptor
{ 
	PID pid;												//An unique process ID for this task.
	PRIORITY pri;										//The priority of this task, from 0 (highest) to 10 (lowest).
	PROCESS_STATE state;									//What's the current state of this task?
	   
	size_t stack_size;									//Total size of the stack in bytes
	unsigned char *stack;								//The origin location of where the stack was allocated at (ie, start of the stack)
	unsigned char *sp;									//The task's current stack pointer, relative to the task's current context
	taskfuncptr code;									//The function to be executed when this process is running.
	   
	   
	/*User defined task args*/
	int arg;												//User specified arg for the task 
	   
	   
	/*Used to pass requests from task to kernel*/
	KERNEL_REQUEST request;								//What the task want the kernel to do (when needed).
	Kernel_Arg request_args[MAX_KERNEL_ARGS];	//What values are needed for the specified kernel request.
	int request_retval;										//Value returned by the kernel after handling the request
	TICK request_timeout;						
	   
	   
	/*Used for task suspension/resuming*/
	PROCESS_STATE last_state;							//What's the PREVIOUS state of this task? Used for task suspension/resume.
	   
	   
	/*Used for other scheduling modes*/
	#ifdef PREVENT_STARVATION
	unsigned int starvation_ticks;
	#endif
	   
} PD;



/************************************************************************/
/*					SHARED KERNEL VARIABLES                             */
/************************************************************************/

//These kernel variables are accessible by other kernel modules and the OS internally
extern volatile PtrList ProcessList;
extern volatile PD* Current_Process;	
extern volatile unsigned int KernelActive;
extern volatile unsigned int Kernel_Request_Cswitch;	
extern volatile ERROR_CODE err;



/************************************************************************/
/*					SHARED KERNEL FUNCTIONS                             */
/************************************************************************/

PD* findProcessByPID(int pid);


#endif /* KERNEL_INTERNAL_H_ */