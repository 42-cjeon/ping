#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#include <linux/if_ether.h>
#include <linux/icmp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

SEC("xdp_hello")
int xdp_hello_main(struct xdp_md *ctx) {
        void *data_end = (void *)(long)ctx->data_end;
        void *data = (void *)(long)ctx->data;

        if (data + sizeof(struct ethhdr) + sizeof(struct ip) + sizeof(struct icmphdr) > data_end)
                return XDP_PASS;

        struct ethhdr *eth = data;
        if (eth->h_proto != htons(ETH_P_IP))
                return XDP_PASS;

        struct iphdr *ip = data + sizeof(struct ethhdr);
        if (ip->protocol!= IPPROTO_ICMP)
                return XDP_PASS;

        struct icmphdr *icmp = data + sizeof(struct ethhdr) + sizeof(struct ip);

        switch (ntohs(icmp->un.echo.sequence) % 16) {
                case 0:
		        icmp->checksum = 0xdead;
                        break;
                case 1:
		        icmp->un.echo.sequence = htons(ntohs(icmp->un.echo.sequence) - 1);
                        break;
                case 3:
                        return XDP_DROP;
                        break;
        }

        return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
