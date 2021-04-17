	.text

/* Switch from current_thread to next_thread. Make next_thread
 * the current_thread, and set next_thread to 0.
 * Use eax as a temporary register; it is caller saved.
 */
	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */
	pushal/* 保存寄存器状态 */
	movl current_thread, %eax
	movl %esp, (%eax)/* 令curent_thread->指向 当前esp */
	
	movl next_thread, %ebx
	movl %ebx, current_thread/* 令next_thread 成为新的current_thread */
	movl (%ebx), %esp/* 将当前esp指向 next_thread */
	popal/* 恢复寄存器状态 */
	
	movl $0x0, next_thread	/* 清除 next_thread */
	ret				/* pop return address from stack */

