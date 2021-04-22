#include <inc/x86.h>
#include <inc/string.h>
#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here
volatile uint32_t* e1000;
struct tx_desc txqueue[NTXDESC];
struct rx_desc rxqueue[NRXDESC];

void txdesc_init(){
    memset(txqueue,0,sizeof(struct tx_desc)*NTXDESC);
    memset(txbuffer,0,TXBUFFSIZE*NTXDESC);
    for (int i = 0 ; i < NTXDESC; ++i){
        txqueue[i].addr = PADDR(txbuffer[i]);
        txqueue[i].status = TXD_STAT_DD;//初始化为DD可用状态
    }
}

void tx_init(){
    E1000REG(TDBAL) = PADDR(txqueue);
    E1000REG(TDBAH) = 0;
    E1000REG(TDLEN) = NTXDESC * sizeof(struct tx_desc);
    E1000REG(TDH) = 0;
    E1000REG(TDT) = 0;
    E1000REG(TCTL) = TCTL_EN | TCTL_PSP |(TCTL_CT & (0x10 << 4))|(TCTL_COLD & (0x40 << 12));
    E1000REG(TIPG) = 10 | (8 << 10) | (12 << 20);

}
//  驱动从尾指针写入数据包，网卡以头指针追赶发送数据包，空队列时二者重合
int txpacket(void *src,size_t length){
    uint32_t tail = E1000REG(TDT);
    struct tx_desc* next = &txqueue[tail];
    // 检查是否可用
    if (!(next->status & TXD_STAT_DD))
        return -1;
    if (length > TXBUFFSIZE)
        length = TXBUFFSIZE;
    
    memmove(txbuffer[tail],src,length);
    next->length = length;
    next->status &= ~TXD_STAT_DD;
    next->cmd = TXD_CMD_EOP | TXD_CMD_RS;//EOP指示为数据包尾，RS开启状态检查
    // 更新队尾
    E1000REG(TDT) = (tail + 1) % NTXDESC;
    return 0;
}

void rxdesc_init(){
    memset(rxqueue,0,sizeof(struct rx_desc)*NRXDESC);
    memset(rxbuffer,0,RXBUFFSIZE*NRXDESC);
    for (int i = 0 ; i < NRXDESC; ++i)
        rxqueue[i].addr = PADDR(rxbuffer[i]);   
}

void rx_init(){
    // 网络字节序处理和有效位设置
    E1000REG(RA) = 0x52|(0x54<<8)|(0x00<<16)|(0x12<<24);
    E1000REG(RA+4) = 0x34|(0x56<<8)|RAH_AV;
    E1000REG(RDBAL) = PADDR(rxqueue);
    E1000REG(RDBAH) = 0;
    E1000REG(RDLEN) = NRXDESC * sizeof(struct rx_desc);
    E1000REG(RDH) = 0;
    E1000REG(RDT) = NRXDESC - 1;
    E1000REG(RCTL) = RCTL_EN | RCTL_BAM |RCTL_SECRC;

}
// 网卡向头指针写入数据包，驱动用尾指针追赶读取数据包，满队列时尾在队列中头之后一位
int rxpacket(void* dst){
    uint32_t tail = (E1000REG(RDT) + 1 ) % NRXDESC;
    struct rx_desc *next = &rxqueue[tail]; 
    // 不可用表示无数据需要读取
    if (!(next->status & RXD_STAT_DD))
        return 0;
        
    memmove(dst,rxbuffer[tail],next->length);
    // 清空可用位
    next->status &= ~RXD_STAT_DD;
    E1000REG(RDT) = tail;
    // 返回读取字节数
    return next->length;
}

int 
e1000_init(struct pci_func *pcif){
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
    cprintf("e1000: status %x\n", E1000REG(DSTATUS));

    txdesc_init();
    tx_init();

    rxdesc_init();
    rx_init();
    
    return  0;
}