#include <nuttx/config.h>

#include <errno.h>

#include <nuttx/semaphore.h>

int sem_get_wait_info(FAR sem_t *sem, int min_priority, FAR struct sem_wait_info_s *info)
{
    int ret;

    ret = nxsem_get_wait_info(sem, min_priority, info);
    if (ret < 0)
    {
        set_errno(-ret);
        ret = ERROR;
    }

    return ret;
}
