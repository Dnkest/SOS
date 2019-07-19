#include <cspace/cspace.h>
#include <utils/page.h>
#include <sel4/sel4_arch/mapping.h>
#include <stdbool.h>

#include "addrspace.h"
#include "frame_table.h"
#include "mapping.h"
#include "pagetable.h"
#include "elf.h"
#include "utils/kmalloc.h"

#define CAPACITY (PAGE_SIZE_4K-sizeof(struct cap_list *)-sizeof(int))/sizeof(seL4_CPtr)
#define PAGE_MASK 0xFFFFFFFFF000

struct cap_list_node {
    seL4_CPtr cap;
    struct cap_list_node *next;
};

struct cap_list {
    seL4_CPtr caps[CAPACITY];
    int size;
    struct cap_list *next;
    //struct cap_list_node *head;
};

cap_list_t *cap_list_create();
void cap_list_insert(cap_list_t *list, seL4_CPtr cap);

addrspace_t *addrspace_create()
{
    addrspace_t *addrspace = (addrspace_t *)kmalloc(sizeof(addrspace_t));
    addrspace->cap_list = cap_list_create();
    addrspace->table = pagetable_create();
    addrspace->regions = NULL;
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

frame_ref_t addrspace_lookup(addrspace_t *addrspace, seL4_Word vaddr)
{
    return pagetable_lookup(addrspace->table, vaddr);
}

seL4_Error addrspace_map_impl(addrspace_t *addrspace, cspace_t *target_cspace, seL4_CPtr target, 
                                seL4_CPtr vspace, seL4_Word vaddr, frame_ref_t frame,
                                cspace_t *source_cspace, seL4_CPtr source)
{
    //printf("%p -> %p\n", vaddr, addrspace_lookup(addrspace, vaddr));
    // if (addrspace_lookup(addrspace, vaddr) != 0) {
    //     printf("skipped\n");
    //     return 0;
    // }

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

    seL4_Word used;
    err = map_frame_cspace(target_cspace, target, vspace, vaddr, seL4_AllRights,
                            seL4_ARM_Default_VMAttributes, free_slots, &used);
    if (err) {
        printf("err%d\n", err);
        ZF_LOGE("Failed to copy cap");
    } else {
        pagetable_put(addrspace->table, vaddr, frame);
    }
    for (size_t i = 0; i < MAPPING_SLOTS; i++) {
        if (used & BIT(i)) {
            //printf("used %lu\n", i);
            cap_list_insert(list, free_slots[i]);
        } else {
            cspace_delete(target_cspace, free_slots[i]);
            cspace_free_slot(target_cspace, free_slots[i]);
        }
    }
    //printf("addr %p mappin %p used cap\n", addrspace, vaddr);
    //cap_list_insert(list, target);
    return err;
}

seL4_Error addrspace_map_one_page(addrspace_t *target_addrspace, cspace_t *target_cspace,
                                    seL4_CPtr target, seL4_CPtr vspace, seL4_Word target_vaddr,
                                    addrspace_t *source_addrspace, cspace_t *source_cspace,
                                    seL4_Word source_vaddr)
{
    frame_ref_t frame = addrspace_lookup(source_addrspace, source_vaddr);
    seL4_Error err = addrspace_map_impl(target_addrspace, target_cspace, target, 
                                vspace, target_vaddr, frame,
                                source_cspace, frame_page(frame));
    if (err) {
        cspace_delete(target_cspace, target);
        cspace_free_slot(target_cspace, target);
    }
    return err;
}

seL4_Error addrspace_map_one_frame(addrspace_t *target_addrspace, cspace_t *target_cspace,
                                    seL4_CPtr target, seL4_CPtr vspace, seL4_Word target_vaddr,
                                    frame_ref_t frame)
{
    seL4_Error err = addrspace_map_impl(target_addrspace, target_cspace, target, 
                                vspace, target_vaddr, frame,
                                frame_table_cspace(), frame_page(frame));
    if (err) {
        cspace_delete(target_cspace, target);
        cspace_free_slot(target_cspace, target);
    }
    return err;
}

seL4_Error addrspace_unmap(addrspace_t *addrspace, cspace_t *cspace,
                            seL4_CPtr frame_cap, seL4_Word vaddr)
{
    pagetable_delete(addrspace->table, vaddr);

    /* unmap our copy of the stack */
    seL4_Error err = seL4_ARM_Page_Unmap(frame_cap);
    assert(err == seL4_NoError);

    /* delete the copy of the stack frame cap */
    err = cspace_delete(cspace, frame_cap);
    assert(err == seL4_NoError);

    /* mark the slot as free */
    cspace_free_slot(cspace, frame_cap);

    return err;
}

seL4_Error addrspace_alloc_map_one_page(addrspace_t *addrspace, cspace_t *cspace, seL4_CPtr frame_cap,
                                    seL4_CPtr vspace, seL4_Word vaddr)
{
    frame_ref_t frame = alloc_frame();
    if (frame == NULL_FRAME) {
        cspace_delete(cspace, frame_cap);
        cspace_free_slot(cspace, frame_cap);
        ZF_LOGE("Couldn't allocate additional frame");
        return seL4_NotEnoughMemory;
    }

    seL4_Error err = addrspace_map_impl(addrspace, cspace, frame_cap, 
                                vspace, vaddr, frame,
                                frame_table_cspace(), frame_page(frame));
    if (err) {
        free_frame(frame);
        cspace_delete(cspace, frame_cap);
        cspace_free_slot(cspace, frame_cap);
    }
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
    assert(list != NULL);
    while (list->next != NULL) { list = list->next; }
    if (list->size == CAPACITY) {
        list->next = cap_list_create();
        list = list->next;
    }
    list->caps[list->size++] = cap;
    // struct cap_list_node *new = (struct cap_list_node *)kmalloc(sizeof(struct cap_list_node));
    // new->cap = cap;
    // struct cap_list_node *cur = list->head;
    // if (cur == NULL) {
    //     list->head = new;
    //     new->next = NULL;
    // } else {
    //     new->next = list->head;
    //     list->head = new;
    // }
}
