#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

static char *lock_file = "/tmp/device.lock";

struct device
{
    int fd;
};

static int create_device(struct device *device)
{
    static char *device_path= "/tmp/device";

    device->fd = open(device_path, O_CREAT | O_RDWR | O_APPEND, 0666);
    if (-1 == device->fd)
    {
        printf("open %s failed\n", device_path);
        return -1;
    }

    return 0;
}

static void destroy_device(struct device *device)
{
    close(device->fd);
}

static void write_to_device(struct device *device, char *p_name, int wr_cycles)
{
    int len;
    
    len = snprintf(NULL, 0, "%s: %d\n", p_name, wr_cycles) + 1;
    char buf[len];
    snprintf(buf, len, "%s: %d\n", p_name, wr_cycles);

    int written = write(device->fd, buf, len);
    if (-1 == written)
    {
        printf("Could not write to device %s\n", p_name);
        return;
    }
   
    usleep(420);

    printf("%s", buf);
}

static unsigned int get_interval_ms(unsigned int min, unsigned int max)
{
    struct timespec time;
    
    clock_gettime(CLOCK_MONOTONIC, &time);

    srand((unsigned) time.tv_nsec);

    return rand() % (max + 1 - min) + min;
}

static void process_run(char *p_name, int wr_cycles)
{
    struct device my_device;
    int lock_fd;
    unsigned int interval_ms;

    interval_ms = get_interval_ms(1, 10);

    printf("Starting process %s\ninterval = %u\n", p_name, interval_ms);

    lock_fd = open(lock_file, O_CREAT | O_RDONLY, 0666);
    if (-1 == lock_fd)
    {
        printf("open %s failed\n", lock_file);
        return;
    }
    
    if (create_device(&my_device))
    {
        printf("creating device failed");
        return;
    }
    
    while (wr_cycles--)
    {
        if (flock(lock_fd, LOCK_EX | LOCK_NB))
        {
            printf("%s blocked\n", p_name);
            
            if (-1 == flock(lock_fd, LOCK_EX))
            {
                printf("%s: flock failed: %s\n", p_name, strerror(errno));
                return;
            }
        }

        write_to_device(&my_device, p_name, wr_cycles);
        
        flock(lock_fd, LOCK_UN);

        usleep(interval_ms * 1000);
    }

    destroy_device(&my_device);
    
    close(lock_fd);
}

int main(int argc, char **argv)
{
    if (fork() == 0)
    {
        process_run("A", 10);
    }
    else
    {
        process_run("B", 10);
    }

    return 0;
}
