
#ifndef _PSP2_KERNEL_MODULEMGR_INTERNAL_H_
#define _PSP2_KERNEL_MODULEMGR_INTERNAL_H_


#include <psp2kern/types.h>


typedef struct SceModuleLibEnt SceModuleLibEnt;
typedef struct SceModuleCB SceModuleCB;

typedef int (* SceKernelModuleEntry)(SceSize args, void *argp);
typedef int (* SceKernelModuleDebugCallback)(SceModuleCB *pModuleCB);


typedef struct SceModuleImportOld { // size is 0x34-bytes
	SceUInt8 size;
	SceUInt8 auxattr;
	uint16_t version;
	uint16_t flags;
	uint16_t entry_num_function;
	uint16_t entry_num_variable;
	uint16_t entry_num_tls;
	uint32_t rsvd1;
	uint32_t libnid;
	const char *libname;
	uint32_t rsvd2;
	uint32_t *table_func_nid;
	void    **table_function;
	uint32_t *table_vars_nid;
	void    **table_variable;
	uint32_t *table_tls_nid;
	void    **table_tls;
} SceModuleImportOld;

typedef struct SceModuleImport { // size is 0x24-bytes
	SceUInt8 size;
	SceUInt8 auxattr;
	uint16_t version;
	uint16_t flags;
	uint16_t entry_num_function;
	uint16_t entry_num_variable;
	uint16_t data_0x0A; // unused?
	SceNID libnid;
	const char *libname;
	SceNID *table_func_nid;
	void  **table_function;
	SceNID *table_vars_nid;
	void  **table_variable;
} SceModuleImport;

typedef struct SceModuleExport { // size is 0x20-bytes
	SceUInt8    size;
	SceUInt8    auxattr;
	SceUInt16   version;
	SceUInt16   flags; // 0x4000:user export
	SceUInt16   entry_num_function;
	SceUInt16   entry_num_variable;
	SceUInt16   data_0x0A; // unused?
	SceUInt8    data_0x0C; // TLS size index
	SceUInt8    data_0x0D[3];
	SceNID      libnid;
	const char *libname;
	SceNID     *table_nid;
	ScePVoid   *table_entry;
} SceModuleExport;

typedef struct SceModuleClient { // size is 0x18-bytes
	struct SceModuleClient *next;
	SceUID stubid;
	SceModuleImport *Import;
	SceModuleLibEnt *LibEnt;
	SceModuleCB *Module;
	ScePVoid stubtbl; // Required the dipsw 0xD2. stub pointer + stub nids
} SceModuleClient;

typedef struct SceModuleLibEnt { // size is 0x2C-bytes
	struct SceModuleLibEnt *next;
	struct SceModuleLibEnt *data_0x04; // maybe
	SceModuleExport *Export;

	/*
	 * (syscall_idx &  0xFFF): syscall index
	 * (syscall_idx & 0x1000): syscall flag
	 * (syscall_idx == 0)    : kernel export
	 */
	SceUInt16 syscall_info;
	SceUInt16 flags;

	SceUInt32 ClientCounter;
	SceModuleClient *ClientHead;
	SceUID libent_guid;
	SceUID libent_puid;
	SceModuleCB *Module;
	ScePVoid dtrace_data0;
	ScePVoid dtrace_data1;
} SceModuleLibEnt;

typedef struct SceModuleSharedHost { // size is 0x10-bytes
	struct SceModuleSharedHost *next;
	SceModuleCB *Module;
	SceUInt32 ClientCounter;
	void *CachedDataSegment;
} SceModuleSharedHost;

typedef struct SceModuleSegment { // size is 0x14
	SceSize filesz;
	SceSize memsz;
	uint8_t perms[4];
	void *vaddr;
	SceUID memblk_id;
} SceModuleSegment;

typedef struct SceModuleDebugPointExportsInfo { // size is 0x8 bytes on FWs 0.931-3.73
	SceUInt32 version;    // ex: 1
	SceUInt32 numEntries; // ex: 1, 5, 7, 16
} SceModuleDebugPointExportsInfo;

typedef struct SceModuleDebugPointExport { // size is 0x28-bytes on FWs 0.931-3.73
	void *unk_0x0;            // maybe pointer to a structure or could be a SceUInt32
	const char *type_name;    // "__proc_", "__interrupt_", "__sched_"
	const char *command_name; // Name of the operation. ex: "exec__failure
	const void *addr;         // Pointer to the exported instructions
	void *storage;            // Can be used in the exported instructions
	void *unk_0x14;           // Seems unused
	void *unk_0x18;           // Seems unused
	void *unk_0x1C;           // Seems unused
	void *unk_0x20;           // Seems unused
	void *unk_0x24;           // Seems unused
} SceModuleDebugPointExport;

typedef struct SceModuleEntryCallParam { // size is 0x14-bytes
	SceUInt32 version; // should be 4
	SceUInt32 threadPriority;
	SceUInt32 threadStackSize;
	SceUInt32 data_0x0C;
	SceUInt32 cpuAffinityMask;
} SceModuleEntryCallParam;

typedef struct SceModuleCB { // size is 0xE8-bytes
	struct SceModuleCB *next;
	SceUInt16 flags;
	SceUInt8 status;
	SceUInt8 data_0x07;
	SceUInt32 module_sdk_version;
	SceUID module_guid;
	SceUID module_puid;
	SceUID pid;
	SceUInt16 attr;
	SceUInt8 minor;
	SceUInt8 major;
	char *module_name;

	struct {
		void *libent_top;
		void *libent_btm;
		void *libstub_top;
		void *libstub_btm;
		SceUInt32 fingerprint;
		void *tlsInit;
		SceSize tlsInitSize;
		SceSize tlsAreaSize;
		void *exidxTop;
		void *exidxBtm;
		void *extabTop;
		void *extabBtm;
	};

	SceUInt16 ExportCounter;
	SceUInt16 ImportCounter;
	ScePVoid WorkPool;
	SceModuleExport *Export; // for kernel access
	SceModuleLibEnt *LibEntVector;
	SceModuleImport *Import; // for kernel access
	SceModuleClient *ClientVector;

	struct { // size is 0x5C-bytes
		char *path;
		SceInt32 SegmentCounter;
		SceModuleSegment segment[3];
		int data_0xAC;
		int data_0xB0;
		SceKernelModuleEntry module_start;
		SceKernelModuleEntry module_stop;
		SceKernelModuleEntry module_exit;
		SceKernelModuleEntry module_bootstart;
	};

	void *module_proc_param;
	const SceModuleEntryCallParam *module_start_thread_param; // noname export. NID:0x1A9822A4
	const SceModuleEntryCallParam *module_stop_thread_param; // noname export. NID:0xD20886EB
	void *arm_exidx; // need dipsw 0xD2
	SceModuleSharedHost *SharedHost;
	int data_0xD8;
	SceModuleDebugPointExportsInfo *pDbgPointInfo;
	SceModuleDebugPointExport **pDbgPointList; // noname export. NID:0x8CE938B1
	int data_0xE4;
} SceModuleCB;

typedef struct SceUIDModuleObject { // size is 0xF4-bytes
	uint32_t sce_reserved[2];
	SceModuleCB moduleCB;
	int data_0xE8;
} SceUIDModuleObject;

typedef struct SceUIDLibraryObject { // size is 0x10-bytes
	uint32_t sce_reserved[2];
	SceModuleLibEnt *pLibEnt;
	SceUID module_guid;
} SceUIDLibraryObject;

typedef struct SceUIDLibStubObject { // size is 0x10-bytes
	uint32_t sce_reserved[2];
	SceNID libnid;
	SceUInt16 importIndex;
	SceUInt16 unk;
} SceUIDLibStubObject;

typedef struct SceAddr2ModCB { // size is 0x14-bytes
	void *Addr2ModTbl;
	SceUID memid_Addr2ModTbl;
	SceUID memid_A2ExidxTmpUser;
	SceUID memid_A2ExidxTmpKern;
	void *A2ExidxTmp;
} SceAddr2ModCB;

typedef struct SceKernelLibraryDB { // size is 0x24-bytes
	SceUID pid;
	SceModuleLibEnt *LibEntHead;
	SceUInt16 LibEntCounter;
	SceUInt16 LostClientCounter;
	SceModuleClient *LostClientHead;
	SceModuleCB *ModuleHead;
	SceUID ModuleId;
	SceUInt16 ModuleCounter;
	SceUInt16 flags;
	SceAddr2ModCB *Addr2ModTbls;
	SceUInt32 mutex;
} SceKernelLibraryDB;


#endif /* _PSP2_KERNEL_MODULEMGR_INTERNAL_H_ */
