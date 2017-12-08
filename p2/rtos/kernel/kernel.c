#include "kernel.h"

#include <stdio.h>
#include <string.h>

#define LED_PIN_MASK 0x80			//Pin 13 = PB7


/*System variables used by the kernel only*/
volatile static unsigned int Last_Dispatched;	//Which task in the process queue was last dispatched.

volatile unsigned int Tick_Count;				//Number of timer ticks missed
volatile unsigned int Ticks_Since_Last_Cswitch;


/*Variables accessible by OS*/
volatile PD* Current_Process;					//Process descriptor for the last running process before entering the kernel
volatile unsigned char *KernelSp;				//Pointer to the Kernel's own stack location.
volatile unsigned char *CurrentSp;				//Pointer to the stack location of the current running task. Used for saving into PD during ctxswitch.						//The process descriptor of the currently RUNNING task. CP is used to pass information from OS calls to the kernel telling it what to do.
volatile unsigned int KernelActive;				//Indicates if kernel has been initialzied by OS_Start().
volatile ERROR_TYPE err;						//Error code for the previous kernel operation (if any)


extern volatile PD Process[MAXTHREAD];


/************************************************************************/
/*						  KERNEL-ONLY HELPERS                           */
/************************************************************************/

/*Returns the pointer of a process descriptor in the global process list, by searching for its PID*/
PD* findProcessByPID(int pid)
{
	int i;
	
	//Valid PIDs must be greater than 0.
	if(pid <=0)
	return NULL;
	
	for(i=0; i<MAXTHREAD; i++)
	{
		if (Process[i].pid == pid)
		return &(Process[i]);
	}
	
	//No process with such PID
	return NULL;
}


/************************************************************************/
/*				   		       OS HELPERS                               */
/************************************************************************/

/*Returns the PID associated with a function's memory address*/
int findPIDByFuncPtr(voidfuncptr f)
{
	int i;
	
	for(i=0; i<MAXTHREAD; i++)
	{
		if (Process[i].code == f)
			return Process[i].pid;
	}
	
	//No process with such PID
	return -1;
}

/************************************************************************/
/*                  HANDLING SLEEP TICKS								 */
/************************************************************************/

static void Kernel_Dispatch_Next_Task();

void Kernel_Tick_ISR()
{
	++Tick_Count;
}

//This function is called ONLY by the timer ISR. Therefore, it will not enter the kernel through regular modes
/*void Kernel_Tick_ISR()
{
	Disable_Interrupt();
	
	++Tick_Count;
	++Ticks_Since_Last_Cswitch;

	//Preemptive Scheduling: Has it been a long time since we switched to a new task?
	if(Ticks_Since_Last_Cswitch >= CSWITCH_FREQ)
	{
		Kernel_Dispatch_Next_Task();
		CSwitch();						//same as Exit_Kernel(), interrupts are automatically enabled at the end
	}
		
	Enable_Interrupt();
}*/

//Processes all tasks that are currently sleeping and decrement their sleep ticks when called. Expired sleep tasks are placed back into their old state
void Kernel_Tick_Handler()
{
	int i;
	
	//No new ticks has been issued yet, skipping...
	if(Tick_Count == 0)
		return;
	
	for(i=0; i<MAXTHREAD; i++)
	{
		//Process any active tasks that are sleeping
		if(Process[i].state == SLEEPING)
		{
			//If the current sleeping task's tick count expires, put it back into its READY state
			Process[i].request_args[0] -= Tick_Count;
			if(Process[i].request_args[0] <= 0)
			{
				Process[i].state = READY;
				Process[i].request_args[0] = 0;
			}
		}
		
		//Process any SUSPENDED tasks that were previously sleeping
		else if(Process[i].last_state == SLEEPING)
		{
			//When task_resume is called again, the task will be back into its READY state instead if its sleep ticks expired.
			Process[i].request_args[0] -= Tick_Count;
			if(Process[i].request_args[0] <= 0)
			{
				Process[i].last_state = READY;
				Process[i].request_args[0] = 0;
			}
		}
	}
	Tick_Count = 0;
}


/************************************************************************/
/*                     KERNEL SCHEDULING FUNCTIONS                      */
/************************************************************************/

//Look through all ready tasks and select the next task for dispatching
static int Kernel_Select_Next_Task()
{
	unsigned int i = 0, j = Last_Dispatched;
	int highest_priority = LOWEST_PRIORITY + 1;
	int next_dispatch = -1;
	
	//Find the next READY task with the highest priority by iterating through the process list ONCE.
	//If all READY tasks have the same priority, the next task of the same priority is dispatched (round robin)
	for(i=0; i<MAXTHREAD; i++)
	{
		//Increment process index
		j = (j+1) % MAXTHREAD;
		
		//Select the READY process with the highest priority
		if(Process[j].state == READY && Process[j].pri < highest_priority)
		{
			next_dispatch = j;
			highest_priority = Process[next_dispatch].pri;
		}
	}
	
	return next_dispatch;
}

/* Dispatches a new task */
static void Kernel_Dispatch_Next_Task()
{
	unsigned int i;
	int next_dispatch = Kernel_Select_Next_Task();
	
	//When none of the tasks in the process list is ready
	if(next_dispatch < 0)
	{
		i = Last_Dispatched;
		
		//We'll temporarily re-enable interrupt in case if one or more task is waiting on events/interrupts or sleeping
		Enable_Interrupt();
		
		//Looping through the process list until any process becomes ready
		while(Process[i].state != READY)
		{
			//Increment process index
			i = (i+1) % MAXTHREAD;
			
			//Check if any timer ticks came in
			Disable_Interrupt();
			Kernel_Tick_Handler();	
			Enable_Interrupt();
		}
		
		//Now that we have some ready tasks, interrupts must be disabled for the kernel to function properly again.
		Disable_Interrupt();
		next_dispatch = Kernel_Select_Next_Task();
		
		if(next_dispatch < 0)
			printf("SOMETHING WENT WRONG WITH SCHEDULLER!\n");
		
	}
	
	Ticks_Since_Last_Cswitch = 0;
	
	if(Current_Process->state == RUNNING)
		Current_Process->state = READY;

	//Load the next selected task's process descriptor into Cp
	Last_Dispatched = next_dispatch;
	Current_Process = &(Process[Last_Dispatched]);
	CurrentSp = Current_Process->sp;
	Current_Process->state = RUNNING;
	
	printf("Dispatching PID:%d\n", Current_Process->pid);
}

/**
  * This internal kernel function is the "main" driving loop of this full-served
  * model architecture. Basically, on OS_Start(), the kernel repeatedly
  * requests the next user task's next system call and then invokes the
  * corresponding kernel function on its behalf.
  *
  * This is the main loop of our kernel, called by OS_Start().
  */
static void Kernel_Handle_Request() 
{
	Kernel_Dispatch_Next_Task();	//Select an initial task to run

	//After OS initialization, THIS WILL BE KERNEL'S MAIN LOOP!
	//NOTE: When another task makes a syscall and enters the loop, it's still in the RUNNING state!
	while(1) 
	{
		//Clears the process' request fields
		Current_Process->request = NONE;
		//Cp->request_arg is not reset, because task_sleep uses it to keep track of remaining ticks

		//Load the current task's stack pointer and switch to its context
		CurrentSp = Current_Process->sp;
		Exit_Kernel();


		/************************************************************************/
		/*     if this task makes a system call, it will return to here!		*/
		/************************************************************************/
		
		
		//Save the current task's stack pointer and proceed to handle its request
		Current_Process->sp = CurrentSp;
		
		//Check if any timer ticks came in
		Kernel_Tick_Handler();

		switch(Current_Process->request)
		{
			case CREATE_T:
			Kernel_Create_Task(Current_Process->code, Current_Process->pri, Current_Process->arg);
			break;
			
			case TERMINATE:
			Kernel_Terminate_Task();
			Kernel_Dispatch_Next_Task();					//Dispatch is only needed if the syscall requires running a different task  after it's done
			break;
		   
			case SUSPEND:
			Kernel_Suspend_Task();
			break;
			
			case RESUME:
			Kernel_Resume_Task();
			break;
			
			case SLEEP:
			Current_Process->state = SLEEPING;
			Kernel_Dispatch_Next_Task();					
			break;
			
			case CREATE_E:
			Kernel_Create_Event();
			break;
			
			case WAIT_E:
			Kernel_Wait_Event();	
			if(Current_Process->state != RUNNING) 
				Kernel_Dispatch_Next_Task();	//Don't dispatch to a different task if the event is already signaled
			break;
			
			case SIGNAL_E:
			Kernel_Signal_Event();
			Kernel_Dispatch_Next_Task();
			break;
			
			case CREATE_M:
			Kernel_Create_Mutex();
			break;
			
			case LOCK_M:
			Kernel_Lock_Mutex();
			if(Current_Process->state != RUNNING)
				Kernel_Dispatch_Next_Task();		//Task is now waiting for mutex lock
				
			break;
			
			case UNLOCK_M:
			Kernel_Unlock_Mutex();
			//Does this need dispatch under any circumstances?
			break;
			
			
			case CREATE_SEM:
			Kernel_Create_Semaphore(Current_Process->request_args[0], Current_Process->request_args[1]);
			break;
			
			case GIVE_SEM:
			Kernel_Semaphore_Give(Current_Process->request_args[0], Current_Process->request_args[1]);
			break;
			
			case GET_SEM:
			Kernel_Semaphore_Get(Current_Process->request_args[0], Current_Process->request_args[1]);
			if(Current_Process->state != RUNNING) 
				Kernel_Dispatch_Next_Task();		//Switch task if the process is now waiting for the semaphore
			break;
			
		   
			case YIELD:
			case NONE:					// NONE could be caused by a timer interrupt
			//Current_Process->state = READY;
			Kernel_Dispatch_Next_Task();
			break;
       
			//Invalid request code, just ignore
			default:
				err = INVALID_KERNET_REQUEST_ERR;
			break;
       }
	   
    } 
}

	
/************************************************************************/
/* KERNEL BOOT                                                          */
/************************************************************************/

/*This function initializes the RTOS and must be called before any othersystem calls.*/
void Kernel_Reset()
{
	KernelActive = 0;
	Tick_Count = 0;
	Ticks_Since_Last_Cswitch = 0;
	Last_Dispatched = 0;
	Last_PID = 0;
	
	err = NO_ERR;
	
	Task_Reset();
	Event_Reset();
	Mutex_Reset();
	Semaphore_Reset();
	
	#ifdef DEBUG
	printf("OS initialized!\n");
	#endif

}

/* This function starts the RTOS after creating a few tasks.*/
void Kernel_Start()
{
	if (!KernelActive && Task_Count > 0)
	{
		Disable_Interrupt();
		
		/* we may have to initialize the interrupt vector for Enter_Kernel() here. */
			/* here we go...  */
		KernelActive = 1;
		
		/*Initialize and start Timer needed for sleep*/
		Timer_init();
		
		#ifdef DEBUG
		printf("OS begins!\n");
		#endif
		
		Kernel_Handle_Request();
		/* NEVER RETURNS!!! */
	}
}
