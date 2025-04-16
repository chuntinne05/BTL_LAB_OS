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
#include "libmem.h"
#include "queue.h"
#include "string.h"
#include <stdlib.h>

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs) {
    char proc_name[100];
    uint32_t data;

    //hardcode for demo only
    uint32_t memrg = regs->a1;
    
    /* TODO: Get name of the target proc */
    //proc_name = libread..

    /* TODO: Traverse proclist to terminate the proc
    *       stcmp to check the process match proc_name
    */
    
    int i = 0;
    data = 0;
    while(data != -1){
        libread(caller, memrg, i, &data);
        proc_name[i]= (char)data;
        if(data == -1) proc_name[i]='\0';
        i++;
    }

    char my_proc_name[100];
    my_proc_name[0] = '\0';
    strcat(my_proc_name, "input/proc/");
    strcat(my_proc_name, proc_name); // Chiếu đến đường dẫn đầy đủ của proc_name

    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    //caller->running_list
    //caller->mlq_ready_queue
    struct queue_t *my_running_list = caller->running_list;
#ifdef MLQ_SCHED
    struct queue_t *my_mlq_ready_queue = caller->mlq_ready_queue;
#endif
    struct pcb_t *target = NULL;
    int killed_count = 0;
    

    /* TODO Maching and terminating 
    *       all processes with given
    *        name in var proc_name
    */
    if (my_running_list->size > 0) {
        int n = my_running_list->size;
        for (int i = 0; i < n; i++) {
            target = dequeue(my_running_list);
            if (target == NULL) continue; // Kiểm tra tránh lỗi NULL

            // Bảo vệ tiến trình hệ thống và chính mình
            if (target->pid == 0 || target == caller) {
                enqueue(my_running_list, target);
                continue;
            }

            if (strcmp(target->path, my_proc_name) == 0) {
                printf("\tProcess %d has been killed\n", target->pid);
                target->pc = target->code->size; // Đánh dấu tiến trình đã hoàn thành
                killed_count++;                
            }

            else enqueue(my_running_list, target); // Trả lại tiến trình vào hàng đợi
        }
    }
#ifdef MLQ_SCHED
    if (my_mlq_ready_queue->size > 0) {
        for (int q = 0; q < MAX_PRIO; q++) { // Giả sử có nhiều mức ưu tiên
            struct queue_t *queue = &my_mlq_ready_queue[q];
            
            int n = queue->size;
            for (int i = 0; i < n; i++) {
                target = dequeue(queue); // Lấy tiến trình từ mảng
                if (target == NULL) continue;

                // Bảo vệ tiến trình hệ thống và chính mình
                if (target->pid == 0 || target == caller) {
                    enqueue(queue, target);
                    continue;
                }

                if (strcmp(target->path, my_proc_name) == 0) {
                    printf("\tProcess %d has been killed\n", target->pid);
                    target->pc = target->code->size; // Đánh dấu tiến trình đã hoàn thành
                    libfree(target, memrg);
                    free(target); // Giải phóng bộ nhớ của tiến trình
                    killed_count++;
                }
                else enqueue(queue, target); // Trả lại tiến trình vào hàng đợi
            }
        }
    }

#endif
    printf("\tKilled %d processes matching \"%s\"\n", killed_count, my_proc_name);
    libfree(caller, memrg); // Giải phóng vùng nhớ trong regs
    return killed_count; // Trả về số lượng quá trình bị xóa
}
