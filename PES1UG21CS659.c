#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <string.h>
#include <ctype.h>

typedef char byte;

// States for MESI protocol
#define INVALID 0
#define MODIFIED 1
#define EXCLUSIVE 2
#define SHARED 3

//parallelize the code

struct cache {
    byte address;
    byte value;
    byte state;
};

struct decoded_inst {
    int type; // 0 is RD, 1 is WR
    int address;
    int value; // Only used for WR
};

typedef struct cache cache;
typedef struct decoded_inst decoded;

byte *memory;

decoded decode_inst_line(char *buffer) {
    decoded inst;
    char inst_type[3]; // Increased size to accommodate "WR"
    sscanf(buffer, "%s", inst_type);
    if (!strcmp(inst_type, "RD")) {
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;
        inst.type = 0;
    } else if (!strcmp(inst_type, "WR")) {
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.address = addr;
        inst.value = val;
        inst.type = 1;
    }
    
    return inst;
}

void print_cachelines(cache *c, int cache_size) {
    for (int i = 0; i < cache_size; i++) {
        cache cacheline = *(c + i);
        printf("Address: %d, State: %d, Value: %d\n", cacheline.address, cacheline.state, cacheline.value);
    }
}

void cpu_loop(int thread_num, int num_threads) {
    int cache_size = 2;
    cache *c = (cache *) calloc(cache_size,sizeof(cache_size));

    char filename[20];
    sprintf(filename, "input_%d.txt", thread_num);
    FILE *inst_file = fopen(filename, "r");
    char inst_line[20];
    cache cacheline = {
        .address = 0,
        .value = 0,
        .state = INVALID
    };

    while (fgets(inst_line, sizeof(inst_line), inst_file)) {
        decoded inst = decode_inst_line(inst_line);
        int hash = inst.address % cache_size;
        cacheline = *(c + hash);

        #pragma omp critical
        {
            switch (cacheline.state) {
                case INVALID:
                    *(memory + cacheline.address) = cacheline.value;
                    cacheline.address = inst.address;
                    
                    cacheline.state = EXCLUSIVE;
                    cacheline.value = *(memory + inst.address);
                    if (inst.type == 1) {
                        cacheline.value = inst.value;
                    }
                    *(c + hash) = cacheline;
                    break;

                case EXCLUSIVE:
                    if (cacheline.address != inst.address) {
                        cacheline.state = INVALID;
                    }
                    if (inst.type == 1) {
                        cacheline.value = inst.value;
                        cacheline.state = MODIFIED;
                    }
                    break;

                case SHARED:
                    if (inst.type == 1) {
                        cacheline.state = MODIFIED;
                        cacheline.value = inst.value;
                    }
                    break;

                case MODIFIED:
                    if (inst.type == 0) {
                        cacheline.state = SHARED;
                    }
                    if (inst.type == 1) {
                        cacheline.value = inst.value;
                    }
                    break;
            }
        }

        switch (inst.type) {
            case 0:
                printf("Thread %d: RD %d: %d\n", thread_num, cacheline.address, cacheline.value);
                break;

            case 1:
                printf("Thread %d: WR %d: %d\n", thread_num, cacheline.address, cacheline.value);
                break;
        }
    }

    #pragma omp barrier
    free(c);
    fclose(inst_file);
}

int main(int argc, char *argv[]) {
    int memory_size = 24;
    memory = (byte *) malloc(sizeof(byte) * memory_size);
    int num_threads = argc > 1 ? atoi(argv[1]) : 1;

    double start = omp_get_wtime();

    #pragma omp parallel num_threads(num_threads)
    {
        int thread_num = omp_get_thread_num();
        cpu_loop(thread_num, num_threads);
    }

    double end = omp_get_wtime();   
    printf("Time: %.8fms\n", end - start);
    free(memory);
    return 0;
}