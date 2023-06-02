
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/io/fcntl.h>
#include "modulemgr_3.10_3.74.h"



#define sceKernelGetTPIDRPRW() ({ \
	ScePVoid v; \
	asm volatile ("mrc p15, #0, %0, c13, c0, #4\n": "=r"(v)); \
	v; \
	})


typedef struct NIDEntStore {
	struct NIDEntStore *next;
	SceNID old;
	SceNID new;
} NIDEntStore;

typedef struct NIDLibStore {
	struct NIDLibStore *next;
	SceNID old;
	SceNID new;
	SceUInt32 nEnt;
	NIDEntStore *ent_root;
} NIDLibStore;


NIDLibStore *g_nid_store = NULL;


int nid_compat_search_lib_by_new(SceNID new, NIDLibStore **result){

	NIDLibStore *lib_store = g_nid_store;

	while(lib_store != NULL){
		if(lib_store->new == new){
			*result = lib_store;
			return 0;
		}
		lib_store = lib_store->next;
	}

	return -1;
}

int parse_line(char *in, char **cont, char **next){

	while(*in != 0 && (*in == ' ' || *in == '\t')){
		in++;
	}

	if(*in == 0){
		return -1;
	}

	char *name_end = strchr(in, ' ');
	if(name_end == NULL){
		name_end = strchr(in, '\t');
	}

	if(name_end == NULL){
		name_end = strchr(in, 0);
	}

	char *new_name = ksceKernelAllocHeapMemory(0x1000B, (name_end - in) + 1);
	if(new_name == NULL){
		return -1;
	}

	new_name[name_end - in] = 0;
	memcpy(new_name, in, name_end - in);

	*cont = new_name;

	if(*name_end == 0){
		*next = NULL;
	}else{
		*next = name_end;
	}

	// ksceKernelPrintf("new_name : \"%s\"\n", new_name);
	// ksceKernelFreeHeapMemory(0x1000B, new_name);

	return 0;
}


// new_compat_func old_lib_nid old_ent_nid new_lib_nid new_ent_nid

int new_compat(char *args){

	int res;
	char *s_nid, *n;
	SceUInt32 old_lib_nid, old_ent_nid, new_lib_nid, new_ent_nid;

	n = args;

#define get_compat(__output__) do {res = parse_line(n, &s_nid, &n); \
	if(res < 0){ \
		return res; \
	} \
	__output__ = strtol(s_nid, NULL, 16); \
	ksceKernelFreeHeapMemory(0x1000B, s_nid); \
	} while(0)

	get_compat(old_lib_nid);
	get_compat(old_ent_nid);
	get_compat(new_lib_nid);
	get_compat(new_ent_nid);

	NIDLibStore *lib_store;
	NIDEntStore *ent_store;

	res = nid_compat_search_lib_by_new(new_lib_nid, &lib_store);
	if(res < 0){
		lib_store = ksceKernelAllocHeapMemory(0x1000B, sizeof(*lib_store));
		lib_store->next = g_nid_store;
		lib_store->new = new_lib_nid;
		lib_store->old = old_lib_nid;
		lib_store->nEnt = 0;
		lib_store->ent_root = NULL;

		g_nid_store = lib_store;
	}

	ent_store = ksceKernelAllocHeapMemory(0x1000B, sizeof(*ent_store));

	ent_store->next = lib_store->ent_root;
	ent_store->new = new_ent_nid;
	ent_store->old = old_ent_nid;

	lib_store->ent_root = ent_store;

	lib_store->nEnt += 1;

	return 0;
}

int nid_compat_free(void){

	NIDLibStore *lib_store = g_nid_store, *lib_next;
	while(lib_store != NULL){
		lib_next = lib_store->next;

		NIDEntStore *ent_store = lib_store->ent_root, *ent_next;
		while(ent_store != NULL){
			ent_next = ent_store->next;

			ksceKernelFreeHeapMemory(0x1000B, ent_store);

			ent_store = ent_next;
		}

		ksceKernelFreeHeapMemory(0x1000B, lib_store);

		lib_store = lib_next;
	}

	g_nid_store = NULL;

	return 0;
}

SceModuleLibEnt *sce_search_libent_by_nid(SceKernelLibraryDB *pLibDB, SceNID libnid){

	int prev = ksceKernelSpinlockLowLockCpuSuspendIntr(&(pLibDB->mutex));

	SceModuleLibEnt *ent = pLibDB->LibEntHead;

	while(ent != NULL){
		if(ent->Export->libnid == libnid){
			break;
		}

		ent = ent->next;
	}

	ksceKernelSpinlockLowUnlockCpuResumeIntr(&(pLibDB->mutex), prev);

	return ent;
}

void *export_find_entry_by_nid(const SceModuleExport *Export, SceNID entnid){

	for(int i=0;i<Export->entry_num_function;i++){
		if(Export->table_nid[i] == entnid){
			return Export->table_entry[i];
		}
	}

	return NULL;
}

int init_libent(const SceModuleLibEnt *sce_lib, SceModuleLibEnt *new_lib, NIDLibStore *lib_store){

	memset(new_lib, 0, sizeof(*new_lib));

	SceModuleExport *Export = ksceKernelAllocHeapMemory(0x1000B, sizeof(*Export));
	if(Export == NULL){
		return -1;
	}

	memset(Export, 0, sizeof(*Export));

	new_lib->next          = NULL;
	new_lib->data_0x04     = NULL;
	new_lib->Export        = Export;
	new_lib->syscall_info  = 0;
	new_lib->flags         = sce_lib->flags;
	new_lib->ClientCounter = 0;
	new_lib->ClientHead    = NULL;
	new_lib->libent_guid   = -1;
	new_lib->libent_puid   = -1;
	new_lib->Module        = sce_lib->Module;
	new_lib->dtrace_data0  = NULL;
	new_lib->dtrace_data1  = NULL;

	Export->size               = sizeof(*Export);
	Export->auxattr            = 0;
	Export->version            = sce_lib->Export->version;
	Export->flags              = sce_lib->Export->flags;
	Export->entry_num_function = lib_store->nEnt;
	Export->entry_num_variable = 0;
	Export->libnid             = lib_store->old;
	Export->libname            = sce_lib->Export->libname;
	Export->table_nid          = ksceKernelAllocHeapMemory(0x1000B, sizeof(SceNID) * lib_store->nEnt);
	if(Export->table_nid == NULL){
		return -1;
	}

	Export->table_entry        = ksceKernelAllocHeapMemory(0x1000B, sizeof(void *) * lib_store->nEnt);
	if(Export->table_entry == NULL){
		return -1;
	}

	NIDEntStore *ent_store = lib_store->ent_root;

	for(int i=0;i<lib_store->nEnt;i++, ent_store = ent_store->next){
		Export->table_nid[i] = ent_store->old;
		Export->table_entry[i] = export_find_entry_by_nid(sce_lib->Export, ent_store->new);
	}

	return 0;
}

int fini_libent(SceModuleLibEnt *new_lib){

	if(new_lib->Export != NULL){
		ksceKernelFreeHeapMemory(0x1000B, new_lib->Export->table_nid);
		ksceKernelFreeHeapMemory(0x1000B, new_lib->Export->table_entry);
	}

	ksceKernelFreeHeapMemory(0x1000B, new_lib->Export);
	ksceKernelFreeHeapMemory(0x1000B, new_lib);

	return 0;
}

int nid_compat_apply(void){

	int res;
	void *pThreadCB;
	void *pProcessObject;
	SceKernelLibraryDB *pLibDB;

	pThreadCB = sceKernelGetTPIDRPRW();
	if(pThreadCB == NULL){
		return -1;
	}

	pProcessObject = *(void **)(pThreadCB + 0x18);
	if(pProcessObject == NULL){
		return -1;
	}

	pLibDB = *(SceKernelLibraryDB **)(pProcessObject + 0x9C);
	if(pLibDB == NULL){
		return -1;
	}

	NIDLibStore *lib_store = g_nid_store;
	while(lib_store != NULL){

		SceModuleLibEnt *sce_lib = sce_search_libent_by_nid(pLibDB, lib_store->new);
		if(sce_lib == NULL){
			ksceKernelPrintf("cannot found library by NID 0x%08X\n", lib_store->new);
			return -1;
		}

		SceModuleLibEnt *new_lib = ksceKernelAllocHeapMemory(0x1000B, sizeof(*new_lib));
		if(new_lib == NULL){
			return -1;
		}

		res = init_libent(sce_lib, new_lib, lib_store);
		if(res < 0){
			fini_libent(new_lib);
			return res;
		}

		int prev = ksceKernelSpinlockLowLockCpuSuspendIntr(&(pLibDB->mutex));
		new_lib->next = pLibDB->LibEntHead;
		pLibDB->LibEntCounter += 1;
		pLibDB->LibEntHead = new_lib;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&(pLibDB->mutex), prev);

		ksceKernelPrintf("0x%08X %s\n", sce_lib->Export->libnid, sce_lib->Export->libname);

		lib_store = lib_store->next;
	}

	return 0;
}

int load_config(const char *script, const char *script_end){

	const char *current_script = script;

	while(current_script != script_end){

		const char *next = memchr(current_script, '\n', script_end - current_script);
		if(next == NULL){
			next = script_end - 1;
		}

		int ch = *current_script;
		if(ch != '#' && ch != '\n'){
			int len = next - current_script + 1;
			char *line = ksceKernelAllocHeapMemory(0x1000B, len + 1);
			line[len] = 0;
			memcpy(line, current_script, len);

			char *cmd, *n;

			parse_line(line, &cmd, &n);

			if(strcmp(cmd, "new_compat_func") == 0){
				new_compat(n);
			}

			ksceKernelFreeHeapMemory(0x1000B, cmd);
			ksceKernelFreeHeapMemory(0x1000B, line);
		}

		current_script = &(next[1]);
	}

	return 0;
}

int load_config_by_path(const char *path){

	int res;
	SceUID fd, memid;
	SceOff offset;
	void *base;

	fd = ksceIoOpen(path, SCE_O_RDONLY, 0);
	if(fd < 0){
		// ksceKernelPrintf("sceIoOpen 0x%X (%s)\n", fd, path);
		return 0xFFFF0001;
	}

	memid = -1;

	do {
		offset = ksceIoLseek(fd, 0LL, SCE_SEEK_END);
		if(offset < 0){
			ksceKernelPrintf("sceIoLseek 0x%X\n", (int)offset);
			res = (int)offset;
			break;
		}

		res = ksceKernelAllocMemBlock("ConfigBuffer", 0x1020D006, ((int)offset + 0xFFF) & ~0xFFF, NULL);
		if(res < 0){
			ksceKernelPrintf("sceKernelAllocMemBlock 0x%X\n", res);
			break;
		}

		memid = res;

		res = ksceKernelGetMemBlockBase(memid, &base);
		if(res < 0){
			ksceKernelPrintf("sceKernelGetMemBlockBase 0x%X\n", res);
			break;
		}

		ksceIoLseek(fd, 0LL, SCE_SEEK_SET);

		res = ksceIoRead(fd, base, (int)offset);
		if(res < 0){
			ksceKernelPrintf("sceIoRead 0x%X\n", res);
			break;
		}

		if(res != (int)offset){
			ksceKernelPrintf("sceIoRead 0x%X != 0x%X\n", res, (int)offset);
			res = -1;
			break;
		}

		ksceIoClose(fd);
		fd = -1;

		res = load_config(base, base + res);
		if(res < 0){
			ksceKernelPrintf("load_config 0x%X\n", res);
			break;
		}

		res = 0;
	} while(0);

	if(memid >= 0){
		ksceKernelFreeMemBlock(memid);
	}

	if(fd >= 0){
		ksceIoClose(fd);
	}

	return res;
}

int loader_main(SceSize arg_len, void *argp){

	int res;

	do {
		res = load_config_by_path("host0:data/nid_compat.txt");
		if(res != 0xFFFF0001){
			break;
		}

		res = load_config_by_path("sd0:/data/nid_compat.txt");
		if(res != 0xFFFF0001){
			break;
		}

		res = load_config_by_path("ux0:/data/nid_compat.txt");
		if(res != 0xFFFF0001){
			break;
		}
	} while(0);

	if(res >= 0){
		nid_compat_apply();
	}

	nid_compat_free();

	return 0;
}

void _start() __attribute__((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args){

	SceUID thid;

	do {
		thid = ksceKernelCreateThread("NIDCompatLoadThread", loader_main, 0x78, 0x1000, 0, 0, NULL);
		if(thid < 0){
			SCE_KERNEL_PRINTF_LEVEL(0, "sceKernelCreateThread 0x%X\n", thid);
			break;
		}

		ksceKernelStartThread(thid, 0, NULL);

		ksceKernelWaitThreadEnd(thid, NULL, NULL);
		ksceKernelDeleteThread(thid);
	} while(0);

	return SCE_KERNEL_START_NO_RESIDENT;
}
