// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  // struct vm_area_struct *pvma = mm->mmap;

  // if (mm->mmap == NULL)
  //   return NULL;

  // int vmait = pvma->vm_id;

  // while (vmait < vmaid)
  // {
  //   if (pvma == NULL)
  //     return NULL;

  //   pvma = pvma->vm_next;
  //   vmait = pvma->vm_id;
  // }

  // return pvma;

  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
      return NULL;

  int vmait = 0;

  while (vmait < vmaid)
  {
      if (pvma == NULL)
          return NULL;

      vmait++;
      pvma = pvma->vm_next;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, int vicfpn , int swpfpn)
{
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  // struct vm_rg_struct * newrg;
  // struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // newrg = malloc(sizeof(struct vm_rg_struct));

  // newrg->rg_start = cur_vma->sbrk; 
  // newrg->rg_end = newrg->rg_start + alignedsz;
  // newrg->rg_next = NULL;

  // cur_vma->sbrk = newrg->rg_end;

  // return newrg;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (!cur_vma) return NULL;

  // Kiểm tra vượt giới hạn `vm_end`
  if (cur_vma->sbrk + alignedsz > cur_vma->vm_end) return NULL;

  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  if (!newrg) return NULL;

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = cur_vma->sbrk + alignedsz;
  cur_vma->sbrk += alignedsz;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  // struct vm_area_struct *pvma = caller->mm->mmap;
  // struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // while (pvma != NULL)
  // {
  //   if(pvma != cur_vma) {
  //     if (vmastart < pvma->vm_end && vmaend > pvma-> vm_start)
  //     {
  //       return -1;
  //     }
  //   }
  //   pvma = pvma->vm_next;
  // }

  // if (vmastart != cur_vma->sbrk)
  // {
  //   printf("Lỗi new vm region start (%d) không khớp với sbrk hiện tại (%d)\n.", vmastart, cur_vma->sbrk );
  //   return -1;
  // }
  
  // return 0;

  struct vm_area_struct *cur = caller->mm->mmap;
  if(vmaend < 0 ){
      printf("[VALIDATE_OVERLAP] Error: NOT ENOUGH MEMORY,%d %d\n");
      return -1;
  }
  #ifdef MM_PAGING_HEAP_GODOWN
  if(vmaend > caller->vmemsz) {
      printf("[VALIDATE_OVERLAP] Error: NOT ENOUGH MEMORY,%d %d\n", caller->vmemsz, vmaend);
      return -1;
  }
  #endif

  while (cur) {
      if (cur->vm_id != vmaid && 
          ((vmastart >= cur->vm_start && vmastart < cur->vm_end) ||
           (vmaend > cur->vm_start && vmaend <= cur->vm_end))) {
          printf("[VALIDATE_OVERLAP] Error: Overlap detected with VMA %d\n", cur->vm_id);
          return -1;
      }
      cur = cur->vm_next;
  }

  //printf("[VALIDATE_OVERLAP] No overlap detected for VMA %d\n", vmaid);
  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int old_end = cur_vma->vm_end;

  if (validate_overlap_vm_area(caller, vmaid, newrg->rg_start, newrg->rg_end) < 0) {
    free(newrg);
    return -1;
  }

  cur_vma->vm_end = area->rg_end;

  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0)
  {
    cur_vma->vm_end = old_end;
    free(newrg);
    return -1;
  }

  cur_vma->sbrk = area->rg_end;
  free(newrg);

  return 0;
}
// #endif
