// Offline patchfinder.
//
// This program exists to test the patchfinder offline (without booting into
// pongoOS), which enables quick iteration on changes and testing with
// different kernel versions.
//
// It is far from perfect, though: The callbacks in module/pf/1*/pf.c are not
// executed, so whatever result you get from pf/offline is only part of the
// story.
//
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef XNU_PF_ACCESS_32BIT
#define XNU_PF_ACCESS_32BIT 0
#endif

#define PF_DECL32(name, matches, masks, mmcount, callback, seg) \
    { \
        .pf_name = name, \
        .pf_matches = matches, \
        .pf_masks = masks, \
        .pf_mmcount = mmcount, \
        .pf_access_type = XNU_PF_ACCESS_32BIT, \
        .pf_callback = NULL /*callback*/, \
        .pf_kext = NULL, \
        .pf_segment = seg, \
        .pf_section = NULL, \
        .pf_unused = 0, \
    }

#define PF_DECL_FULL(name, matches, masks, mmcount, access, callback, kext, seg, sect) \
    { \
        .pf_name = name, \
        .pf_matches = matches, \
        .pf_masks = masks, \
        .pf_mmcount = mmcount, \
        .pf_access_type = access, \
        .pf_callback = NULL /*callback*/, \
        .pf_kext = kext, \
        .pf_segment = seg, \
        .pf_section = sect, \
        .pf_unused = 0, \
    }

#include <pf/pfs.h>

uint64_t g_sysent_addr = 0;
uint64_t g_kalloc_canblock_addr = 0;
uint64_t g_kfree_addr_addr = 0;
uint64_t g_sysctl__kern_children_addr = 0;
uint64_t g_sysctl_register_oid_addr = 0;
uint64_t g_sysctl_handle_long_addr = 0;
uint64_t g_name2oid_addr = 0;
uint64_t g_sysctl_geometry_lock_addr = 0;
uint64_t g_lck_rw_done_addr = 0;
uint64_t g_h_s_c_sbn_branch_addr = 0;
uint64_t g_h_s_c_sbn_epilogue_addr = 0;
uint64_t g_lck_grp_alloc_init_addr = 0;
uint64_t g_lck_rw_alloc_init_addr = 0;
uint64_t g_exec_scratch_space_addr = 0;
/* don't count the first opcode */
uint64_t g_exec_scratch_space_size = -sizeof(uint32_t);
uint32_t *g_ExceptionVectorsBase_stream = NULL;
uint64_t g_bcopy_phys_addr = 0;
uint64_t g_phystokv_addr = 0;
uint64_t g_copyin_addr = 0;
uint64_t g_copyout_addr = 0;
uint64_t g_IOSleep_addr = 0;
uint64_t g_kprintf_addr = 0;
uint64_t g_vm_map_unwire_addr = 0;
uint64_t g_vm_deallocate_addr = 0;
uint64_t g_kernel_map_addr = 0;
uint64_t g_kernel_thread_start_addr = 0;
uint64_t g_thread_deallocate_addr = 0;
uint64_t g_mach_make_memory_entry_64_addr = 0;
uint64_t g_offsetof_struct_thread_map = 0;
uint64_t g_current_proc_addr = 0;
uint64_t g_proc_list_lock_addr = 0;
uint64_t g_proc_ref_locked_addr = 0;
uint64_t g_proc_list_mlock_addr = 0;
uint64_t g_lck_mtx_lock_addr = 0;
uint64_t g_lck_mtx_unlock_addr = 0;
uint64_t g_proc_rele_locked_addr = 0;
uint64_t g_proc_uniqueid_addr = 0;
uint64_t g_proc_pid_addr = 0;
uint64_t g_allproc_addr = 0;
uint64_t g_lck_rw_lock_shared_addr = 0;
uint64_t g_lck_rw_lock_shared_to_exclusive_addr = 0;
uint64_t g_lck_rw_lock_exclusive_addr = 0;
uint64_t g_vm_map_wire_external_addr = 0;
uint64_t g_mach_vm_map_external_addr = 0;

/* Only for <14.5 */
uint64_t g_ipc_port_release_send_addr = 0;

/* Only for >=14.5 */
uint64_t g_ipc_port_release_send_and_unlock_addr = 0;

uint64_t g_lck_rw_free_addr = 0;
uint64_t g_lck_grp_free_addr = 0;
int g_patched_doprnt_hide_pointers = 0;
uint64_t g_copyinstr_addr = 0;
uint64_t g_thread_terminate_addr = 0;
int g_patched_pinst_set_tcr = 0;
int g_patched_all_msr_tcr_el1_x18 = 0;
uint64_t g_snprintf_addr = 0;
uint64_t g_strlen_addr = 0;
uint64_t g_proc_name_addr = 0;
uint64_t g_strncmp_addr = 0;
uint64_t g_memset_addr = 0;
uint64_t g_memmove_addr = 0;
uint64_t g_panic_addr = 0;
uint64_t g_mach_to_bsd_errno_addr = 0;
uint64_t g_xnuspy_sysctl_mib_ptr = 0;
uint64_t g_xnuspy_sysctl_mib_count_ptr = 0;
uint64_t g_xnuspy_ctl_callnum = 0;

/* Only for >=14.5 && <15.0 */
uint64_t g_io_lock_addr = 0;

uint64_t g_vm_allocate_external_addr = 0;
uint64_t g_vm_map_deallocate_addr = 0;
uint64_t g_offsetof_struct_vm_map_refcnt = 0;
uint64_t g_IOLog_addr = 0;


struct mapping {
    void *data;
    size_t size;
};

struct mapping *map_file(const char *path)
{
    struct mapping *m = malloc(sizeof(*m));
    if (!m) return NULL;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat statbuf;
    int res = fstat(fd, &statbuf);
    if (res < 0) return NULL;

    m->size = statbuf.st_size;
    m->data = mmap(NULL, (m->size + 0xffff) & ~0xffff, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m->data == MAP_FAILED) return NULL;

    close(fd);
    return m;
}

bool find_version(struct mapping *m, int *major, int *minor, int *patch)
{
    const char *needle = "Darwin Kernel Version ";

    void *eureka = memmem(m->data, m->size, needle, strlen(needle));
    if (!eureka) return false;

    sscanf(eureka + strlen(needle), "%d.%d.%d", major, minor, patch);
    return major != 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s kernel\n", argv[0]);
        exit(1);
    }

    struct mapping *m = map_file(argv[1]);
    if (!m) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        exit(1);
    }

    int major, minor, patchlevel;
    if (!find_version(m, &major, &minor, &patchlevel)) {
        fprintf(stderr, "Failed to find kernel version\n");
        exit(1);
    }

    printf("[!] Kernel version %d.%d.%d\n", major, minor, patchlevel);
    int version;
    switch (major) {
    case 19: version = 0; break;
    case 20: version = 1; break;
    case 21: version = 2; break;
    default:
        fprintf(stderr, "Unsupported kernel version\n");
        exit(1);
    }


    for (size_t pf_idx = 0; pf_idx < MAXPF; pf_idx++) {
        struct pf *pf = &g_all_pfs[pf_idx][version];
        if (!pf->pf_name)
            continue;

        printf("[?] %s\n", pf->pf_name);
        //for (int i = 0; i < pf->pf_mmcount; i++)
        //  printf("[.]   %08lx %08lx\n", pf->pf_matches[i], pf->pf_masks[i]);

        int matches = 0;
        bool matched = false;
        const uint32_t *code = m->data;
        for (size_t k = 0; k < m->size / sizeof(uint32_t); k++) {
            if ((code[k] & pf->pf_masks[matches]) == pf->pf_matches[matches]) {
                if (++matches == pf->pf_mmcount) {
                    // Print fake address that's kind of works in the kernel's __TEXT segment
                    printf("[+] > %016llx ->", k * sizeof(uint32_t) + 0xfffffff007003ff4ull);
                    for (int i = 0; i < 4; i++)
                        printf(" %08x", code[k - (matches-1) + i]);
                    printf("\n");

                    matched = true;
                    matches = 0;
                }
            } else {
                matches = 0;
            }
        }

        if (!matched)
            printf("[-] === NOPE ===\n");

    }

    return 0;
}
