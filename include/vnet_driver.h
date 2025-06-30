#ifndef VNET_DRIVER_H
#define VNET_DRIVER_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define VNET_DEVICE_NAME_A "vnet0"
#define VNET_DEVICE_NAME_B "vnet1"
#define VNET_MAX_PACKET_SIZE 1500
#define VNET_STATS_PROC_NAME "vnet_stats"

/* Cấu trúc lưu trữ thông tin private của mỗi interface */
struct vnet_priv {
    struct net_device_stats stats;    // Thống kê network
    struct net_device *peer;          // Con trỏ tới interface đối tác
    spinlock_t lock;                  // Lock để đồng bộ
    char name[IFNAMSIZ];             // Tên interface
    int id;                          // ID của interface (0 hoặc 1)
};

/* Cấu trúc lưu packet đã bắt được */
struct captured_packet {
    struct list_head list;
    struct sk_buff *skb;
    char interface_name[IFNAMSIZ];
    unsigned long timestamp;
    int direction; // 0: A->B, 1: B->A
};

/* Biến toàn cục */
extern struct net_device *vnet_devices[2];
extern struct list_head captured_packets;
extern spinlock_t capture_lock;
extern int packet_count;

/* Khai báo hàm */
int vnet_open(struct net_device *dev);
int vnet_close(struct net_device *dev);
netdev_tx_t vnet_start_xmit(struct sk_buff *skb, struct net_device *dev);
struct net_device_stats *vnet_get_stats(struct net_device *dev);
void vnet_cleanup(void);

#endif /* VNET_DRIVER_H */