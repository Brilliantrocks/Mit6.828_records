#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	char buf[2048];
	size_t len;

	// LAB 6: Your code here:
	while (1){
		// 	- read a packet from the device driver
		// 接收队列为空时交还CPU控制权
		if ((len = sys_rxpacket(buf)) == 0){
			sys_yield();
		}
		//	- send it to the network server
		// Hint: When you IPC a page to the network server, it will be
		// reading from it for a while, so don't immediately receive
		// another packet in to the same physical page.
		else{
		memcpy(nsipcbuf.pkt.jp_data, buf, len);
		nsipcbuf.pkt.jp_len = len;
		ipc_send(ns_envid,NSREQ_INPUT,&nsipcbuf,PTE_U|PTE_P);
		cprintf("input once\n");	
		// Hint: When you IPC a page to the network server, it will be
		// reading from it for a while, so don't immediately receive
		// another packet in to the same physical page.
		for(int i=0; i<500; i++)
			sys_yield();
		}
	}		
}
