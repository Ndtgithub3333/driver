#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Netfilter Hook for Virtual Network Packet Capture");
MODULE_VERSION("1.0");

#define PROC_FILENAME "vnet_capture"
#define MAX_CAPTURED_PACKETS 1000

/* Cấu trúc lưu thông tin packet đã bắt */
struct packet_info {
    unsigned long timestamp;
    __be32 src_ip;
    __be32 dst_ip;
    __be16 src_port;
    __be16 dst_port;
    __u8 protocol;
    __u16 length;
    char interface[IFNAMSIZ];
    char direction[10]; // "IN" hoặc "OUT"
};

/* Mảng lưu trữ thông tin packets */
static struct packet_info captured_packets[MAX_CAPTURED_PACKETS];
static int packet_index = 0;
static int total_packets = 0;
static spinlock_t capture_lock;

/* Proc entry */
static struct proc_dir_entry *proc_entry;

/* Hàm phân tích và lưu thông tin packet */
static void analyze_and_store_packet(struct sk_buff *skb, const char *interface, const char *direction)
{
    struct iphdr *ip_header;
    struct tcphdr *tcp_header;
    struct udphdr *udp_header;
    struct packet_info *pkt_info;
    unsigned long flags;
    
    /* Kiểm tra xem có phải IP packet không */
    if (skb->protocol != htons(ETH_P_IP))
        return;
    
    ip_header = ip_hdr(skb);
    if (!ip_header)
        return;
    
    spin_lock_irqsave(&capture_lock, flags);
    
    /* Lưu thông tin packet */
    pkt_info = &captured_packets[packet_index];
    pkt_info->timestamp = jiffies;
    pkt_info->src_ip = ip_header->saddr;
    pkt_info->dst_ip = ip_header->daddr;
    pkt_info->protocol = ip_header->protocol;
    pkt_info->length = ntohs(ip_header->tot_len);
    strncpy(pkt_info->interface, interface, IFNAMSIZ - 1);
    pkt_info->interface[IFNAMSIZ - 1] = '\0';
    strncpy(pkt_info->direction, direction, sizeof(pkt_info->direction) - 1);
    pkt_info->direction[sizeof(pkt_info->direction) - 1] = '\0';
    
    /* Lấy thông tin port nếu là TCP hoặc UDP */
    pkt_info->src_port = 0;
    pkt_info->dst_port = 0;
    
    if (ip_header->protocol == IPPROTO_TCP) {
        tcp_header = tcp_hdr(skb);
        if (tcp_header) {
            pkt_info->src_port = ntohs(tcp_header->source);
            pkt_info->dst_port = ntohs(tcp_header->dest);
        }
    } else if (ip_header->protocol == IPPROTO_UDP) {
        udp_header = udp_hdr(skb);
        if (udp_header) {
            pkt_info->src_port = ntohs(udp_header->source);
            pkt_info->dst_port = ntohs(udp_header->dest);
        }
    }
    
    /* Cập nhật index */
    packet_index = (packet_index + 1) % MAX_CAPTURED_PACKETS;
    total_packets++;
    
    spin_unlock_irqrestore(&capture_lock, flags);
    
    printk(KERN_INFO "netfilter_capture: Bắt packet %s on %s: %pI4:%d -> %pI4:%d (protocol: %d, len: %d)\n",
           direction, interface,
           &pkt_info->src_ip, pkt_info->src_port,
           &pkt_info->dst_ip, pkt_info->dst_port,
           pkt_info->protocol, pkt_info->length);
}

/* Hook function cho INPUT chain */
static unsigned int hook_func_in(void *priv,
                                struct sk_buff *skb,
                                const struct nf_hook_state *state)
{
    /* Chỉ bắt packet từ virtual interfaces */
    if (state->in && 
        (strncmp(state->in->name, "vnet", 4) == 0)) {
        analyze_and_store_packet(skb, state->in->name, "IN");
    }
    
    return NF_ACCEPT; /* Cho phép packet đi tiếp */
}

/* Hook function cho OUTPUT chain */
static unsigned int hook_func_out(void *priv,
                                 struct sk_buff *skb,
                                 const struct nf_hook_state *state)
{
    /* Chỉ bắt packet từ virtual interfaces */
    if (state->out && 
        (strncmp(state->out->name, "vnet", 4) == 0)) {
        analyze_and_store_packet(skb, state->out->name, "OUT");
    }
    
    return NF_ACCEPT; /* Cho phép packet đi tiếp */
}

/* Cấu trúc netfilter hooks */
static struct nf_hook_ops netfilter_ops_in = {
    .hook = hook_func_in,
    .hooknum = NF_INET_LOCAL_IN,
    .pf = PF_INET,
    .priority = NF_IP_PRI_FIRST,
};

static struct nf_hook_ops netfilter_ops_out = {
    .hook = hook_func_out,
    .hooknum = NF_INET_LOCAL_OUT,
    .pf = PF_INET,
    .priority = NF_IP_PRI_FIRST,
};

/* Hàm hiển thị thông tin trong /proc */
static int proc_show(struct seq_file *m, void *v)
{
    int i, count;
    struct packet_info *pkt;
    unsigned long flags;
    char protocol_name[8];
    char src_ip_str[16], dst_ip_str[16];

    seq_puts(m, "============== Virtual Network Packet Capture Statistics ==============\n");
    seq_printf(m, "Total packets captured: %d\n", total_packets);
    seq_printf(m, "Current buffer size: %d packets\n", 
               (total_packets > MAX_CAPTURED_PACKETS) ? MAX_CAPTURED_PACKETS : total_packets);
    seq_puts(m, "\n");

    // Header: | Timestamp  | Dir |   Source IP   | SPort |   Dest IP    | DPort | Proto | Len  |   Iface   |
    seq_puts(m, "+------------+-----+-----------------+-------+-----------------+-------+-------+------+-----------+\n");
    seq_puts(m, "| Timestamp  | Dir |   Source IP     | SPort |     Dest IP     | DPort | Proto | Len  |   Iface   |\n");
    seq_puts(m, "+------------+-----+-----------------+-------+-----------------+-------+-------+------+-----------+\n");

    spin_lock_irqsave(&capture_lock, flags);

    count = (total_packets > MAX_CAPTURED_PACKETS) ? MAX_CAPTURED_PACKETS : total_packets;

    for (i = 0; i < count; i++) {
        int idx = (packet_index - count + i + MAX_CAPTURED_PACKETS) % MAX_CAPTURED_PACKETS;
        pkt = &captured_packets[idx];

        // Protocol as string
        switch (pkt->protocol) {
            case IPPROTO_TCP:
                strcpy(protocol_name, "TCP");
                break;
            case IPPROTO_UDP:
                strcpy(protocol_name, "UDP");
                break;
            case IPPROTO_ICMP:
                strcpy(protocol_name, "ICMP");
                break;
            default:
                snprintf(protocol_name, sizeof(protocol_name), "%u", pkt->protocol);
                break;
        }

        snprintf(src_ip_str, sizeof(src_ip_str), "%pI4", &pkt->src_ip);
        snprintf(dst_ip_str, sizeof(dst_ip_str), "%pI4", &pkt->dst_ip);

        // Căn chỉnh bảng
        seq_printf(m,
            "| %10lu | %-3s | %15s | %5u | %15s | %5u | %5s | %4u | %-9s |\n",
            pkt->timestamp,
            pkt->direction,
            src_ip_str,
            pkt->src_port,
            dst_ip_str,
            pkt->dst_port,
            protocol_name,
            pkt->length,
            pkt->interface
        );
    }

    spin_unlock_irqrestore(&capture_lock, flags);

    seq_puts(m, "+------------+-----+---------------+-------+---------------+-------+-------+------+-----------+\n");

    return 0;
}

/* Hàm mở proc file */
static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

/* Proc file operations */
static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* Hàm khởi tạo module */
static int __init vnet_netfilter_init(void)
{
    int ret;
    
    printk(KERN_INFO "netfilter_capture: Bắt đầu khởi tạo Netfilter Hook\n");
    
    /* Khởi tạo lock */
    spin_lock_init(&capture_lock);
    
    /* Tạo proc entry */
    proc_entry = proc_create(PROC_FILENAME, 0444, NULL, &proc_fops);
    if (!proc_entry) {
        printk(KERN_ERR "netfilter_capture: Không thể tạo proc entry\n");
        return -ENOMEM;
    }
    
    /* Đăng ký netfilter hooks */
    ret = nf_register_net_hook(&init_net, &netfilter_ops_in);
    if (ret) {
        printk(KERN_ERR "netfilter_capture: Không thể đăng ký INPUT hook\n");
        proc_remove(proc_entry);
        return ret;
    }
    
    ret = nf_register_net_hook(&init_net, &netfilter_ops_out);
    if (ret) {
        printk(KERN_ERR "netfilter_capture: Không thể đăng ký OUTPUT hook\n");
        nf_unregister_net_hook(&init_net, &netfilter_ops_in);
        proc_remove(proc_entry);
        return ret;
    }
    
    printk(KERN_INFO "netfilter_capture: Khởi tạo thành công\n");
    printk(KERN_INFO "netfilter_capture: Xem thống kê tại /proc/%s\n", PROC_FILENAME);
    
    return 0;
}

/* Hàm cleanup module */
static void __exit vnet_netfilter_exit(void)
{
    printk(KERN_INFO "netfilter_capture: Bắt đầu cleanup\n");
    
    /* Unregister netfilter hooks */
    nf_unregister_net_hook(&init_net, &netfilter_ops_in);
    nf_unregister_net_hook(&init_net, &netfilter_ops_out);
    
    /* Remove proc entry */
    proc_remove(proc_entry);
    
    printk(KERN_INFO "netfilter_capture: Cleanup hoàn tất\n");
}

module_init(vnet_netfilter_init);
module_exit(vnet_netfilter_exit);