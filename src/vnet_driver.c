#include "../include/vnet_driver.h"
#include <linux/version.h>
#include <linux/etherdevice.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Virtual Network Driver with Packet Capture");
MODULE_VERSION("1.0");

/* Biến toàn cục */
struct net_device *vnet_devices[2];
struct list_head captured_packets;
spinlock_t capture_lock;
int packet_count = 0;

/* Hàm mở interface */
int vnet_open(struct net_device *dev)
{
    printk(KERN_INFO "vnet: Interface %s được mở\n", dev->name);

    /* Bắt đầu queue truyền dữ liệu */
    netif_start_queue(dev);

    return 0;
}

/* Hàm đóng interface */
int vnet_close(struct net_device *dev)
{
    printk(KERN_INFO "vnet: Interface %s được đóng\n", dev->name);

    /* Dừng queue truyền dữ liệu */
    netif_stop_queue(dev);

    return 0;
}

/* Hàm xử lý gói tin gửi đi */
netdev_tx_t vnet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);
    struct vnet_priv *peer_priv;
    struct sk_buff *new_skb;
    struct captured_packet *cap_pkt;
    unsigned long flags;

    /* Kiểm tra peer device có tồn tại không */
    if (!priv->peer)
    {
        printk(KERN_ERR "vnet: Không tìm thấy peer device cho %s\n", dev->name);
        dev_kfree_skb(skb);
        priv->stats.tx_dropped++;
        return NETDEV_TX_OK;
    }

    peer_priv = netdev_priv(priv->peer);

    /* Lưu packet để capture */
    cap_pkt = kmalloc(sizeof(struct captured_packet), GFP_ATOMIC);
    if (cap_pkt)
    {
        cap_pkt->skb = skb_copy(skb, GFP_ATOMIC);
        if (cap_pkt->skb)
        {
            strncpy(cap_pkt->interface_name, dev->name, IFNAMSIZ);
            cap_pkt->interface_name[IFNAMSIZ-1] = '\0';
            cap_pkt->timestamp = jiffies;
            cap_pkt->direction = priv->id; // 0: vnet0->vnet1, 1: vnet1->vnet0

            spin_lock_irqsave(&capture_lock, flags);
            list_add_tail(&cap_pkt->list, &captured_packets);
            packet_count++;
            spin_unlock_irqrestore(&capture_lock, flags);

            printk(KERN_INFO "vnet: Bắt được packet từ %s (size: %u bytes)\n",
                   dev->name, skb->len);
        }
        else
        {
            kfree(cap_pkt);
        }
    }

/* Cập nhật thông tin skb cho peer device */
    new_skb = skb_copy(skb, GFP_ATOMIC);
    if (!new_skb) {
        printk(KERN_ERR "vnet: Không thể copy skb\n");
        dev_kfree_skb(skb);
        priv->stats.tx_dropped++;
        return NETDEV_TX_OK;
    }

    // THÊM DÒNG NÀY: Đặt lại con trỏ network header
    skb_reset_network_header(new_skb);
    
    // THÊM DÒNG NÀY: Đặt lại con trỏ transport header
    skb_reset_transport_header(new_skb);
    
    new_skb->dev = priv->peer;
    new_skb->protocol = eth_type_trans(new_skb, priv->peer);
    
    /* Đặt trạng thái checksum */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    new_skb->ip_summed = CHECKSUM_UNNECESSARY;
#else
    new_skb->ip_summed = CHECKSUM_NONE;
#endif


    /* Cập nhật thống kê cho sender */
    spin_lock_irqsave(&priv->lock, flags);
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;
    spin_unlock_irqrestore(&priv->lock, flags);

    /* Cập nhật thống kê cho receiver */
    spin_lock_irqsave(&peer_priv->lock, flags);
    peer_priv->stats.rx_packets++;
    peer_priv->stats.rx_bytes += new_skb->len;
    spin_unlock_irqrestore(&peer_priv->lock, flags);

    /* Gửi packet lên network stack của peer */
    netif_rx(new_skb);

    /* Giải phóng skb gốc */
    dev_kfree_skb(skb);

    printk(KERN_INFO "vnet: Chuyển packet từ %s tới %s (size: %u bytes)\n",
           dev->name, priv->peer->name, new_skb->len);

    return NETDEV_TX_OK;
}

/* Hàm lấy thống kê */
struct net_device_stats *vnet_get_stats(struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);
    return &priv->stats;
}

/* Cấu trúc các hàm operations của network device */
static const struct net_device_ops vnet_netdev_ops = {
    .ndo_open = vnet_open,
    .ndo_stop = vnet_close,
    .ndo_start_xmit = vnet_start_xmit,
    .ndo_get_stats = vnet_get_stats,
};

/* Hàm khởi tạo network device */
static void vnet_setup(struct net_device *dev)
{
    struct vnet_priv *priv;

    /* Thiết lập cơ bản cho ethernet device */
    ether_setup(dev);

    /* Gán operations */
    dev->netdev_ops = &vnet_netdev_ops;

    /* Thiết lập flags */
    dev->flags |= IFF_NOARP;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    /* Kernel 5.15+ sử dụng cơ chế checksum mới */
    dev->features |= NETIF_F_HW_CSUM;
#else
    /* Kernel cũ hơn */
    dev->features |= NETIF_F_NO_CSUM;
#endif
    
    /* Khởi tạo private data */
    priv = netdev_priv(dev);
    memset(priv, 0, sizeof(struct vnet_priv));
    spin_lock_init(&priv->lock);

    /* Tạo MAC address giả */
    eth_random_addr(dev->dev_addr);

    printk(KERN_INFO "vnet: Device %s được setup với MAC: %pM\n",
           dev->name, dev->dev_addr);
}

/* Hàm khởi tạo module */
static int __init vnet_init(void)
{
    int ret = 0;
    struct vnet_priv *priv0, *priv1;

    printk(KERN_INFO "vnet: Bắt đầu khởi tạo Virtual Network Driver\n");

    /* Khởi tạo danh sách captured packets */
    INIT_LIST_HEAD(&captured_packets);
    spin_lock_init(&capture_lock);

    /* Tạo device đầu tiên (vnet0) */
    vnet_devices[0] = alloc_netdev(sizeof(struct vnet_priv),
                                   VNET_DEVICE_NAME_A,
                                   NET_NAME_ENUM,
                                   vnet_setup);
    if (!vnet_devices[0])
    {
        printk(KERN_ERR "vnet: Không thể cấp phát memory cho %s\n",
               VNET_DEVICE_NAME_A);
        return -ENOMEM;
    }

    /* Tạo device thứ hai (vnet1) */
    vnet_devices[1] = alloc_netdev(sizeof(struct vnet_priv),
                                   VNET_DEVICE_NAME_B,
                                   NET_NAME_ENUM,
                                   vnet_setup);
    if (!vnet_devices[1])
    {
        printk(KERN_ERR "vnet: Không thể cấp phát memory cho %s\n",
               VNET_DEVICE_NAME_B);
        free_netdev(vnet_devices[0]);
        return -ENOMEM;
    }

    /* Thiết lập quan hệ peer giữa 2 devices */
    priv0 = netdev_priv(vnet_devices[0]);
    priv1 = netdev_priv(vnet_devices[1]);

    priv0->peer = vnet_devices[1];
    priv1->peer = vnet_devices[0];
    priv0->id = 0;
    priv1->id = 1;
    strncpy(priv0->name, VNET_DEVICE_NAME_A, IFNAMSIZ);
    strncpy(priv1->name, VNET_DEVICE_NAME_B, IFNAMSIZ);

    /* Đăng ký devices với kernel */
    ret = register_netdev(vnet_devices[0]);
    if (ret)
    {
        printk(KERN_ERR "vnet: Không thể đăng ký %s, lỗi: %d\n",
               VNET_DEVICE_NAME_A, ret);
        free_netdev(vnet_devices[0]);
        free_netdev(vnet_devices[1]);
        return ret;
    }

    ret = register_netdev(vnet_devices[1]);
    if (ret)
    {
        printk(KERN_ERR "vnet: Không thể đăng ký %s, lỗi: %d\n",
               VNET_DEVICE_NAME_B, ret);
        unregister_netdev(vnet_devices[0]);
        free_netdev(vnet_devices[0]);
        free_netdev(vnet_devices[1]);
        return ret;
    }

    printk(KERN_INFO "vnet: Khởi tạo thành công 2 virtual network interfaces\n");
    printk(KERN_INFO "vnet: %s <-> %s\n", VNET_DEVICE_NAME_A, VNET_DEVICE_NAME_B);

    return 0;
}

/* Hàm cleanup khi remove module */
void vnet_cleanup(void)
{
    struct captured_packet *cap_pkt, *tmp;
    unsigned long flags;

    printk(KERN_INFO "vnet: Bắt đầu cleanup Virtual Network Driver\n");

    /* Unregister và free network devices */
    if (vnet_devices[0])
    {
        unregister_netdev(vnet_devices[0]);
        free_netdev(vnet_devices[0]);
        printk(KERN_INFO "vnet: Đã cleanup %s\n", VNET_DEVICE_NAME_A);
    }

    if (vnet_devices[1])
    {
        unregister_netdev(vnet_devices[1]);
        free_netdev(vnet_devices[1]);
        printk(KERN_INFO "vnet: Đã cleanup %s\n", VNET_DEVICE_NAME_B);
    }

    /* Cleanup captured packets */
    spin_lock_irqsave(&capture_lock, flags);
    list_for_each_entry_safe(cap_pkt, tmp, &captured_packets, list)
    {
        list_del(&cap_pkt->list);
        if (cap_pkt->skb)
            dev_kfree_skb(cap_pkt->skb);
        kfree(cap_pkt);
    }
    packet_count = 0;
    spin_unlock_irqrestore(&capture_lock, flags);

    printk(KERN_INFO "vnet: Cleanup hoàn tất\n");
}

static void __exit vnet_exit(void)
{
    vnet_cleanup();
}

module_init(vnet_init);
module_exit(vnet_exit);