/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
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
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
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
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

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
  struct vm_rg_struct rgnode;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  int ret = inc_vma_limit(caller, vmaid, inc_sz);
  if (ret != 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  int old_sbrk = cur_vma->sbrk;
  cur_vma->sbrk += inc_sz;
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  *alloc_addr = old_sbrk;

  if (size < inc_sz)
  {
    struct vm_rg_struct *freerg = malloc(sizeof(struct vm_rg_struct));
    if (freerg == NULL)
    {
      pthread_mutex_unlock(&mmvm_lock);
      return -1;
    }
    freerg->rg_start = old_sbrk + size;
    freerg->rg_end = old_sbrk + inc_sz;
    enlist_vm_freerg_list(caller->mm, freerg);
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
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
  //struct vm_rg_struct rgnode;

  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  struct vm_rg_struct *rg = &caller->mm->symrgtbl[rgid];
  if (rg->rg_start == rg->rg_end) 
    return -1;

  struct vm_rg_struct *new_rg = malloc(sizeof(struct vm_rg_struct));
  new_rg->rg_start = rg->rg_start;
  new_rg->rg_end = rg->rg_end;
  enlist_vm_freerg_list(caller->mm, new_rg);

  rg->rg_start = rg->rg_end = 0;
  
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
  return __alloc(proc, 0, reg_index, size, &addr);
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
  return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  // uint32_t pte = mm->pgd[pgn];

  // if (!PAGING_PAGE_PRESENT(pte))
  // { 
  //   int vicpgn, swpfpn; 
  //   MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
  //   uint32_t pte = mm->pgd[pgn];

  //   if (!PAGING_PAGE_PRESENT(pte))
  //   {
  //     int vicpgn, swpfpn;
  
  //     find_victim_page(caller->mm, &vicpgn);
  //     MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
  
  //     if (sys_memmap(caller, vicpgn, swpfpn, 0, SYSMEM_SWP_OP) < 0){
  //       return -1;
  //     }
  //     if (sys_memmap(caller, swpfpn, PAGING_FPN(pte), 0, SYSMEM_SWP_OP) < 0){
  //       return -1;
  //     }
  //     pte_set_swap(pte);
  //     mm->pgd[pgn] = PAGING_FPN(pte);
  //     enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  //   }
  //   *fpn = PAGING_FPN(mm->pgd[pgn]);
  //   return 0;
  // }
  // *fpn = PAGING_FPN(mm->pgd[pgn]);
  // return 0;
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte))
  {
    int freefpn;
    if (MEMPHY_get_freefp(caller->mram, &freefpn) == 0) {
      /* Free frame available in RAM */
      int tgt_swpfpn = PAGING_PTE_SWP(pte);
      struct sc_regs regs;
      regs.a1 = SYSMEM_SWP_OP;
      regs.a2 = (uint32_t)caller->active_mswp;
      regs.a3 = tgt_swpfpn;
      regs.a4 = (uint32_t)caller->mram;
      regs.a5 = freefpn;
      /* TODO: Invoke syscall 17 - Assuming it copies the page */
      pte_set_fpn(&mm->pgd[pgn], freefpn);
    } else {
      /* No free frames in RAM, select a victim */
      int vicpgn;
      if (find_victim_page(mm, &vicpgn) != 0) return -1;
      uint32_t vic_pte = mm->pgd[vicpgn];
      int vicfpn = PAGING_PTE_FPN(vic_pte);
      int swpfpn;
      if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0) return -1;

      /* Swap victim page to swap space */
      struct sc_regs regs;
      regs.a1 = SYSMEM_SWP_OP;
      regs.a2 = (uint32_t)caller->mram;
      regs.a3 = vicfpn;
      regs.a4 = (uint32_t)caller->active_mswp;
      regs.a5 = swpfpn;
      /* TODO: Invoke syscall 17 */

      /* Swap target page into RAM */
      int tgt_swpfpn = PAGING_PTE_SWP(pte);
      regs.a1 = SYSMEM_SWP_OP;
      regs.a2 = (uint32_t)caller->active_mswp;
      regs.a3 = tgt_swpfpn;
      regs.a4 = (uint32_t)caller->mram;
      regs.a5 = vicfpn;
      /* TODO: Invoke syscall 17 */

      /* Update page table entries */
      pte_set_swap(&mm->pgd[vicpgn],0, swpfpn);
      pte_set_fpn(&mm->pgd[pgn], vicfpn);
    }
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_PTE_FPN(mm->pgd[pgn]);
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  // int pgn = PAGING_PGN(addr);
  // int fpn;
  // if (pg_getpage(mm, pgn, &fpn, caller) != 0)
  //   return -1; 

  // int phyaddr = PAGING_FPN_TO_ADDR(fpn) + PAGING_OFFST(addr);
  // if (sys_memmap(caller, phyaddr, (int)data, sizeof(BYTE), SYSMEM_IO_READ) < 0){
  //   return -1;
  // }
  // *data = MEMPHY_read(caller->mram, phyaddr);
  // return 0;
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) | off;
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = (uint32_t)caller->mram;
  regs.a3 = phyaddr;
  // *data = (BYTE)regs.a0;
  return MEMPHY_read(caller->mram, phyaddr, data);
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  // int pgn = PAGING_PGN(addr);
  // int fpn;

  // if (pg_getpage(mm, pgn, &fpn, caller) != 0)
  //   return -1; /* invalid page access */
  // int phyaddr = PAGING_FPN_TO_ADDR(fpn) + PAGING_OFFST(addr); // Get physical address
  // if (sys_memmap(caller, phyaddr, (int)&value, sizeof(BYTE), SYSMEM_IO_WRITE) < 0){
  //   return -1;
  // }
  // MEMPHY_write(caller->mram, phyaddr, value);
  // return 0;
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) | off;
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = (uint32_t)caller->mram;
  regs.a3 = phyaddr;
  regs.a4 = value;
  /* TODO: Invoke syscall 17 - Writes value to physical address */
  return MEMPHY_write(caller->mram, phyaddr, value);
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
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
  *destination = (uint32_t)data;

#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); 
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
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
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
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
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
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  if (mm->fifo_pgn == NULL)
  return -1;

  struct pgn_t *victim = mm->fifo_pgn;
  mm->fifo_pgn = victim->pg_next;
  *retpgn = victim->pgn;
  free(victim);
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
    if (curr->rg_end - curr->rg_start >= size) {
      newrg->rg_start = curr->rg_start;
      newrg->rg_end = curr->rg_start + size;
      if (curr->rg_end > newrg->rg_end) {
        curr->rg_start = newrg->rg_end;
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

//#endif
