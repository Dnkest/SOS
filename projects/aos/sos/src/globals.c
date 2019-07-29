#include "globals.h"

static cspace_t *g_cspace = NULL;
static seL4_CPtr g_vspace = NULL;

void set_global_cspace(cspace_t *cspace)
{
    g_cspace = cspace;
}

void set_global_vspace(seL4_CPtr vspace)
{
    g_vspace = vspace;
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
