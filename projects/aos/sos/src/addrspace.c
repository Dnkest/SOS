#include <cspace/cspace.h>
#include <utils/page.h>
#include <sel4/sel4_arch/mapping.h>
#include <stdbool.h>
#include <stdio.h>

#include "addrspace.h"
#include "frame_table.h"
#include "mapping.h"
#include "pagetable.h"
#include "elf.h"
#include "utils/kmalloc.h"
#include "vframe_table.h"

#define CAPACITY (PAGE_SIZE_4K-sizeof(struct cap_list *)-sizeof(int))/sizeof(seL4_CPtr)
#define PAGE_MASK 0xFFFFFFFFF000

struct cap_list {
    seL4_CPtr caps[CAPACITY];
    int size;
    struct cap_list *next;
};

cap_list_t *cap_list_create();
void cap_list_insert(cap_list_t *list, seL4_CPtr cap);
void cap_list_destroy(cap_list_t *list);

void addrspace_destory(addrspace_t *addrspace)
{
//printf("a1\n");
    pagetable_destroy(addrspace->table);
//printf("a2\n");
    cap_list_destroy(addrspace->cap_list);
//printf("a3\n");
    region_t *cur = addrspace->regions, *tmp;

    while (cur != NULL) {
        tmp = cur;
        cur = cur->next;
        kfree(tmp);
    }
    kfree(addrspace);
}

addrspace_t *addrspace_create()
{
    addrspace_t *addrspace = (addrspace_t *)kmalloc(sizeof(addrspace_t));
    addrspace->cap_list = cap_list_create();
    addrspace->table = pagetable_create();
    addrspace->regions = NULL;
    addrspace->pages = 0;
    return addrspace;
}

void addrspace_define_region(addrspace_t *addrspace, seL4_Word vaddr,
                        size_t memsize, unsigned long permissions)
{
    region_t *region = (region_t *)kmalloc(sizeof(region_t));

    region->base = vaddr & PAGE_MASK;
    region->top = ((vaddr + memsize) & PAGE_MASK);
    if (memsize != (memsize & PAGE_MASK)) {
        region->top += BIT(seL4_PageBits);
    }
    region->read =  permissions & PF_R || permissions & PF_X;
    region->write = permissions & PF_W;
    region->next = NULL;
    printf("Defined region: %p-->%p, %d, %d\n",region->base,region->top,region->read ,region->write);

    if (addrspace->regions == NULL) {
        addrspace->regions = region;
    } else {
        region_t *cur = addrspace->regions, *prev;
        while (cur != NULL) {
            prev = cur;

            cur = cur->next;
        }
        prev->next = region;
    }

}

bool addrspace_check_valid_region(addrspace_t *addrspace, seL4_Word fault_address)
{
    if (fault_address == 0) {
        return false;
    }
    region_t *cur = addrspace->regions;
    while (cur != NULL) {
        //printf("check region: %p-->%p, %d, %d\n",cur->base,cur->top,cur->read ,cur->write);
        if (fault_address >= cur->base && fault_address < cur->top) {
            return true;
        }
        cur = cur->next;
    }
    return false;
}

vframe_ref_t addrspace_lookup_vframe(addrspace_t *addrspace, seL4_Word vaddr)
{
    return pagetable_lookup(addrspace->table, vaddr);
}

int addrspace_vaddr_exists(addrspace_t *addrspace, seL4_Word vaddr)
{
    return (pagetable_lookup(addrspace->table, vaddr) != 0);
}

seL4_Error addrspace_map_impl(addrspace_t *addrspace, cspace_t *target_cspace, seL4_CPtr target, 
                                seL4_CPtr vspace, seL4_Word vaddr, vframe_ref_t vframe,
                                cspace_t *source_cspace, seL4_CPtr source)
{
    cap_list_t *list = addrspace->cap_list;

    seL4_Error err = cspace_copy(target_cspace, target, source_cspace, source, seL4_AllRights);
    if (err != seL4_NoError) {
        ZF_LOGE("Failed to copy cap");
        return err;
    }

    seL4_CPtr free_slots[MAPPING_SLOTS];
    for (size_t i = 0; i < MAPPING_SLOTS; i++) {
        free_slots[i] = cspace_alloc_slot(target_cspace);
        if (free_slots[i]== seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for frame");
            return seL4_NotEnoughMemory;
        }
    }

    seL4_Word used[3];
    for (int i = 0; i < 3; i++) { used[i] = 0; }
    err = map_frame_cspace(target_cspace, target, vspace, vaddr, seL4_AllRights,
                            seL4_ARM_Default_VMAttributes, free_slots, used);
    if (err) {
        printf("err%d\n", err);
        ZF_LOGE("Failed to copy cap");
    } else {
        pagetable_put(addrspace->table, vaddr, vframe);
    }
    for (size_t i = 0; i < MAPPING_SLOTS; i++) {
        if (used[i]) {
            //printf("used %d %p\n", i, free_slots[i]);
            cap_list_insert(list, free_slots[i]);
        } else {
            cspace_delete(target_cspace, free_slots[i]);
            cspace_free_slot(target_cspace, free_slots[i]);
        }
    }
    return err;
}

seL4_Error addrspace_alloc_map_one_page(addrspace_t *addrspace, cspace_t *cspace, seL4_CPtr frame_cap,
                                    seL4_CPtr vspace, seL4_Word vaddr)
{
    //printf("allocating %p, %p\n", vspace, vaddr);
    vframe_ref_t vframe = alloc_vframe(frame_cap, vaddr, vspace);
    //printf("a done\n");
    frame_ref_t frame = frame_from_vframe(vframe);

    seL4_Error err = addrspace_map_impl(addrspace, cspace, frame_cap, 
                                vspace, vaddr, vframe,
                                frame_table_cspace(), frame_page(frame));
    if (err) {
        cspace_delete(cspace, frame_cap);
        cspace_free_slot(cspace, frame_cap);
    }
    addrspace->pages++;
    return err;
}

cap_list_t *cap_list_create()
{
    cap_list_t *list = (cap_list_t *)kmalloc(sizeof(cap_list_t));
    list->size = 0;
    list->next = NULL;
    return list;
}

void cap_list_insert(cap_list_t *list, seL4_CPtr cap)
{
    while (list != NULL && list->next != NULL) { list = list->next; }
    if (list->size == CAPACITY) {
        list->next = cap_list_create();
        list = list->next;
    }
    list->caps[list->size++] = cap;
}

void cap_list_destroy(cap_list_t *list)
{
    cap_list_t *t;
    while (list != NULL) {
        for (int i = 0; i < list->size; i++) {
            //printf("cap %p\n", list->caps[i]);
            seL4_ARM_Page_Unmap(list->caps[i]);
            cspace_delete(global_cspace(), list->caps[i]);
            cspace_free_slot(global_cspace(), list->caps[i]);
        }
        t = list;
        list = list->next;
        kfree(t);
    }
}
