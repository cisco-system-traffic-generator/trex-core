#include "bpf.h"

char buffer[100];

int main() { 

    /* ouput for BPF bytecode */
    struct bpf_program program;

    /* enter BPF filter syntax here */
    const char *buf = "arp";

    /* compile - no attach to PCAP devic e*/
    int rc = pcap_compile_nopcap(65535, DLT_EN10MB, &program, buf, 0, 0);

    /* fake header */
    struct pcap_pkthdr header;
    header.len = 42;
    header.caplen = header.len;

    /* BPF traverse */
    int match = pcap_offline_filter(&program, &header, buffer);
    printf("BPF result was %d\n", match);
}

