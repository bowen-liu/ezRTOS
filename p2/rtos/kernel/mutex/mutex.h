#ifndef MUTEX_H_
#define MUTEX_H_

#include "../kernel_internal.h"
#include "../others/pid_queue.h"

//For the ease of manageability, we're making a new mutex data type. The old MUTEX type defined in OS.h will simply serve as an identifier.
typedef struct {
	
	MUTEX id;								//unique id for this mutex, 0 = uninitialized
	PID owner;								//the process that locked the mutex; 0 = free
	unsigned int lock_count;				//mutex can be recursively locked
	unsigned int highest_priority;
	PRIORITY owner_orig_priority;
	PID_Queue wait_queue;
	PID_Queue orig_priority;
	
	
} MUTEX_TYPE;

/*Variables Accessible by the OS*/
extern volatile unsigned int Mutex_Count;		//Number of Mutexes created so far.
extern volatile unsigned int Last_MutexID;

/*Accessible by OS*/
MUTEX_TYPE* findMutexByMutexID(MUTEX m);
void Kernel_Create_Mutex(void);

/*Accessible within kernel only*/
void Mutex_Reset(void);
void Kernel_Lock_Mutex(void);
void Kernel_Unlock_Mutex(void);



#endif /* MUTEX_H_ */