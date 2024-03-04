#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>

#define MADV_PAGEOUT    21	/* reclaim these pages */

#define MAP_SIZE (100UL * 1024* 1024)

static void write_bytes(char *addr)
{
	unsigned long i;

	for (i = 0; i < MAP_SIZE; i++)
		*(addr + i) = (char)i;
}

static void read_bytes(char *addr)
{
	unsigned long i;
	unsigned long r;

	for (i = 0; i < MAP_SIZE; i++) {
		r = i; //rand() % MAP_SIZE;
		if (*(addr + r) != (char)r && *(addr + r) != 0) {
			fprintf(stderr,
				"Error: Mismatch at %lx r=%lu(%lx), *(addr + r):%lu(%lx) (char)r:%d !!!! \n",
				(unsigned long)addr + r, r, r, *(addr + r),
				*(addr + r), (char)r);
			kill(getppid(), 9);
			_exit(0);
		}
	}
}

static void *pgout_data(void *addr)
{
	while (1)
		madvise(addr, MAP_SIZE, MADV_PAGEOUT);
}

static void *dontneed_data(void *addr)
{
	while (1)
		madvise(addr, MAP_SIZE, MADV_DONTNEED);
}

static void *read_data(void *addr)
{
	while (1)
		read_bytes(addr);
}

static void *write_data(void *addr)
{
	while (1)
		write_bytes(addr);
}

int main(int argc, char **argv)
{
	int ret;
	unsigned long count;

	srand(100);

	pid_t pid = fork();
	while (1) {
		pthread_t tid1, tid2, tid3, tid4;
		if (pid < 0) {
			perror("fail to fork");
			return -1;
		}

		//parent
		if (pid != 0) {
			while (1) {
				sleep(5);
				printf("------------ test count:%ld -------------\n", count++);
				system("cat /proc/vmstat | grep swp");
				system("cat /proc/swaps");
			}
		}

		//child
		char *addr = mmap(NULL, MAP_SIZE + 128 * 1024, PROT_READ | PROT_WRITE,
			 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (addr == MAP_FAILED) {
			perror("fail to malloc");
			return -1;
		}

		addr = ((unsigned long)addr + 64UL * 1024 - 1UL) & ~(64UL * 1024 - 1);
		write_bytes(addr);

		if (pthread_create(&tid1, NULL, read_data, (void *)addr)) {
			perror("fail to pthread_create");
			return -1;
		}

		if (pthread_create(&tid2, NULL, pgout_data, (void *)addr)) {
			perror("fail to pthread_create");
			return -1;
		}

		if (pthread_create(&tid3, NULL, write_data, (void *)addr)) {
			perror("fail to pthread_create");
			return -1;
		}

		if (pthread_create(&tid4, NULL, dontneed_data, (void *)addr)) {
			perror("fail to pthread_create");
			return -1;
		}

		pthread_join(tid1, NULL);
		pthread_join(tid2, NULL);
		pthread_join(tid3, NULL);
		pthread_join(tid4, NULL);
		munmap(addr, MAP_SIZE);
	}

	return 0;
}
