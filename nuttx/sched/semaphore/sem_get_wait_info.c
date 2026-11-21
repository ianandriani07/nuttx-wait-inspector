#include <nuttx/config.h>

#include <errno.h>
#include <sys/types.h>

#include <nuttx/irq.h>
#include <nuttx/queue.h>
#include <nuttx/sched.h>
#include <nuttx/semaphore.h>

int nxsem_get_wait_info(FAR sem_t *sem, int min_priority, FAR struct sem_wait_info_s *info)
{
    struct sem_wait_info_s result = {0, 0, -1};
    FAR struct tcb_s *tcb;
    irqstate_t flags;
    int priority;

    if (sem == NULL || info == NULL ||
        min_priority < SCHED_PRIORITY_MIN ||
        min_priority > SCHED_PRIORITY_MAX)
    {
        return -EINVAL;
    }

    flags = enter_critical_section();

    for (tcb = (FAR struct tcb_s *)dq_peek(SEM_WAITLIST(sem));
         tcb != NULL;
         tcb = tcb->flink)
        {
            priority = tcb->sched_priority;

            result.total_waiters++;

            if (priority >= min_priority)
            {
                result.waiters_ge_threshold++;
            }

            if (priority > result.max_priority)
            {
                result.max_priority = priority;
            }
        }
    leave_critical_section(flags);

    *info = result;
    return OK;
}
