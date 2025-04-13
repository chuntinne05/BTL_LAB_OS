// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t ram_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t swap_lock = PTHREAD_MUTEX_INITIALIZER;
/*
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 * hàm này ánh xạ một khoảng các trang ảo từ addr -> (pgnum * PAGING_PAGESZ)
 * vào bảng trang của tiến trình 
 * Thêm các stt trang (page numbers) vào fifo list
 */
int vmap_page_range(struct pcb_t *caller,           // process call
                    int addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   // no guarantee all given pages are mapped
  // struct framephy_struct *fpit
  // int pgit;
  // int pgn = PAGING_PGN(addr);

  // ret_rg->rg_start = addr;
  // ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
  // ret_rg->rg_next = NULL;
  // // ret_rg->vmaid = caller->mm->mmap->vm_id;

  // for (pgit = 0; pgit < pgnum; pgit++) {
  //   int cur_addr = addr +  pgit * PAGING_PAGESZ;
  //   int pgn = PAGING_PGN(cur_addr);

  //   // nếu không có khung trống trong frames , lấy fpn của nó
  //   if (frames !=  NULL)
  //   {
  //     pte_set_fpn(&caller->mm->pgd[pgn], frames->fpn);
  //     frames = frames->fp_next;
  //   }
  //   else
  //   {
  //     printf("lỗi không đủ frames để ánh xạ tại địa chỉ %d \n", cur_addr);
  //     return -1;
  //   }

  //   // thêm stt trang vào fifo
  //   enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  // }
  // return 0;

  struct framephy_struct *cur_frame = frames; // Con trỏ đến frame hiện tại
  int pgn_start = PAGING_PGN(addr);          // Trang ảo bắt đầu
  int pgn_end = pgn_start + pgnum;           // Trang ảo kết thúc

    // //Cập nhật thông tin vùng ánh xạ trả về
    // ret_rg->rg_start = addr;                    // Địa chỉ ảo bắt đầu
    // ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ; // Địa chỉ ảo kết thúc
    // ret_rg->vmaid = caller->mm->mmap->vm_id;   // Gắn vùng địa chỉ ảo hiện tại

    // Ánh xạ từng trang ảo vào frame vật lý
  for (int pgit = 0; pgit < pgnum; pgit++) {
    int pgn = pgn_start + pgit;            // Trang hiện tại
    uint32_t *pte = &caller->mm->pgd[pgn]; // PTE của trang

    // Kiểm tra xem frame có đủ để ánh xạ không
    if (cur_frame == NULL) {
        //printf("[VMAP_PAGE_RANGE] Error: Not enough frames for page %d.\n", pgn);
        return -1; // Trả lỗi nếu không đủ frame
    }

    // Kiểm tra trang đã được ánh xạ trước đó chưa
    if (PAGING_PAGE_PRESENT(*pte)) {
        //printf("[VMAP_PAGE_RANGE] Warning: Page %d is already mapped to frame %d.\n", 
                //pgn, PAGING_PTE_FPN(*pte));
        continue; // Bỏ qua nếu trang đã được ánh xạ
    }

    // Ánh xạ frame vào PTE
    pte_set_fpn(pte, cur_frame->fpn); // Cập nhật bảng trang
    //printf("[VMAP_PAGE_RANGE] Page %d mapped to frame %d.\n", pgn, cur_frame->fpn);

    // Thêm trang vào danh sách FIFO để theo dõi
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);

    // Chuyển sang frame tiếp theo
    cur_frame = cur_frame->fp_next;
  }

    return 0; // Trả về thành công
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 * Hàm lặp qua số trang cần cấp phát (pgnum), gọi MEMPHY_get_freefp để lấy
 * 1 khung trống -> tạo node của framephy_struct
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  // int pgit, fpn;
  // *frm_lst = NULL;

  // for (pgit = 0; pgit < req_pgnum; pgit++)
  // {
  //   // if (MEMPHY_get_freefp(caller->mram, &fpn) == 0)
  //   // {
  //   //   struct framephy_struct *new_node = malloc(sizeof(struct framephy_struct));
  //   //   if (new_node == NULL)
  //   //   {
  //   //     printf("lỗi allocate memory trong alloc_pages_range\n");
  //   //     return -1;
  //   //   }
  //   //   new_node->fpn = fpn;
  //   //   new_node->fp_next = *frm_lst;
  //   //   *frm_lst = new_node;
  //   // }
  //   // else
  //   // { 
  //   //   // không đủ khung trống (OUT OF MEMORY - OOM)
  //   //   printf("lỗi không đủ khung trống trong RAM\n");
  //   //   return -3000;
  //   // }
  // }

  // return 0;

  for (int pgit = 0; pgit < req_pgnum; pgit++) {
    uint32_t *pte = &caller->mm->pgd[pgit];

    // Kiểm tra nếu trang đã được ánh xạ
    if (PAGING_PAGE_PRESENT(*pte)) {
        //printf("[ALLOC_PAGES_RANGE] Page %d already mapped to frame %d.\n", pgit, PAGING_PTE_FPN(*pte));
        continue;
    }

    int fpn;
    // Lấy khung trống từ RAM
    pthread_mutex_lock(&ram_lock);
    if (MEMPHY_get_freefp(caller->mram, &fpn) != 0) {
        pthread_mutex_unlock(&ram_lock);

        //printf("[ALLOC_PAGES_RANGE] Error: Out of free frames.\n");
        return -1;
    }
    pthread_mutex_unlock(&ram_lock);

    // Cập nhật danh sách khung trang
    struct framephy_struct *node = malloc(sizeof(struct framephy_struct));
    if (node == NULL) {
        printf("[ALLOC_PAGES_RANGE] Error: Failed to allocate memory for framephy_struct.\n");
        return -1;
    }
    node->fpn = fpn;
    node->fp_next = *frm_lst;
    *frm_lst = node;

    // Cập nhật PTE
    pte_set_fpn(pte, fpn);
    if (!PAGING_PAGE_PRESENT(*pte)) {
        printf("[ALLOC_PAGES_RANGE] Error: Failed to set frame number in PTE for page %d.\n", pgit);
        // Thêm trang vào danh sách FIFO
          //enlist_pgn_node(&caller->mm->fifo_pgn, pgit);
        return -1;
    }
    enlist_pgn_node(&caller->mm->fifo_pgn, pgit);
    //printf("[ALLOC_PAGES_RANGE] Page %d mapped to frame %d.\n", pgit, fpn);
  }

  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 * Hàm gọi alloc_pages_range để cấp phát số trang, sau đó gọi vmap_page_range
 * để ánh xạ trang vào bộ nhớ
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc; // lưu mã trả về từ alloc_pages_range

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;
  if (ret_alloc == -3000)
  {
#ifdef MMDBG
    printf("OOM: vm_map_ram out of memory \n");
#endif
    return -1;
  }

  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 * sao chép ndung trang từ khung nguồn sang đích
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn)
{
  int cellidx;
  int addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));

  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;

  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  vma0->vm_next = NULL;
  vma0->vm_mm = mm;

  mm->mmap = vma0;
  mm->fifo_pgn = NULL;

  return 0;
}

// khởi tạo 1 memo region với biên re_start. r_end
struct vm_rg_struct *init_vm_rg(int rg_start, int rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}
//  Thêm một node của vm_rg vào danh sách
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

// Thêm một node chứa số trang (pgn) vào danh sách FIFO
int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[%d]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[%ld->%ld]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[%d]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start, pgn_end;
  int pgit;

  if (end == -1)
  {
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d", start, end);
  if (caller == NULL) { printf("NULL caller\n"); return -1;}
  printf("\n");

  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

// #endif