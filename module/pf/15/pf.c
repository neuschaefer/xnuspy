#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include <pongo.h>

#include <asm/asm.h>
#include <common/common.h>
#include <pf/offsets.h>
#include <pf/pf_common.h>

uint64_t g_vm_map_unwire_nested_addr = 0;
uint64_t g_proc_ref_addr = 0;
uint64_t g_proc_rele_addr = 0;
uint64_t g_ipc_object_lock_addr = 0;

/* Confirmed working 15.0 - 15.8 */
bool ipc_port_release_send_finder_15(xnu_pf_patch_t *patch, 
        void *cacheable_stream){
    /* We land in IOUserClient::releaseNotificationPort, which does almost
     * nothing but call ipc_object_lock and ipc_port_release_send_and_unlock */
    uint32_t *opcode_stream = cacheable_stream;
    xnu_pf_disable_patch(patch);

    uint32_t *ipc_object_lock = get_branch_dst_ptr(opcode_stream + 1);
    uint32_t *ipc_port_release_send_and_unlock = get_branch_dst_ptr(opcode_stream + 3);

    g_ipc_port_release_send_and_unlock_addr = xnu_ptr_to_va(ipc_port_release_send_and_unlock);
    g_ipc_object_lock_addr = xnu_ptr_to_va(ipc_object_lock);

    puts("xnuspy: found ipc_port_release_send_and_unlock");
    puts("xnuspy: found ipc_object_lock");

    return true;
}

/* Confirmed working 15.0 */
bool proc_name_snprintf_strlen_finder_15(xnu_pf_patch_t *patch,
        void *cacheable_stream){
    /* will land either in AppleEmbeddedUSBDevice::setAuthenticationProperites
     * or AppleEmbeddedUSBDevice::setProperties, both look the same, so
     * just use the one we land in first */
    xnu_pf_disable_patch(patch);

    uint32_t *opcode_stream = cacheable_stream;

    uint32_t *snprintf = get_branch_dst_ptr(opcode_stream + 2);
    uint32_t *strlen = get_branch_dst_ptr(opcode_stream + 4);
    uint32_t *proc_name = get_branch_dst_ptr(opcode_stream + 8);

    g_snprintf_addr = xnu_ptr_to_va(snprintf);
    g_strlen_addr = xnu_ptr_to_va(strlen);
    g_proc_name_addr = xnu_ptr_to_va(proc_name);

    puts("xnuspy: found snprintf");
    puts("xnuspy: found strlen");
    puts("xnuspy: found proc_name");

    return true;
}

/* Confirmed working 15.0-15.8 */
bool current_proc_finder_15(xnu_pf_patch_t *patch, void *cacheable_stream){
    /* The next instruction after the pattern is a call to current_proc */
    xnu_pf_disable_patch(patch);

    uint32_t *opcode_stream = cacheable_stream;

    uint32_t *current_proc = get_branch_dst_ptr(opcode_stream + 8);

    g_current_proc_addr = xnu_ptr_to_va(current_proc);

    puts("xnuspy: found current_proc");

    return true;
}

/* Confirmed working 15.0 */
bool vm_map_unwire_nested_finder_15(xnu_pf_patch_t *patch, 
        void *cacheable_stream){
    /* This matches five places. In three of them, we'll be sitting on a
     * BL to vm_map_unwire_nested if the instruction right behind us is
     * mov x5, #0 */
    uint32_t *opcode_stream = cacheable_stream;

    if(opcode_stream[-1] != 0xd2800005)
        return false;

    xnu_pf_disable_patch(patch);

    uint32_t *vm_map_unwire_nested = get_branch_dst_ptr(opcode_stream);

    g_vm_map_unwire_nested_addr = xnu_ptr_to_va(vm_map_unwire_nested);
    
    puts("xnuspy: found vm_map_unwire_nested");

    return true;
}

/* Confirmed working 15.0-15.8 */
bool kernel_map_finder_15(xnu_pf_patch_t *patch, void *cacheable_stream){
    /* Will at a sequence of four calls to _vm_map_page_mask.
     * Just prior to the calls, _kernel_map is loaded into x0:
     *   adrp    x26, 0xfffffff006d4e000
     *   ldr     x26, [x26, #0x460]
     *   ldr     x0,[x26]
     */
    uint32_t *opcode_stream = cacheable_stream;
    xnu_pf_disable_patch(patch);

    /* get constant pool entry */
    uint64_t *kernel_mapp = (uint64_t *)get_pc_rel_target(opcode_stream - 2);

    /* get address in constant pool entry (already a kernel virtual address) */
    g_kernel_map_addr = *kernel_mapp + kernel_slide;
    
    puts("xnuspy: found kernel_map");
    
    return true;
}

/* Confirmed working 15.0, 15.5 - 15.8 */
bool vm_deallocate_finder_15(xnu_pf_patch_t *patch, void *cacheable_stream){
    xnu_pf_disable_patch(patch);

    uint32_t *opcode_stream = cacheable_stream;

    uint32_t *vm_deallocate = get_branch_dst_ptr(opcode_stream + 0);

    g_vm_deallocate_addr = xnu_ptr_to_va(vm_deallocate);

    puts("xnuspy: found vm_deallocate");

    return true;
}

/* Confirmed working 15.0 */
bool proc_list_mlock_lck_mtx_lock_unlock_finder_15(xnu_pf_patch_t *patch,
        void *cacheable_stream){
    /* This gets three hits, but they are identical for
     * all kernels. we can only search for 8 instructions
     * at a time, so we check for the 9th instruction
     * (bl _lck_mtx_unlock). We are sitting on an adrp or
     * adr to proc_list_mlock, and then the third instruction
     * from this point is a BL to lck_mtx_lock */
    xnu_pf_disable_patch(patch);
    
    uint32_t *opcode_stream = cacheable_stream;
    
    /* NOT a double pointer */
    uint64_t *proc_list_mlock = (uint64_t *)get_pc_rel_target(opcode_stream);

    uint32_t *lck_mtx_lock = get_branch_dst_ptr(opcode_stream + 3);
    uint32_t *lck_mtx_unlock = get_branch_dst_ptr(opcode_stream + 8);

    g_proc_list_mlock_addr = xnu_ptr_to_va(proc_list_mlock);
    g_lck_mtx_lock_addr = xnu_ptr_to_va(lck_mtx_lock);
    g_lck_mtx_unlock_addr = xnu_ptr_to_va(lck_mtx_unlock);

    puts("xnuspy: found proc_list_mlock");
    puts("xnuspy: found lck_mtx_lock");
    puts("xnuspy: found lck_mtx_unlock");

    return true;
}

/* Confirmed working 15.0 */
bool lck_grp_free_finder_15(xnu_pf_patch_t *patch, void *cacheable_stream){
    /* We landed in lifs_kext_stop. There's three sequences of
     * ldr x0, ... BL _lck_grp_free in front of us */
    xnu_pf_disable_patch(patch);

    uint32_t *opcode_stream = cacheable_stream;

    uint32_t *lck_grp_free = get_branch_dst_ptr(opcode_stream + 1);
    
    g_lck_grp_free_addr = xnu_ptr_to_va(lck_grp_free);

    puts("xnuspy: found lck_grp_free");

    return true;
}

/* Confirmed working 15.0 - 15.8*/
bool proc_ref_rele_finder_15(xnu_pf_patch_t *patch, void *cacheable_stream){
    /* We landed inside memorystatus_update_vm_pressure, at one of two
     * equivalent pieces of code, and use the first one we find. */
    xnu_pf_disable_patch(patch);

    uint32_t *opcode_stream = cacheable_stream;

    uint32_t *proc_ref = get_branch_dst_ptr(opcode_stream + 9);
    uint32_t *proc_rele = get_branch_dst_ptr(opcode_stream + 1);

    g_proc_ref_addr = xnu_ptr_to_va(proc_ref);
    g_proc_rele_addr = xnu_ptr_to_va(proc_rele);

    puts("xnuspy: found proc_ref");
    puts("xnuspy: found proc_rele");

    return true;
}

/* Confirmed working 15.0 - 15.8 */
bool lck_rw_alloc_init_finder_15(xnu_pf_patch_t *patch,
        void *cacheable_stream){
    /* We landed inside OSSymbol::initialize. */
    xnu_pf_disable_patch(patch);

    uint32_t *opcode_stream = cacheable_stream;
    uint32_t *lck_rw_alloc_init = get_branch_dst_ptr(opcode_stream + 2);

    g_lck_rw_alloc_init_addr = xnu_ptr_to_va(lck_rw_alloc_init);

    puts("xnuspy: found lck_rw_alloc_init");

    return true;
}

/* Confirmed working 15.0 - 15.8 */
bool vm_allocate_external_finder_15(xnu_pf_patch_t *patch,
        void *cacheable_stream){
    /* We landed somewhere in IONetworkingFamily */
    xnu_pf_disable_patch(patch);

    uint32_t *opcode_stream = cacheable_stream;
    uint32_t *vm_allocate = get_branch_dst_ptr(opcode_stream + 7);

    g_vm_allocate_external_addr = xnu_ptr_to_va(vm_allocate);

    puts("xnuspy: found vm_allocate_external");

    return true;
}
