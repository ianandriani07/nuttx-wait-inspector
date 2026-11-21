#include <nuttx/config.h>

#include <errno.h>
#include <inttypes.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

static FAR void *waiter(FAR void *arg)
{
    FAR sem_t *sem = arg;
    int ret;

    do
    {
        ret = sem_wait(sem);
    } while (ret < 0 && errno == EINTR);

    return ret == 0 ? NULL : (FAR void *)1;
}

static int create_waiter(FAR pthread_t *thread, FAR sem_t *sem, int priority)
{
    pthread_attr_t attr;
    struct sched_param param = {0};
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret != 0)
        {
            return ret;
        }

    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    if (ret == 0)
        {
            ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        }

    if (ret == 0)
        {
            param.sched_priority = priority;
            ret = pthread_attr_setschedparam(&attr, &param);
        }

    if (ret == 0)
        {
            ret = pthread_create(thread, &attr, waiter, sem);
        }

    pthread_attr_destroy(&attr);
    return ret;
}

static int check_info(FAR sem_t *sem, int threshold,
                      uint32_t expected_total, uint32_t expected_ge, int expected_max)
{
    struct sem_wait_info_s info = {123, 456, 789};
    int failed;
    int ret;

    ret = sem_get_wait_info(sem, threshold, &info);
    if (ret != 0)
        {
            printf("FAIL: threshold=%d returned %d, errno=%d\n", threshold, ret, errno);
            return 1;
        }

    failed = info.total_waiters != expected_total ||
            info.waiters_ge_threshold != expected_ge ||
            info.max_priority != expected_max;

    printf("%s: threshold=%d total=%" PRIu32
         " ge=%" PRIu32 " max=%d\n",
         failed ? "FAIL" : "PASS", threshold,
         info.total_waiters, info.waiters_ge_threshold,
         info.max_priority);

    return failed;
}

static int check_invalid(FAR const char *name, FAR sem_t *sem,
                         int threshold, FAR struct sem_wait_info_s *info)
{
    struct sem_wait_info_s before = {0, 0 ,0};
    int error;
    int ret;

    if (info != NULL)
        {
            before = *info;
        }

    errno = 0;
    ret = sem_get_wait_info(sem, threshold, info);
    error = errno;

    if (ret != -1 || error != EINVAL)
        {
            printf("FAIL: %s returned %d, errno=%d\n", name, ret, error);
            return 1;
        }

    if (info != NULL &&
        (info->total_waiters != before.total_waiters ||
        info->waiters_ge_threshold != before.waiters_ge_threshold ||
        info->max_priority != before.max_priority))
        {
            printf("FAIL: %s changed the result buffer\n", name);
            return 1;
        }

    printf("PASS: %s\n", name);
    return 0;
}

static int check_waiters(FAR sem_t *sem)
{
    const int priorities[3] = {110, 120, 130};
    pthread_t threads[3];
    FAR void *result;
    int created = 0;
    int failures = 0;
    int ready = 0;
    int value = 0;
    int attempt;
    int ret;
    int i;

    if (sem_setprotocol(sem, SEM_PRIO_NONE) != 0)
        {
            perror("sem_setprotocol");
            return 1;
        }

    for (i = 0; i < 3; i++)
        {
            ret = create_waiter(&threads[i], sem, priorities[i]);
            if (ret != 0)
                {
                printf("FAIL: create priority %d, error=%d\n",
                        priorities[i], ret);
                failures++;
                break;
                }

            created++;
        }

    if (created == 3)
        {
            for (attempt = 0; attempt < 200; attempt++)
                {
                if (sem_getvalue(sem, &value) != 0)
                    {
                    perror("sem_getvalue");
                    break;
                    }

                if (value == -3)
                    {
                    ready = 1;
                    break;
                    }

                usleep(10000);
                }

            if (ready)
                {
                    failures += check_info(sem, 1,   3, 3, 130);
                    failures += check_info(sem, 110, 3, 3, 130);
                    failures += check_info(sem, 120, 3, 2, 130);
                    failures += check_info(sem, 130, 3, 1, 130);
                    failures += check_info(sem, 131, 3, 0, 130);
                    failures += check_info(sem, 255, 3, 0, 130);

                ret = sem_getvalue(sem, &value);
                if (ret != 0 || value != -3)
                    {
                        printf("FAIL: inspection changed semaphore state\n");
                        failures++;
                    }
                }
            else
                {
                    printf("FAIL: expected three blocked waiters\n");
                    failures++;
                }
        }

    for (i = 0; i < created; i++)
        {
            if (sem_post(sem) != 0)
                {
                    perror("sem_post");
                    failures++;
                }
        }

    for (i = 0; i < created; i++)
        {
            ret = pthread_join(threads[i], &result);
            if (ret != 0)
                {
                    printf("FAIL: pthread_join[%d], error=%d\n", i, ret);
                    failures++;
                }
            else if (result != NULL)
                {
                    printf("FAIL: waiter[%d] could not acquire semaphore\n", i);
                    failures++;
                }
        }

    failures += check_info(sem, 120, 0, 0, -1);
    return failures;
}

int main(int argc, FAR char *argv[])
{
    struct sem_wait_info_s info = {123, 456, 789};
    sem_t sem;
    int failures = 0;

    if (sem_init(&sem, 0, 0) != 0)
        {
            perror("sem_init");
            return EXIT_FAILURE;
        }

    failures += check_info(&sem, 1, 0, 0, -1);
    failures += check_info(&sem, 255, 0, 0, -1);

    failures += check_invalid("NULL semaphore", NULL, 100, &info);
    failures += check_invalid("NULL result", &sem, 100, NULL);
    failures += check_invalid("threshold 0", &sem, 0, &info);
    failures += check_invalid("threshold 256", &sem, 256, &info);

    failures += check_waiters(&sem);

    if (sem_destroy(&sem) != 0)
        {
            perror("sem_destroy");
            failures++;
        }

    printf("wait_inspector: %s (%d failures)\n",
            failures == 0 ? "PASS" : "FAIL", failures);

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
