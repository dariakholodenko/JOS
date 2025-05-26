// User-level IPC library routines

#include <inc/lib.h>

#define debug 0

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'thisenv' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	int r;
	
	if (debug) {
		cprintf("%d ipc_recv: envid %08x thisens %08x\n", __LINE__, sys_getenvid(), thisenv);
	}
	
	// LAB 4: Your code here.
	if (!pg) {
		pg = (void*)(-1);
	}
	
	if ((r = sys_ipc_recv(pg)) < 0) {
		if (from_env_store)
			*from_env_store = 0;
	
		if (perm_store)
			*perm_store = 0;
			
		return r; //-E_INVAL;
	}
	
	if (debug) {
		cprintf("%d ipc_recv: envid %08x thisens %08x\n", __LINE__, sys_getenvid(), thisenv);
	}
	
	if (from_env_store)
		*from_env_store = thisenv->env_ipc_from;
	
	if (perm_store)
		*perm_store = thisenv->env_ipc_perm;
	
	if (debug) {
		cprintf("%d ipc_recv: envid %08x r = %d, thisenv %08x\n", __LINE__, sys_getenvid(), r, thisenv);
	}
	
	//if (debug) {
	//	if (thisenv)
	//		cprintf("ipc_recv: %08x received from %08x success, page %08x\n", sys_getenvid(), thisenv->env_ipc_from, pg);
	//	else
	//		cprintf("ipc_recv: %08x received success, page %08x\n", sys_getenvid(), pg);
	//}
		
	if (debug) {
		if (!thisenv)
			cprintf("%s %d: thisenv id = %d, r = %d, thisenv = %p\n",__FILE__, __LINE__, (sys_getenvid() & ((1<<10)-1)), r, thisenv);
	}
		
	return thisenv->env_ipc_value;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_try_send a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	int r;
	if (!pg) {
		pg = (void*)(-1);
	}
	
	do {
		r = sys_ipc_try_send(to_env, val, pg, perm);
		if (r < 0 && r != -E_IPC_NOT_RECV) 
			panic("ipc_send: %e\n", r);
		
		sys_yield();
		//if (r == -E_IPC_NOT_RECV) {
		//	sys_yield();
		//} else if (r < 0)
		//	panic("ipc_send: %e\n", r);
	}
	while (r != 0);
		
	//if (debug)
	//	cprintf("ipc_send: %08x sending to %08x success, page %08x\n", sys_getenvid(), to_env, pg);
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
