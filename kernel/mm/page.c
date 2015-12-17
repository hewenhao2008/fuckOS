#include <string.h>
#include <errno.h>

#include <mm/pages.h>
#include <mm/mmu.h>
#include <mm/mmzone.h>

#include <fuckOS/kernel.h>

#include <asm/x86.h>

void page_check()
{

}

static struct page* 
_pages_alloc(struct zone_struct *zone ,
			gfp_t flage,uint32_t order)
{
	struct page* page = NULL;

	if ((flage & _GFP_ZERO) && (flage & _GFP_HIGH))
		panic("(flage & _GFP_ZERO) && (flage & _GFP_HIGH)\n");

	page = alloc_buddy(zone,order);

	if(page == NULL)
		goto out;

	if (flage & _GFP_ZERO) {
		int i = PAGE_SIZE << order;
		memset((void*)page2virt(page),0,i);
	}

out:
	return page;
}
void pages_free(struct page* page,uint8_t order)
{
#ifdef CONFIG_DEBUG
	assert(page);
	assert(order >= 0);
	assert(order <= MAXORDER);
#endif	
	if(page2phys(page) < NORMAL_ADDR)
		free_buddy(&zone_normal,page,order);
	else
		free_buddy(&zone_high,page,order);
}

void page_free(struct page* page)
{
	pages_free(page,0);
}


struct page* 
page_alloc(gfp_t flage)
{
	return pages_alloc(flage,0);
}


struct page* 
pages_alloc(gfp_t flage,uint8_t order)
{
	struct page* page = NULL;
#ifdef CONFIG_DEBUG
	assert(order >= 0);
	assert(order <= MAXORDER);
#endif
	if((flage & _GFP_ZERO) && (flage & _GFP_HIGH))
		goto out;
	if(flage & _GFP_HIGH)
		page = _pages_alloc(&zone_high ,flage,order);
	else
		page = _pages_alloc(&zone_normal ,flage,order);
out:
	return page;
}

void refresh_tlb(struct mm_struct* mm, viraddr_t va)
{
	// Flush the entry only if we're modifying the current address space.
	//if (!curenv || curenv->env_pgdir == pgdir)
	invlpg((void*)va);
	//int cr3 = rcr3();
	//lcr3(cr3);
}

void page_decref(struct page* page)
{
	if (atomic_dec_and_test(&page->nref)) {
		//page_free(pf);
	}
}

struct page* 
page_lookup(struct mm_struct* mm, 
		viraddr_t va, pte_t **pte_store)
{
#ifdef CONFIG_DEBUG
	assert(mm);
	assert(va);
#endif
	pte_t* pte = page_walk(mm, va, false);
	if( pte == NULL ) 
		return NULL;
	if(pte_none(*pte)) 
		return NULL;
	if ( NULL != pte_store )
		*pte_store = pte;
	return virt2page(pte_page_vaddr(*pte));
}

void page_remove(struct mm_struct* mm, viraddr_t va) 
{
	struct page *page;
	pte_t *pte_store;

#ifdef CONFIG_DEBUG
	assert(mm);
	assert(va);
#endif
	page = page_lookup(mm, va, &pte_store);

	if( page == NULL ) return;
	
	page_decref(page);

	pte_set(pte_store,0,0);

	refresh_tlb(mm, va);
}

int page_insert(struct mm_struct* mm, 
			struct page *page,viraddr_t va, uint32_t perm)
{
#ifdef CONFIG_DEBUG
	assert(mm);
	assert(page);
	assert(va);
#endif
	pte_t* pte = page_walk(mm, va,true);

	if (!pte) return -ENOMEM;

	atomic_inc(&page->nref);

	if (pte_present(*pte)) 
		page_remove(mm, va);	
	
	pte_set(pte, page2phys(page) , perm);

	refresh_tlb(mm, va);
	return 0;
}

static int get_pmd(pgd_t *pgd,int perm)
{
	struct page* page;
	physaddr_t pa;
	page = pages_alloc(_GFP_ZERO,0);
	if (!page)
		return -1;
	pa = page2phys(page);
#ifdef CONFIG_PAE
	uint32_t pe = perm & ~(_PAGE_RW | _PAGE_USER);
#else
	uint32_t pe = perm;
#endif
	pgd_set(pgd,pa,pe);
	return 0;
}

static int get_pte(pmd_t *pmd,int perm)
{
	struct page* page;
	physaddr_t pa;
	page = pages_alloc(_GFP_ZERO,0);
	if (!page)
		return -1;
	pa = page2phys(page);
	pmd_set(pmd,pa,perm);
	return 0;
}

/*
*page_walk - look up a pte_t from a user-virtual address
*/
pte_t *page_walk(struct mm_struct* mm,viraddr_t address,bool create)
{
#ifdef CONFIG_DEBUG
	assert(mm);
	assert(address);
#endif

	pgd_t * pgd = NULL;
	pte_t * pte = NULL;
	pmd_t * pmd = NULL;
	struct page *page = NULL;

	spin_lock(&mm->page_table_lock);

	pgd =  pgd_offset (mm->mm_pgd, address);

	if (pgd_none(*pgd) && !create)
		goto unlock;
	else if (pgd_none(*pgd)) {
		if( get_pmd(pgd,_PAGE_PRESENT | _PAGE_RW | _PAGE_USER) < 0)
			goto unlock;
		pgd =  pgd_offset (mm->mm_pgd, address);
	}
	pmd =  pmd_offset (pgd, address);
	if (pmd_none(*pmd) && !create)
		goto unlock;
	else if (pmd_none(*pmd)) {
		if( get_pte(pmd,_PAGE_PRESENT | _PAGE_RW | _PAGE_USER) < 0)
			goto unlock;
		pmd =  pmd_offset (pgd, address);
	}
	pte = pte_offset (pmd, address);
unlock:
	spin_unlock(&mm->page_table_lock);
	return pte;
}
