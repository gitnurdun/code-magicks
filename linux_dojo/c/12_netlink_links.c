/* MAGIC: NETLINK_ROUTE is a TLV message socket; RTM_GETLINK dumps the links that ip link shows. */
#define _GNU_SOURCE
#include <errno.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    int fd = -1, rc = 1;

    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT ||
            errno == ENOSYS) {
            printf("skipped: netlink route\n");
            return 0;
        }
        perror("socket(AF_NETLINK)");
        return 1;
    }

    struct sockaddr_nl local;
    memset(&local, 0, sizeof local);
    local.nl_family = AF_NETLINK;
    if (bind(fd, (struct sockaddr *)&local, sizeof local) < 0) {
        perror("bind(netlink)");
        goto out;
    }

    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifm;
    } req;
    memset(&req, 0, sizeof req);
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof req.ifm);
    req.nlh.nlmsg_type = RTM_GETLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nlh.nlmsg_seq = 1;
    req.ifm.ifi_family = AF_UNSPEC;

    struct sockaddr_nl kernel;
    memset(&kernel, 0, sizeof kernel);
    kernel.nl_family = AF_NETLINK;
    if (sendto(fd, &req, req.nlh.nlmsg_len, 0,
               (struct sockaddr *)&kernel, sizeof kernel) < 0) {
        perror("sendto(netlink)");
        goto out;
    }

    int links = 0, has_lo = 0, done = 0;
    char first[IFNAMSIZ] = "";
    char buf[32768];

    while (!done) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("recv(netlink)");
            goto out;
        }
        if (n == 0)
            break;

        int rem = (int)n;
        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, rem); nlh = NLMSG_NEXT(nlh, rem)) {
            if (nlh->nlmsg_type == NLMSG_DONE) {
                done = 1;
                break;
            }
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                if ((int)NLMSG_PAYLOAD(nlh, 0) >=
                    (int)sizeof(struct nlmsgerr)) {
                    struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(nlh);
                    if (err->error != 0)
                        fprintf(stderr, "netlink: %s\n", strerror(-err->error));
                }
                done = 1;
                break;
            }
            if (nlh->nlmsg_type != RTM_NEWLINK)
                continue;
            if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct ifinfomsg)))
                continue;

            struct ifinfomsg *ifm = (struct ifinfomsg *)NLMSG_DATA(nlh);
            int alen = (int)(nlh->nlmsg_len - NLMSG_LENGTH(sizeof *ifm));
            struct rtattr *rta = IFLA_RTA(ifm);
            char name[IFNAMSIZ] = "";
            uint32_t mtu = 0;
            int have_mtu = 0;

            for (; RTA_OK(rta, alen); rta = RTA_NEXT(rta, alen)) {
                if (rta->rta_type == IFLA_IFNAME) {
                    size_t plen = (size_t)RTA_PAYLOAD(rta);
                    if (plen >= sizeof name)
                        plen = sizeof name - 1;
                    if (plen > 0) {
                        memcpy(name, RTA_DATA(rta), plen);
                        name[plen] = '\0';
                    }
                } else if (rta->rta_type == IFLA_MTU &&
                           RTA_PAYLOAD(rta) >= (int)sizeof mtu) {
                    memcpy(&mtu, RTA_DATA(rta), sizeof mtu);
                    have_mtu = 1;
                }
            }

            if (name[0] != '\0' && have_mtu) {
                if (links == 0)
                    snprintf(first, sizeof first, "%s", name);
                links++;
                if (strcmp(name, "lo") == 0)
                    has_lo = 1;
            }
        }
    }

    printf("links>=2=%s has_lo=%s first=%s\n",
           links >= 2 ? "yes" : "no", has_lo ? "yes" : "no",
           first[0] ? first : "?");
    rc = 0;

out:
    if (fd >= 0)
        close(fd);
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Opens a NETLINK_ROUTE socket, asks the kernel for a dump of every
     network link, walks the returned netlink and rtnetlink TLVs, and
     reports whether at least two links exist, whether lo is one of them,
     and the name of the first link in the dump.

   Step by step
     1. socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE) creates a datagram
        socket to the kernel routing subsystem. bind() with nl_pid 0
        lets the kernel assign a local port id for replies.
     2. The request is an nlmsghdr followed by an ifinfomsg with
        ifi_family = AF_UNSPEC, type RTM_GETLINK, and flags
        NLM_F_REQUEST|NLM_F_DUMP. A dump is a sequence, not one message.
     3. recv() may return several nlmsghdrs in one buffer. NLMSG_OK checks
        that each header fits; NLMSG_NEXT advances by the aligned
        nlmsg_len. NLMSG_DONE ends the dump; NLMSG_ERROR carries a
        negative errno payload.
     4. Each RTM_NEWLINK payload starts with struct ifinfomsg. The
        attributes begin at IFLA_RTA(ifm), and each is a struct rtattr
        TLV: rta_len (header + payload), rta_type, then payload. RTA_OK,
        RTA_ALIGN, and RTA_NEXT walk them safely.
     5. IFLA_IFNAME is a string; IFLA_MTU is a 32-bit host-order integer.
        The code copies the name with a length check and only counts a
        link when both attributes parsed.

   Output
     links>=2=yes has_lo=yes first=lo

   Why it matters
     This is the same API that ip link, NetworkManager, and container
     runtimes use. It is namespace-aware, avoids parsing /sys or text,
     and gives one structured snapshot of interface names, indexes, MTUs,
     flags, and statistics.

   Caveats
     - A dump is not an atomic snapshot; links can appear or disappear
       between recv() calls, and indices can be reused.
     - The receive buffer must be large enough for every datagram. A
       truncated netlink message is discarded, so check sizing or use
       MSG_TRUNC.
     - Attribute order is not guaranteed and optional attributes may be
       absent; never index by position. IFLA_IFNAME is normally
       NUL-terminated but the length should still be bounded.
     - Reading a dump is unprivileged; modifying links needs
       CAP_NET_ADMIN. A seccomp filter may still deny the socket.
     - Netlink is a datagram protocol and is not reliable: messages can
       be dropped under memory pressure, and ENOBUFS can be reported when
       the socket receive buffer overflows during a large dump.
   ============================================== */
