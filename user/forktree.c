// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

#if 0
#define DEPTH 3

void forktree(const char *cur);

void
forkchild(const char *cur, char branch)
{
	char nxt[DEPTH+1];

	if (strlen(cur) >= DEPTH)
		return;

	snprintf(nxt, DEPTH+1, "%s%c", cur, branch);
	if (fork() == 0) {
		forktree(nxt);
		exit();
	}
}

void
forktree(const char *cur)
{
	cprintf("%04x: I am '%s'\n", sys_getenvid(), cur);

	forkchild(cur, '0');
	forkchild(cur, '1');
}

void
umain(int argc, char **argv)
{
	forktree("");
}

#endif


#define DEPTH 3

void forktree(const char *cur);

void
forkchild(const char *cur, char branch)
{	
	char nxt[DEPTH+1];

	if (strlen(cur) >= DEPTH)
		return;
	
	//if (branch == '1') {
	//	sys_set_prio(0, ENV_PRIO_LOW);
	//}
	snprintf(nxt, DEPTH+1, "%s%c", cur, branch);
	if (fork() == 0) {
		forktree(nxt);
		exit();
	}
}

void
forktree(const char *cur)
{
	cprintf("%04x: I am '%s'\n", sys_getenvid(), cur);
	//cprintf("%04x: I am '%s'\n", thisenv->env_id, cur);

	forkchild(cur, '0');
	forkchild(cur, '1');
}

void
umain(int argc, char **argv)
{	
	forktree("");
}
