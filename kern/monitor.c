// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/env.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "bt", "Display backtrace information", mon_backtrace },
	{ "mp", "Show mappings for virtual addresses", mon_showmappings },
	{ "clrprm", "Clear permissions of a mapping: clrprm addr", mon_modify_permissions },
	{ "chprm", "Change permissions of a mapping: chprm addr <+/->[W|U]", mon_modify_permissions },
	{ "continue", "Continue execution", mon_continue },
	{ "c", "Continue execution", mon_continue },
	{ "step", "Single step program", mon_step },
	{ "s", "Single step program", mon_step },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	struct Eipdebuginfo info;
	uint32_t eip;
	uint32_t *args;
	uint32_t *ebp = (uint32_t*)read_ebp();
	cprintf("Stack backtrace:\n");
	
	//in entry.S before calling the C code ebp was set to 0
	//so in a chain of nested function calls 0 stored in ebp would mean end of the chain
	while(ebp) { 
		eip = *(ebp + 1);
		args = ebp + 2;
		debuginfo_eip((uintptr_t)eip, &info);
		cprintf("ebp %x  eip %x  args", ebp, eip);
		int i;
		for(i = 0; i < 5; i++) {
			cprintf(" %08x", args[i]);
		}
		cprintf("\n");
		uint32_t eip_offset = eip - info.eip_fn_addr;
		cprintf("\t%s:%d: %.*s+%d\n", info.eip_file, 
						info.eip_line, info.eip_fn_namelen, 
						info.eip_fn_name, eip_offset);
		ebp = (uint32_t*)*ebp;		
	}

	return 0;
}

int str2int_perms(char* str_perms)
{
	int perms = 0;
	
	if(strchr(str_perms, 'P') || strchr(str_perms, 'p'))
		perms |= PTE_P;
		
	if(strchr(str_perms, 'W') || strchr(str_perms, 'w'))
		perms |= PTE_W;

	if(strchr(str_perms, 'U') || strchr(str_perms, 'u'))
		perms |= PTE_U;
	
	if(strchr(str_perms, 'T') || strchr(str_perms, 't'))
		perms |= PTE_PWT;
		
	if(strchr(str_perms, 'C') || strchr(str_perms, 'c'))
		perms |= PTE_PCD;
	
	if(strchr(str_perms, 'A') || strchr(str_perms, 'a'))
		perms |= PTE_A;
	
	if(strchr(str_perms, 'D') || strchr(str_perms, 'd'))
		perms |= PTE_D;
	
	if(strchr(str_perms, 'S') || strchr(str_perms, 's'))
		perms |= PTE_PS;
	
	if(strchr(str_perms, 'G') || strchr(str_perms, 'g'))
		perms |= PTE_G;
	
	if(strchr(str_perms, 'V') || strchr(str_perms, 'v'))
		perms |= PTE_AVAIL;
		
	return perms;
}

void int2str_perms(char* str_perms_p, int perms)
{
	char str_perms[] = "----------";
	int perm_num = 10;
	char arr_perms[] = {'P', 'W', 'U', 'T', 'C',
						'A', 'D', 'S' ,'G', 'V'};
	int i;
	for(i = 0; i < perm_num; i++) {
		if (perms & 0x001) {
			str_perms[perm_num - i - 1] = arr_perms[i];
		}
		perms >>= 1;
	}
	memcpy(str_perms_p, str_perms, strlen(str_perms));
	/*if(perms & PTE_P)
		str_perms[9] = 'P';
		
	if(perms & PTE_W)
		str_perms[8] = 'W';
		
	if(perms & PTE_U)
		str_perms[7] = 'U';
	
	if(perms & PTE_PWT)
		str_perms[6] = 'T';
		
	if(perms & PTE_PCD)
		str_perms[5] = 'C';
		
	if(perms & PTE_A)
		str_perms[4] = 'A';
	
	if(perms & PTE_D)
		str_perms[3] = 'D';
	
	if(perms & PTE_PS)
		str_perms[2] = 'S';
		
	if(perms & PTE_G)
		str_perms[1] = 'G';
	
	if(perms & PTE_AVAIL)
		str_perms[0] = 'V';*/
	
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if(argc < 2 || argc > 3) {
		if(argc == 1)
			cprintf("showmappings: too few arguments!\n");
		
		if(argc > 3)
			cprintf("showmappings: too many arguments!\n");
		return 0;
	}
	
	uintptr_t va_start = strtol(argv[1], NULL, 16);
	uintptr_t va_end = (argc > 2) ? strtol(argv[2], NULL, 16) : va_start;
	
	pde_t *kern_pgdir = (pde_t *)KADDR(rcr3());
	uintptr_t va;
	for(va = va_start; va <= va_end; va += 0x1000) {
		pte_t *pte = pgdir_walk(kern_pgdir, (void*)va, false);
		if (!pte) {
			cprintf("VA 0x%x: PA [unmapped]\n", va);
			continue;
		}
		physaddr_t pa = PTE_ADDR(*pte);
		int perms = PGOFF(*pte);
		char str_perms[] = "----------";
		int2str_perms(str_perms, perms);
		cprintf("VA 0x%x PA 0x%x perms %s\n", va, pa, str_perms);
	}
	return 0;
}

int mon_modify_permissions(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va = strtol(argv[1], NULL, 16);
	pde_t * kern_pgdir = (pde_t *)KADDR(rcr3());
	pte_t * pte_p = pgdir_walk(kern_pgdir, (void*)va, false);
	int perms = PGOFF(*pte_p);
	
	if (strcmp(argv[0], "clrprm") == 0) {
		if (argc > 2) {
			cprintf("Usage: clrprm addr\n");
			return 0;
		}
		perms &= ~PTE_W;
		perms &= ~PTE_U;
	}
	
	if (strcmp(argv[0], "chprm") == 0) {
		if (argc != 3) {
			cprintf("Usage:  chprm addr <+/->[W|U]\n");
			return 0;
		}
		
		int max_arg_len = strlen("+UW");
		char p[max_arg_len]; 
		memcpy(p, argv[2], max_arg_len);
		if (strchr(p, 'W') || strchr(p, 'w')) {
			if (p[0] == '-')
				perms &= ~PTE_W;
			else
				perms |= PTE_W;
		}
		
		if (strchr(p, 'U') || strchr(p, 'u')) {
			if (p[0] == '-')
				perms &= ~PTE_U;
			else
				perms |= PTE_U;
		}
	}
	
	if (!pte_p) {
		cprintf("VA 0x%x: PA [unmapped]\n", va);
		return 0;
	}
	
	physaddr_t pa = PTE_ADDR(*pte_p);
	//make sure PTE_P present bit is preserved
	*pte_p =  pa | perms | PTE_P;
	char str_perms[] = "----------";
	int2str_perms(str_perms, perms);
	cprintf("VA 0x%x PA 0x%x perms %s\n", va, pa, str_perms);
		
	return 0;
}


int mon_continue(int argc, char **argv, struct Trapframe *tf) {
	if (argc != 1) {
		cprintf("Usage: continue\n");
		return 0;
	}
	
	if(!tf) {
		cprintf("Nothing to continue\n");
		return 0;
	}
	
	//clear Trap bit to disable single-step mode for debugging
	curenv->env_tf.tf_eflags &= ~FL_TF; 
	env_run(curenv);
	
	return 0;
}

int mon_step(int argc, char **argv, struct Trapframe *tf) {
	if (argc != 1)
		cprintf("Usage: step\n");
	
	if(!tf)
		cprintf("Nothing to step\n");
	
	//set Trap bit to enable single-step mode for debugging
	curenv->env_tf.tf_eflags |= FL_TF; 
	env_run(curenv);
	
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
