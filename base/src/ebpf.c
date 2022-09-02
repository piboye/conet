#include <linux/filter.h>
#include <sys/socket.h>

int enable_reuseport_cbpf(int fd) 
{
  struct sock_filter code[] = {
     {BPF_LD | BPF_W | BPF_ABS, 0, 0, SKF_AD_OFF + SKF_AD_CPU}, 
    {BPF_RET | BPF_A, 0, 0, 0}};
  struct sock_fprog prog = { .len = 2, .filter = code };
  return setsockopt(fd, SOL_SOCKET, SO_ATTACH_REUSEPORT_CBPF, &prog, sizeof(prog));
}