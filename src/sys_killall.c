/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

 #include "common.h"
 #include "syscall.h"
 #include "stdio.h"
 #include "libmem.h" //add free_pcb_memphy in libmem.h
 
 
 #include "queue.h"
 #include <string.h>
 
 int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
 {
     char proc_name[100];
     uint32_t data;
 
     //hardcode for demo only
     uint32_t memrg = regs->a1;
     
     /* TODO: Get name of the target proc */
     //proc_name = libread..
     int i = 0;
     data = 0;
     while(data != (uint32_t)(-1)){
         libread(caller, memrg, i, &data);
         proc_name[i] = (char)data;
         if(data == -1) proc_name[i]='\0';
         i++;
     }
     printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);
 
     /* TODO: Traverse proclist to terminate the proc
      *       stcmp to check the process match proc_name
      */
     //caller->running_list
     //caller->mlq_ready_queue
 
     /* TODO Maching and terminating 
      *       all processes with given
      *        name in var proc_name
      */
      
     //return 0;
 
     int count = 0;
     
     /* Find and terminate processes in the running list */
     if (caller->running_list != NULL) {
         struct queue_t *run_list = caller->running_list;
         for (i = 0; i < run_list->size; i++) {
             struct pcb_t *proc = run_list->proc[i];
             if (proc != NULL && strcmp(proc->path, proc_name) == 0) {
                 /* Free memory resources if we're using paging */
                 #ifdef MM_PAGING
                 if (proc->mm != NULL) {
                     free_pcb_memph(proc);
                 }
                 #endif
                 
                 /* Mark process as terminated */
                 proc->pid = 0; // Assuming 0 is invalid PID
                 count++;
                 
                 /* Remove terminated process from the list by shifting elements */
                 for (int j = i; j < run_list->size - 1; j++) {
                     run_list->proc[j] = run_list->proc[j + 1];
                 }
                 run_list->size--;
                 i--;
             }
         }
     }
     
     #ifdef MLQ_SCHED
     /* Find and terminate processes in multi-level queues */
     if (caller->mlq_ready_queue != NULL) {
         for (int prio = 0; prio < MAX_PRIO; prio++) {
             struct queue_t *queue = &caller->mlq_ready_queue[prio];
             for (i = 0; i < queue->size; i++) {
                 struct pcb_t *proc = queue->proc[i];
                 if (proc != NULL && strcmp(proc->path, proc_name) == 0) {
                     /* Free memory resources */
                     #ifdef MM_PAGING
                     if (proc->mm != NULL) {
                         free_pcb_memph(proc);
                     }
                     #endif
                     
                     /* Mark process as terminated */
                     proc->pid = 0;
                     count++;
                     
                     /* Remove terminated process from the queue */
                     for (int j = i; j < queue->size - 1; j++) {
                         queue->proc[j] = queue->proc[j + 1];
                     }
                     queue->size--;
                     i--;
                 }
             }
         }
     }
     #else
     /* Find and terminate processes in the ready queue */
     if (caller->ready_queue != NULL) {
         struct queue_t *ready_q = caller->ready_queue;
         for (i = 0; i < ready_q->size; i++) {
             struct pcb_t *proc = ready_q->proc[i];
             if (proc != NULL && strcmp(proc->path, proc_name) == 0) {
                 /* Free memory resources */
                 #ifdef MM_PAGING
                 if (proc->mm != NULL) {
                     free_pcb_memph(proc);
                 }
                 #endif
                 
                 /* Mark process as terminated */
                 proc->pid = 0;
                 count++;
                 
                 /* Remove terminated process from the queue */
                 for (int j = i; j < ready_q->size - 1; j++) {
                     ready_q->proc[j] = ready_q->proc[j + 1];
                 }
                 ready_q->size--;
                 i--;
             }
         }
     }
     #endif
     
     printf("Terminated %d processes with name \"%s\"\n", count, proc_name);
      
     return count; // Return the number of processes terminated
 }
 