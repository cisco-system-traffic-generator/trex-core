#include "bpfjit.h"
#include "bpf_api.h"
#include <stdlib.h>
#include <stdio.h>

char pkt[64];

int main() {
    bpf_h bpf = bpf_compile("arp");
    struct bpf_program *program = (struct bpf_program *)bpf;

    bpfjit_func_t func = bpfjit_generate_code(NULL, program->bf_insns, program->bf_len);
    bpf_destroy(bpf);
    
    bpf_args_t args;
    args.buflen = 64;
    args.wirelen = 64;
    args.pkt = pkt;
    
    int rc = func(NULL, &args);
    printf("result is %d\n", rc);
    bpfjit_free_code(func);
    
}
