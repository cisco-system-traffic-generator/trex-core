#include <bpf_api.h>
#include <stdlib.h>

bpf_h
bpf_compile(const char *bpf_filter) {
    return BPF_H_NONE;
}

void
bpf_destroy(bpf_h bpf) {}


int
bpf_verify(const char *bpf_filter) {
    return 0;
}

int
bpf_run(bpf_h bpf, const char *buffer, uint32_t len) {
    return 0;
}

