#include "globals.h"

static cspace_t *g_cspace = NULL;
static seL4_CPtr g_vspace = NULL;
static seL4_CPtr g_ipc_ep = NULL;

void set_global_cspace(cspace_t *cspace)
{
    g_cspace = cspace;
}

void set_global_vspace(seL4_CPtr vspace)
{
    g_vspace = vspace;
}

void set_ipc_ep(seL4_CPtr ep)
{
    g_ipc_ep = ep;
}

cspace_t *global_cspace()
{
    assert(g_cspace != NULL);
    return g_cspace;
}

seL4_CPtr global_vspace()
{
    assert(g_vspace != NULL);
    return g_vspace;
}

seL4_CPtr ipc_ep()
{
    assert(g_ipc_ep != NULL);
    return g_ipc_ep;
}
