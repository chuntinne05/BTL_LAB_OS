/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"

#define SYSMEM_MAP_OP 1   // sử dụng trình xử lý giả lập (dummy handler)
#define SYSMEM_INC_OP 2   // tăng giới hạn bộ nhớ với hàm inc_vma_limit()
#define SYSMEM_SWP_OP 3   // hoán đổi trang bộ nhớ với hàm mm_swap_page()
#define SYSMEM_IO_READ 4  // đọc bộ nhớ vật lý với hàm MEMPHY_read()
#define SYSMEM_IO_WRITE 5 // ghi bộ nhớ vật lý với hàm MEMPHY_write()

extern struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid);
int inc_vma_limit(struct pcb_t *, int, int);
int __mm_swap_page(struct pcb_t *, int, int);
int liballoc(struct pcb_t *, uint32_t, uint32_t);
int libfree(struct pcb_t *, uint32_t);
int libread(struct pcb_t *, uint32_t, uint32_t, uint32_t *);
int libwrite(struct pcb_t *, BYTE, uint32_t, uint32_t);
int free_pcb_memph(struct pcb_t *);