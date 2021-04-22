#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	while(1){
		if (ipc_recv(NULL,&nsipcbuf,NULL) != NSREQ_OUTPUT)
			continue;
	//	- send the packet to the device driver
		while (sys_txpacket(nsipcbuf.pkt.jp_data,nsipcbuf.pkt.jp_len) < 0)
		// 传输失败时调度切换进程
		sys_yield();
	}
}
