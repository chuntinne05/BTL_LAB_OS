/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
// #define MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt) // Thêm vùng nhớ đã giải phóng vào danh sách vùng trống
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid) // Lấy thông tin vùng nhớ của một biến (dựa vào symrgtbl)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    return NULL;
  }

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  /* TODO: commit the vmaid */
  // rgnode.vmaid
  pthread_mutex_lock(&mmvm_lock); // update
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) // tìm vùng trống trong VM, nếu tìm thấy (== 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  // Nếu không còn vùng trống, sẽ gọi syscall để mở rộng giới hạn vùng nhớ (sbrk)
  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
  /*Attempt to increate limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  // int inc_limit_ret;
  
  /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
  int old_sbrk = cur_vma->sbrk;
  
  /* TODO INCREASE THE LIMIT as invoking systemcall
  * sys_memap with SYSMEM_INC_OP
  */
 struct sc_regs regs;
 regs.a1 = SYSMEM_INC_OP;
 regs.a2 = vmaid;
 regs.a3 = inc_sz; // size mở rộng
 
 /* SYSCALL 17 sys_memmap */
 regs.orig_ax = 17;
 if (syscall(caller, regs.orig_ax, &regs) != 0) // nếu mở rộng không thành công
  {
    regs.flags = -1;                    // failed
    perror("failed to increase limit"); // in syscall
    pthread_mutex_unlock(&mmvm_lock);   //
    return -1;
  }
  regs.flags = 0; // successful

  /* TODO: commit the limit increment */
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;        // bắt đầu từ limit cũ
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + inc_sz; // kết thúc ở limit đã mở rộng

  /* TODO: commit the allocation address */
  // *alloc_addr = caller->mm->symrgtbl[rgid].rg_end - inc_sz;
  *alloc_addr = old_sbrk;
  pthread_mutex_unlock(&mmvm_lock); //
  return 0;
  // return inc_limit_ret;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct rgnode;
  // Dummy initialization for avoding compiler dummy warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    return -1;
  }

  // thu hồi vùng nhớ và đưa vào danh sách vùng nhớ trống freerg_list
  /* TODO: Manage the collect freed region to freerg_list */
  rgnode = *get_symrg_byid(caller->mm, rgid);
  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, &rgnode);

  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  /* TODO Implement allocation on vm area 0 */
  int addr;
  /* By default using vmaid = 0 */
  int val = __alloc(proc, 0, reg_index, size, &addr);
#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
  printf("PID=%d - Region=%d - Address=%08ld - Size=%d byte\n", proc->pid, reg_index, addr * sizeof(uint32_t), size);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  // print page number -> frame number, after will print ===========
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  /* TODO Implement free region */

  /* By default using vmaid = 0 */
  int val = __free(proc, 0, reg_index);
#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
  printf("PID=%d - Region=%d\n", proc->pid, reg_index);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  // print page number -> frame number, after will print ===========
  return val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
// Kiểm tra page có đang hiện diện trong RAM không => swap page nếu không có
{
  // pte: entry của page cần truy cập từ bảng trang (Page Table Entry)
  uint32_t pte = mm->pgd[pgn]; // pgd: Page Table Directory, pgd[pgn] -> framenum (fpn)

  if (!PAGING_PAGE_PRESENT(pte))
  // thực hiện thuật toán chọn victim page, gọi syscall để swap out/in, và cập nhật lại bảng trang
  {             /* Page is not online, make it actively living */
    int vicpgn; // số page của victim
    int swpfpn; // frame trong swap để chứa dữ liệu của victim
    int vicfpn; // frame vật lý của victim
    uint32_t vicpte;

    int tgtfpn = PAGING_PTE_SWP(pte); // the target frame storing our variable (frame trong swap chứa dữ liệu cần lấy lại)

    /* TODO: Play with your paging theory here */
    /* Find victim page */
    find_victim_page(caller->mm, &vicpgn); // chon page free

    /* Get free frame in MEMSWP */
    // Lấy 1 frame trống trong swap để chứa victim page
    MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

    /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/

    // Swap victim frame từ RAM -> SWP, (Swap out): đưa dữ liệu của victim từ RAM -> SWAP
    /* TODO copy victim frame to swap
     * SWP(vicfpn <--> swpfpn)
     * SYSCALL 17 sys_memmap
     * with operation SYSMEM_SWP_OP
     */
    vicpte = mm->pgd[vicpgn];
    vicfpn = PAGING_PTE_FPN(vicpte);

    struct sc_regs regs;
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = vicfpn;
    regs.a3 = swpfpn;
    regs.orig_ax = 17;

    /* SYSCALL 17 sys_memmap */

    /* TODO copy target frame form swap to mem
     * SWP(tgtfpn <--> vicfpn)
     * SYSCALL 17 sys_memmap
     * with operation SYSMEM_SWP_OP
     */

    if (syscall(caller, regs.orig_ax, &regs) != 0)
    {
      regs.flags = -1; // failed
      return -1;
    }

    regs.flags = 0; // successful

    // Swap in: lấy dữ liệu từ swap (target page) vào frame RAM vừa giải phóng
    /* TODO copy target frame form swap to mem
    //regs.a1 =...
    //regs.a2 =...
    //regs.a3 =..
    */

    regs.a2 = tgtfpn; // đang ở swap
    regs.a3 = vicfpn; // nơi cần đưa vào RAM

    /* SYSCALL 17 sys_memmap */
    if (syscall(caller, regs.orig_ax, &regs) != 0)
    {
      regs.flags = -1; // failed
      return -1;
    }

    regs.flags = 0; // successful

    /* Update page table */
    // Update victim PTE: đã bị swap ra (swap out)
    uint32_t swptyp = caller->active_mswp_id;
    pte_set_swap(&vicpte, swptyp, swpfpn); // đánh dấu trang đã bị swap ra và frame swap đang lưu trang đó
    mm->pgd[vicpgn] = vicpte;

    /* Update its online status of the target page */
    // Update target PTE: đã được swap vào RAM
    // pte_set_fpn() &
    // mm->pgd[pgn];
    // pte_set_fpn();
    // Update lại PTE của trang đang truy cập (present + frame mới)
    // pte = vicpte; không làm mm->pgd[pgn] thay đổi

    // Update target PTE (pgn)
    pte_set_fpn(&pte, vicfpn);   // RAM frame mới của page cần truy cập
    PAGING_PTE_SET_PRESENT(pte); // Đánh dấu present
    mm->pgd[pgn] = pte;          // Cập nhật lại PTE vào bảng trang thật

    // printf("Swapping out victim page %d (fpn=%d) to swpfpn=%d\n", vicpgn, vicfpn, swpfpn);
    // printf("Swapping in target page %d (tgtfpn=%d) to fpn=%d\n", pgn, tgtfpn, vicfpn);

    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(pte);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to access
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
// Truy cập trực tiếp bộ nhớ vật lý qua syscall để đọc
{
  int pgn = PAGING_PGN(addr);
  int offst = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
  {
    return -1; /* invalid page access */
  }

  /* TODO
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  int phyaddr = PAGING_PHYADDR(fpn, offst);

  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;

  /* SYSCALL 17 sys_memmap */
  regs.orig_ax = 17;

  if (syscall(caller, regs.orig_ax, &regs) != 0)
  {
    regs.flags = -1;
    return -1; // read failed
  }

  // Update data
  // data = (BYTE);
  *data = (BYTE)(regs.a3);
  regs.flags = 0; // succesful

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to access
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller) // Truy cập trực tiếp bộ nhớ vật lý qua syscall để ghi
{
  int pgn = PAGING_PGN(addr);
  int offst = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
  {
    return -1; /* invalid page access */
  }

  /* TODO
   *  MEMPHY_write(caller->mram, phyaddr, value);
   *  MEMPHY WRITE
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
   */
  int phyaddr = PAGING_PHYADDR(fpn, offst);

  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr;
  regs.a3 = value;

  /* SYSCALL 17 sys_memmap */
  regs.orig_ax = 17;
  if (syscall(caller, regs.orig_ax, &regs) != 0)
  {
    regs.flags = -1;
    return -1; // failed
  }

  regs.flags = 0; // succesful
  // Update data
  // data = (BYTE)

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t *destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
  /* TODO update result of reading action*/
  // destination
  if (val == 0)
  {
    *destination = (uint32_t)data;
  }
#ifdef IODUMP
  printf("================================================================\n");
  printf("===== PHYSICAL MEMORY AFTER READING =====\n");
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
#ifdef IODUMP
  printf("================================================================\n");
  printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller) // Thu hồi toàn bộ frame vật lý của tiến trình khi kết thúc
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
  }

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) // Chọn trang cần thay thế khi RAM đầy
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  // FIFO: First In First Out
  if (pg == NULL)
  {
    return -1;
  }

  // Special case: only one node in list
  if (pg->pg_next == NULL)
  {
    *retpgn = pg->pgn;
    free(pg);
    mm->fifo_pgn = NULL;
    return 0;
  }

  // Normal case: more than one node in list
  while (pg->pg_next->pg_next != NULL)
  {
    pg = pg->pg_next;
  }

  // pg now points to second last, remove last
  *retpgn = pg->pg_next->pgn;
  free(pg->pg_next);
  pg->pg_next = NULL;

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL) return -1;

  // struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  struct vm_rg_struct *prev = NULL;
  struct vm_rg_struct *curr = cur_vma->vm_freerg_list;
  while (curr != NULL) {
    unsigned long freesize = curr->rg_end - curr->rg_start + 1;
    if(freesize < size || curr->rg_start > curr->rg_end) {
      return -1;
    }
    if (freesize >= size) {
      newrg->rg_start = curr->rg_start;
      newrg->rg_end   = curr->rg_start + size - 1;
      // Update current region: if there is remaining space, adjust rg_start
      if (freesize > size) {
        curr->rg_start += size;
      } else {
        if (prev == NULL) {
          cur_vma->vm_freerg_list = curr->rg_next;
        } else {
          prev->rg_next = curr->rg_next;
        }
        free(curr);
      }
      return 0;
    }
    prev = curr;
    curr = curr->rg_next;
  }
  return -1;
}
// #endif