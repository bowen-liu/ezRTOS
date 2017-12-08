#ifndef TASK_H_
#define TASK_H_

#include "../kernel_internal.h"


void Task_Reset(void);
void Kernel_Create_Task(voidfuncptr f, PRIORITY py, int arg);
void Kernel_Suspend_Task(void);
void Kernel_Resume_Task(void);
void Kernel_Terminate_Task(void);


/*Variables shared with the main kernel module*/
extern volatile PD Process[MAXTHREAD];	
extern volatile unsigned int Task_Count;	



#endif /* TASK_H_ */