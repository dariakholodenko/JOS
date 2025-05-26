// implement fork from user space

#include <inc/lib.h>
#include <inc/string.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

extern void _pgfault_upcall();
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	envid_t envid = sys_getenvid();
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	
	if (!(err & FEC_WR))
		panic("[%08x] pgfault: Invalid access to va 0x%x, error %e from eip %p\n", envid, addr, err, utf->utf_eip);
		
	if(!(uvpt[PGNUM(addr)] & PTE_COW))
		panic("[%08x] pgfault: Attempt to write to non-COW page at va %p\n", envid, addr);
		
	int perm = PTE_P | PTE_U | PTE_W; 
	
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	void *addr_al = (void *)ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(envid, (void *)PFTEMP, perm);
	if (r < 0)
		panic("pgfault: failed to allocate a new page: %e\n", r);
	
	memmove((void *)PFTEMP, addr_al, PGSIZE);
	r = sys_page_map(envid, (void *)PFTEMP, envid, addr_al, perm);
	if (r < 0)
		panic("pgfault: failed to map a new page: %e\n", r);
	
	r = sys_page_unmap(envid, (void *)PFTEMP);
	if (r < 0)
		panic("pgfault: failed to unmap a page: %e\n", r);
	
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void *va = (void *)(pn * PGSIZE);
	envid_t parent_envid = sys_getenvid();
	
	// LAB 4: Your code here.
	int perm = PGOFF(uvpt[pn]);
	perm &= PTE_SYSCALL;
	if ((perm & PTE_SHARE)) { // || (!(perm & PTE_W) && !(perm & PTE_COW))) {
		r = sys_page_map(parent_envid, va, envid, va, perm);
		if (r < 0) {
			panic("duppage: failed to map a new page %e\n", r);
			return r;
		}
		return 0;
	}
	
	//perm &= ~PTE_W;
	//perm |= PTE_COW;
	else if ((perm & PTE_COW) || (perm & PTE_W)) {
		r = sys_page_map(parent_envid, va, envid, va, PTE_COW | PTE_U | PTE_P);
		if (r < 0) {
			panic("duppage: failed to map a new page %e\n", r);
			return r;
		}
		
		//remap our page in the current env to be COW
		r = sys_page_map(parent_envid, va, parent_envid, va, PTE_COW | PTE_U | PTE_P);
		if (r < 0) {
			panic("duppage: failed to map a page %e\n", r);
			return r;
		}
	}
	else {
		r = sys_page_map(parent_envid, va, envid, va, PTE_U | PTE_P);
		if (r < 0) {
			panic("duppage: failed to map a new page %e\n", r);
			return r;
		}
	}
	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	
	set_pgfault_handler(pgfault);
	envid_t envid = sys_exofork();
	if (envid < 0)
		panic("in fork, sys_exofork: %e\n", envid);
	
	if (envid == 0) {
		//child
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	//parent
	/*unsigned ptx;
	for (ptx = PGNUM(UTEXT); ptx < PGNUM(UXSTACKTOP - PGSIZE); ptx++) {
		unsigned pdx = ptx/NPDENTRIES;
		if ((uvpd[pdx] & PTE_P) && (uvpt[ptx] & PTE_P)) {
				duppage(envid, ptx);
		}
	}*/
	cprintf("%s %d: [%08x] env %08x is running\n", __FILE__, __LINE__, sys_getenvid(), envid);
	uint32_t addr;
	for (addr = 0; addr < USTACKTOP; addr += PGSIZE) { 
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U)) {
				duppage(envid, PGNUM(addr));
		}
	}
	cprintf("%s %d: [%08x] env %08x is running\n", __FILE__, __LINE__, sys_getenvid(), envid);
	int r = sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
	if (r < 0)
		panic("fork: failed to allocate a new page %e\n", r);
		
	
	r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	if (r < 0)
		panic("fork: failed to set a pagfault handler %e\n", r);
		
	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if (r < 0)
		panic("fork: failed to set a status %e\n", r);
		
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
