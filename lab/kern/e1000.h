#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

#define E1000REG(offset) *(e1000 + offset/4)
#define DSTATUS   0x00008  

#define RXD_STAT_DD       0x01    /* Descriptor Done */
#define RXD_STAT_EOP      0x02    /* End of Packet */

#define RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define RDLEN    0x02808  /* RX Descriptor Length - RW */
#define RDH      0x02810  /* RX Descriptor Head - RW */
#define RDT      0x02818  /* RX Descriptor Tail - RW */
#define RCTL     0x00100  /* RX Control - RW */
#define RA       0x05400  /* Receive Address - RW Array */
#define RAH_AV  0x80000000        /* Receive descriptor valid */

#define RCTL_EN             0x00000002    /* enable */
#define RCTL_LPE            0x00000020    /* long packet enable */
#define RCTL_BAM            0x00008000    /* broadcast enable */
#define RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

/* Transmit Descriptor bit definitions */
#define TXD_CMD_EOP    0x01 /* End of Packet */
#define TXD_CMD_IFCS   0x02 /* Insert FCS (Ethernet CRC) */
#define TXD_CMD_IC     0x04 /* Insert Checksum */
#define TXD_CMD_RS     0x08 /* Report Status */
#define TXD_CMD_RPS    0x10 /* Report Packet Sent */
#define TXD_CMD_DEXT   0x20 /* Descriptor extension (0 = legacy) */
#define TXD_CMD_VLE    0x40 /* Add VLAN tag */
#define TXD_CMD_IDE    0x80 /* Enable Tidv register */
#define TXD_STAT_DD    0x01 /* Descriptor Done */
#define TXD_STAT_EC    0x02 /* Excess Collisions */
#define TXD_STAT_LC    0x04 /* Late Collisions */
#define TXD_STAT_TU    0x08 /* Transmit underrun */
#define TXD_CMD_TCP    0x01 /* TCP packet */
#define TXD_CMD_IP     0x02 /* IP packet */
#define TXD_CMD_TSE    0x04 /* TCP Seg enable */
#define TXD_STAT_TC    0x04 /* Tx Underrun */

#define TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define TDLEN    0x03808  /* TX Descriptor Length - RW */
#define TDH      0x03810  /* TX Descriptor Head - RW */
#define TDT      0x03818  /* TX Descripotr Tail - RW */
#define TCTL     0x00400  /* TX Control - RW */
#define TIPG     0x00410  /* TX Inter-packet gap -RW */

#define TCTL_EN     0x00000002    /* enable tx */
#define TCTL_PSP    0x00000008    /* pad short packets */
#define TCTL_CT     0x00000ff0    /* collision threshold */
#define TCTL_COLD   0x003ff000    /* collision distance */

#define NTXDESC 64
#define TXBUFFSIZE 2048

#define NRXDESC 128
#define RXBUFFSIZE 2048

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
}__attribute__((packed));

struct rx_desc
{
	uint64_t addr;
	uint16_t length;
	uint16_t pchecksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
} __attribute__((packed));

char txbuffer [NTXDESC][TXBUFFSIZE];
char rxbuffer [NRXDESC][RXBUFFSIZE];

int e1000_init(struct pci_func *pcif);
int txpacket(void *src,size_t length);
int rxpacket(void *dst);

#endif  // SOL >= 6
