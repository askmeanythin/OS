/* allocation.h - smart student-to-PC allocation */
#ifndef SEL_ALLOCATION_H
#define SEL_ALLOCATION_H

#include "common.h"

typedef enum {
    ALLOC_OK = 0,
    ALLOC_ERR_STUDENT_UNKNOWN,
    ALLOC_ERR_EXAM_UNKNOWN,
    ALLOC_ERR_EXAM_NOT_ACTIVE,
    ALLOC_ERR_STUDENT_ALREADY_ASSIGNED,
    ALLOC_ERR_NO_PC_AVAILABLE,
    ALLOC_ERR_DB
} AllocResult;

/* Deterministic policy (documented in docs/architecture.md):
 * among READY, fresh, unassigned PCs pick the lowest (cpu+ram) score,
 * ties broken by pc_id ascending. Executed inside BEGIN IMMEDIATE so two
 * concurrent requests can never take the same PC; partial unique indexes
 * in the schema are a second line of defence. */
AllocResult allocation_allocate(const char *exam_id, const char *student_id,
                                const HealthThresholds *th,
                                char *out_pc_id, int out_pc_sz,
                                char *err, int err_sz);
int allocation_release(const char *exam_id, const char *student_id); /* 0 ok */
int allocation_release_pc(const char *pc_id);

#endif
