#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <math.h>


int main() {
	for(int i=0; i < 1000000000; i++) {
		int j = 12;
		int k = 14;
	}
	printf("FINISH");
	return(0);

}


