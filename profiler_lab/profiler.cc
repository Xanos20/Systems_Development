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
#include <stdint.h>
#include <pthread.h>
#include <inttypes.h>
#include "inspect.h"

#include <iostream>
#include <map>
//using namespace std;



// TODO: WRITE program that loops 10^9 times
// then start reading samples from ring buffer

#define PAGE_SIZE 4096
#define RING_BUFFER_SIZE (PAGE_SIZE*8)

/*
  NOTE: Does not run on MacOS/OSX/Windows
*/

static long perf_event_open(struct perf_event_attr* hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}
/*
struct Node {
  void* mapped;
  struct perf_event_mmap_page* first_page;
  pid_t* pid;
  //uint32_t* tid;
  struct Node* next;
};

struct RING_BUFFERS {
  struct Node* next;
};
*/

struct Node {
  void* mapped;
  struct perf_event_mmap_page* first_page;
};

//struct RING_BUFFERS BUFFERS;
/*
std::map<char*, size_t> function_map;
std::map<char*, size_t>:: iterator iter;
*/

std::map<std::string,int> mymap;
std::map<std::string,int>::iterator it;


std::map<std::string, struct Node> BUFFERS;
std::map<std::string, struct Node>::iterator buff_iter;



/*
void new_ring_buffer(Node* curr, uint32_t* pid, uint32_t* tid, int perf_fd) {
  size_t mmap_size = 9*PAGE_SIZE;
  void* mapped = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, perf_fd, 0);
  if(mapped == ((void*) -1)) {
    perror("MMAP Failed.");
    exit(2);
  }
  curr->mapped = mapped;
  curr->first_page = (struct perf_event_mmap_page*) (curr->mapped);
  curr->next = NULL;
  curr->pid = pid;
  if(BUFFERS.next == NULL) {
    curr->tid = pid;
  } else {
    curr->tid = tid;
  }
  return;
}*/

void run_profiler(pid_t child_pid, int perf_fd) {
  //BUFFERS.next = NULL;

  // TODO: Process samples in the perf_event file


  struct record {
                          struct perf_event_header header;
                          uint64_t    ip;
                          uint32_t    pid;
                          uint32_t    tid;

                      };


  struct exit_record {
                          struct perf_event_header header;
                          uint32_t    pid, ppid;
                          uint32_t    tid, ptid;
                        };




  //BUFFERS.next = (struct Node*) malloc(sizeof(struct Node));
  //new_ring_buffer(BUFFERS.next, &child_pid, &child_pid, perf_fd);
                        /*
  Node* curr = BUFFERS.next;

  size_t mmap_size = 9*PAGE_SIZE;
  void* mapped = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, perf_fd, 0);
  if(mapped == ((void*) -1)) {
    perror("MMAP Failed.");
    exit(2);
  }
  curr->mapped = mapped;
  curr->first_page = (struct perf_event_mmap_page*) (curr->mapped);
  curr->next = NULL;
  //curr->pid = &child_pid;
  curr->pid = &(child_pid); */



  size_t buffer_num = 1;

  size_t mmap_size = 9*PAGE_SIZE;
  void* mapped = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, perf_fd, 0);
  if(mapped == ((void*) -1)) {
    perror("MMAP Failed.");
    exit(2);
  }
  struct Node node;
  node.mapped = mapped;
  // make header which is the first page which stores all metadata of the ring buffer
  node.first_page = (struct perf_event_mmap_page*) (mapped);
  BUFFERS.insert({std::to_string(buffer_num), node});



  buff_iter=BUFFERS.begin();
  while(buff_iter!= BUFFERS.end()) {
    Node curr = BUFFERS[std::to_string(buffer_num)];
    if(curr.first_page->data_tail < curr.first_page->data_head) {
      // exit on perf_record_exit

      struct perf_event_header* event_header = (struct perf_event_header*) ( ((char*) curr.mapped) + ((curr.first_page->data_tail % curr.first_page->data_size) + curr.first_page->data_offset) );
      // get type and cast

      // Filter out the type of struct type is PERF_SAMPLE_IP  PERF_RECORD_SAMPLE
      if(event_header->type == PERF_RECORD_SAMPLE) {
        // cast data into its app struct with the three fields (ip, pid, tid)
        struct record* rec = (struct record*) event_header;
        /*
        printf("DATA_SIZE = %llu\n", curr->first_page->data_size);
        fprintf(stderr, "IP  = 0X%lx\n", rec->ip);
        fprintf(stderr, "PID = 0X%x\n", rec->pid);
        fprintf(stderr, "TID = 0X%x\n", rec->tid);
        fprintf(stderr, "PID ot hex = %u\n", rec->pid);
        fprintf(stderr, "REAL PID = %d\n", child_pid);
        */
        const char* fname = address_to_function(rec->pid, ( (void*)(rec->ip) ));
        if(fname == NULL) {
          fprintf(stderr, "Could not find function information.\n");
        } else {
          printf("%s\n", fname);
        }
        fprintf(stderr, "Function= %s\n", fname);
        fprintf(stderr, "\n");

        // Add function occurance to global hashmap
        if(fname == NULL) {
          it = mymap.find("Unknown Function");
          if(it == mymap.end()) {
            mymap.insert({"Unknown Function", 1});
          } else {
            mymap["Unknown Function"] += 1;
          }
        }
        else {
          it = mymap.find(fname);
          if(it == mymap.end()) {
            fprintf(stderr, "Fun not in map\n");
            mymap.insert({fname, 1});
            fprintf(stderr, "Inserted fname");
          }
          else {
          mymap[fname] += 1;
          }
        }

      }
      else if(event_header->type == PERF_RECORD_FORK) {
        // open new perf_event file
        // call perf_event_open
        struct perf_event_attr pe = {
          .size = sizeof(struct perf_event_attr),
          .type = PERF_TYPE_HARDWARE,             // Count occurrences of a hardware event
          .config = PERF_COUNT_HW_REF_CPU_CYCLES, // Count cycles on the CPU independent of frequency scaling
          .disabled = 1,                          // Start the counter in a disabled state
          .exclude_kernel = 1,                    // Do not take samples in the kernel
          .exclude_hv = 1,                        // Do not take samples in the hypervisor
          .enable_on_exec = 1,                     // Enable profiling on the first exec call
          .sample_period  = 1000000,
          .task = 1,
          .sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID
        };
        int perf_fd = perf_event_open(&pe, child_pid, -1, -1, 0);
        if(perf_fd == -1) {
           fprintf(stderr, "perf_event_open failed\n");
           exit(2);
        }
        // TODO new ring buffer
        buffer_num += 1;
        size_t mmap_size = 9*PAGE_SIZE;
        void* mapped = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, perf_fd, 0);
        if(mapped == ((void*) -1)) {
          perror("MMAP Failed.");
          exit(2);
        }
        struct Node node;
        node.mapped = mapped;
        // make header which is the first page which stores all metadata of the ring buffer
        node.first_page = (struct perf_event_mmap_page*) (mapped);
        BUFFERS.insert({std::to_string(buffer_num), node});
      }
      else if(event_header->type == PERF_RECORD_EXIT) {
        struct exit_record* rec = (struct exit_record*) event_header;
        fprintf(stderr, "Exiting Process PID=%u\n TID=%u\n", rec->pid, rec->tid);
        // remove buffer from ring buffer list
        BUFFERS.erase(buff_iter);
        ++buff_iter;
      } else {
        // nothing to sample
      }
      // Now, increase data_tail (IMPORTANT)
      fprintf(stderr, "event header size = %u\n", event_header->size);
      fprintf(stderr, "data tail = %llu\n", curr.first_page->data_tail);
      curr.first_page->data_tail += event_header->size;
    }
  }

  // Lastly, Print the count of events from perf_event
  long long count;
  read(perf_fd, &count, sizeof(long long));
  printf("  ref cycles: %lld\n", count);
  size_t sum = 0;

  for(it=mymap.begin(); it != mymap.end(); it++) {
    // get sum of all calls to all functions
    std::cout << '\t' << it->first << '\t' << it->second << '\n';
    sum += it->second;
  }
  for(it=mymap.begin(); it != mymap.end(); it++) {
    std::cout << '\t' << it->first << '\t' << "Percent=%" << (100)*((float)(it->second)/(float)(sum)) << '\n';
  }

  return;
}

int main(int argc, char** argv) {
  if(argc < 2) {
    fprintf(stderr, "Usage: %s <command to run with profiler> [command arguments...]\n", argv[0]);
    exit(1);
  }

  // Create a pipe so the parent can tell the child to exec
  int pipefd[2];
  if(pipe(pipefd) == -1) {
    perror("pipe failed");
    exit(2);
  }

  // Create a child process
  pid_t child_pid = fork();

  if(child_pid == -1) {
    perror("fork failed");
    exit(2);
  } else if(child_pid == 0) {
    // In child process. Read from the pipe to pause until the parent resumes the child.
    char c;
    if(read(pipefd[0], &c, 1) != 1) {
      perror("read from pipe failed");
      exit(2);
    }
    close(pipefd[0]);

    // Exec the requested command
    if(execvp(argv[1], &argv[1])) {
      perror("exec failed");
      exit(2);
    }
  } else {
    // In the parent process

    // Set up perf_event for the child process
    struct perf_event_attr pe = {
      .size = sizeof(struct perf_event_attr),
      .type = PERF_TYPE_HARDWARE,             // Count occurrences of a hardware event
      .config = PERF_COUNT_HW_REF_CPU_CYCLES, // Count cycles on the CPU independent of frequency scaling
      .disabled = 1,                          // Start the counter in a disabled state
      //.inherit = 1,                           // Processes or threads created in the child should also be profiled
      .exclude_kernel = 1,                    // Do not take samples in the kernel
      .exclude_hv = 1,                        // Do not take samples in the hypervisor
      .enable_on_exec = 1,                     // Enable profiling on the first exec call
      //.sample_period  = 1000000,
      .sample_period  = 10000,
      //.sample_id_all = 1,
      .task = 1,
      .sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID
    };
    //TODO: SAMPLE Type bitwise | with sample_type additions

    // for sample records, define a new struct




    int perf_fd = perf_event_open(&pe, child_pid, -1, -1, 0);
    if(perf_fd == -1) {
      fprintf(stderr, "perf_event_open failed\n");
      exit(2);
    }

    // Wake up the child process by writing to the pipe
    char c = 'A';
    write(pipefd[1], &c, 1);
    close(pipefd[1]);

    // Start profiling
    run_profiler(child_pid, perf_fd);
  }

  return 0;
}
