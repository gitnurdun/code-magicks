/* MAGIC: a four-instruction classic BPF program attached to a UDP
   socket drops packets by looking at payload byte zero. */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <linux/filter.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    struct sock_filter code[] = {
        /* The UDP header is still present: payload byte 0 is at offset 8. */
        BPF_STMT(BPF_LD | BPF_B | BPF_ABS, 8),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 'G', 0, 1),
        BPF_STMT(BPF_RET | BPF_K, 0xffff),
        BPF_STMT(BPF_RET | BPF_K, 0),
    };
    struct sock_fprog fprog = {
        .len = (unsigned short)(sizeof code / sizeof code[0]),
        .filter = code,
    };
    socklen_t alen = sizeof addr;
    char buf[64];
    int rx = -1, tx = -1, good = 0, bad = 0, rc = 1;

    rx = socket(AF_INET, SOCK_DGRAM, 0);
    if (rx < 0) {
        perror("socket");
        return 1;
    }
    if (bind(rx, (struct sockaddr *)&addr, sizeof addr) != 0) {
        perror("bind");
        goto out;
    }
    if (getsockname(rx, (struct sockaddr *)&addr, &alen) != 0) {
        perror("getsockname");
        goto out;
    }
    if (setsockopt(rx, SOL_SOCKET, SO_ATTACH_FILTER,
                   &fprog, sizeof fprog) != 0) {
        if (errno == EPERM || errno == EINVAL) {
            printf("skipped: SO_ATTACH_FILTER refused: %s\n",
                   strerror(errno));
            rc = 0;
        } else {
            perror("setsockopt(SO_ATTACH_FILTER)");
        }
        goto out;
    }

    tx = socket(AF_INET, SOCK_DGRAM, 0);
    if (tx < 0) {
        perror("socket");
        goto out;
    }
    if (sendto(tx, "GOOD", 4, 0,
               (struct sockaddr *)&addr, sizeof addr) != 4) {
        perror("sendto GOOD");
        goto out;
    }
    if (sendto(tx, "BAD!", 4, 0,
               (struct sockaddr *)&addr, sizeof addr) != 4) {
        perror("sendto BAD!");
        goto out;
    }

    usleep(50000);
    for (;;) {
        ssize_t n = recv(rx, buf, sizeof buf, MSG_DONTWAIT);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            perror("recv");
            goto out;
        }
        if (n == 4 && memcmp(buf, "GOOD", 4) == 0)
            good++;
        else if (n == 4 && memcmp(buf, "BAD!", 4) == 0)
            bad++;
    }
    printf("good=%d bad=%d\n", good, bad);
    rc = 0;

out:
    if (tx >= 0)
        close(tx);
    if (rx >= 0)
        close(rx);
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Attaches a tiny classic BPF program to a UDP socket and sends it
     two four-byte datagrams, "GOOD" and "BAD!".  The filter returns the
     full packet for payloads starting with 'G' and zero (drop) for
     everything else, so a nonblocking drain receives only "GOOD" and
     the program prints good=1 bad=0.

   Step by step
     1. The receiver socket is bound to 127.0.0.1 port 0 (kernel picks
        a free port); getsockname() reads the chosen port back for the
        sender.
     2. The filter is four cBPF instructions.  BPF_LD|BPF_B|BPF_ABS,8
        loads one byte from packet offset 8 into the accumulator.  The
        IP layer has pulled the IP header, but this filter runs before
        udp_csum_pull_header(), so skb->data still points at the 8-byte
        UDP header; payload byte 0 is therefore at offset 8.  Reading
        offset 0 would inspect the high byte of the UDP source port -
        the classic socket-filter offset trap.
     3. BPF_JMP|BPF_JEQ|BPF_K compares A with 'G'; on a match it jumps
        zero instructions to the accept return, otherwise one
        instruction to the drop return.
     4. BPF_RET|BPF_K,0xffff returns a nonzero length: keep the whole
        packet.  BPF_RET|BPF_K,0 returns zero: discard it.  The return
        value is a byte count, not a boolean, and the kernel trims the
        skb to it.
     5. SO_ATTACH_FILTER takes a struct sock_fprog (instruction count
        plus pointer) and runs the verifier, which checks jumps stay in
        range and the program terminates.
     6. The sender socket sends both datagrams to the bound address.
        The kernel runs the filter in the receive path before queueing,
        so "BAD!" never appears in the socket buffer.
     7. After a short wait (loopback delivery is asynchronous), the
        receiver drains with recv(MSG_DONTWAIT) until EAGAIN; exactly
        one 4-byte "GOOD" datagram is counted.

   Output
     good=1 bad=0

   Why it matters
     Socket filters are the original in-kernel packet dispatcher.  They
     drop unwanted traffic before it consumes socket-buffer memory or
     wakes a userspace reader, and the same cBPF/eBPF machinery backs
     seccomp, tcpdump, and XDP.  Classic filters are unprivileged: any
     user can attach one to a socket they own, with no CAP_BPF or
     CAP_NET_ADMIN.

   Caveats
     - The filter's base is the UDP header, not the payload: offset 8 is
       the first payload byte.  The kernel runs the socket filter before
       udp_csum_pull_header(), then strips the header, so a filter that
       reads offset 0 is looking at the source port.  This is a common
       mistake and the reason the offset here is 8, not 0.
     - Offsets are socket-family-relative.  A filter on an AF_PACKET
       socket sees the link-layer header, and offsets there are
       completely different; do not copy this offset to another family.
     - Classic BPF is limited: no maps, no helper calls, no loops.  For
       those you need eBPF (SO_ATTACH_BPF), which additionally requires
       privileges or unprivileged-BPF being enabled.
     - The filter runs at receive time; a packet already queued when the
       filter is attached is not re-filtered.
     - setsockopt can fail with EPERM under a restrictive LSM/seccomp
       policy or EINVAL if the program fails verification; the program
       reports a skip instead of aborting.
     - UDP is unreliable in general, though loopback delivery is local.
       The fixed sleep is a demo convenience; production code would use
       poll() with a timeout instead of sleeping.
     - A BPF return of 0xffff keeps at most 65535 bytes; returning a
       larger value is capped by the kernel.
   ============================================== */
