/* MAGIC: the list owns only links; the real objects own themselves.
   WHY: no allocation, one identity, and good locality. */
#include <stddef.h>
#include <stdio.h>
struct list_head{struct list_head*next,*prev;};
static void init(struct list_head*h){h->next=h->prev=h;}
static void add(struct list_head*n,struct list_head*h){n->next=h->next;n->prev=h;h->next->prev=n;h->next=n;}
#define container_of(p,t,m)((t*)((char*)(p)-offsetof(t,m)))
struct job{int id;struct list_head link;};
int main(void){struct list_head q;struct job a={.id=1},b={.id=2};init(&q);add(&a.link,&q);add(&b.link,&q);for(struct list_head*p=q.next;p!=&q;p=p->next)printf("job %d\n",container_of(p,struct job,link)->id);}

/* ================= EXPLANATION =================
   What it does
     Implements a Linux-style circular doubly linked list whose nodes are
     embedded directly in the payload objects. The list owns only links;
     the jobs own themselves, so no node is ever allocated separately.

   Step by step
     1. init(h) sets h->next = h->prev = h. The head is a sentinel, not a
        data node; an empty list points at itself.
     2. add(n, h) inserts n immediately after the head:
            n->next = h->next;   n->prev = h;
            h->next->prev = n;   h->next = n;
        This is O(1) and makes every new element the first one, so the
        list behaves like a stack.
     3. main() inserts a (id 1) and then b (id 2). Forward traversal from
        q.next therefore visits b, then a; reaching &q again terminates
        the loop. The links form a cycle, so there is no NULL check.
     4. container_of() subtracts offsetof(struct job, link) from each node
        address to recover the job and print its id.

   Output
     "job 2"
     "job 1"

   Why it matters
     One object has one identity and one set of links: no wrapper node
     allocation, no copying of the payload, and traversal follows the
     actual objects through memory. This is why the kernel can put a task
     on dozens of lists at once without extra metadata.

   Caveats
     - The head must never be treated as an element; it is a sentinel.
     - A node can be in only one list at a time. Removing requires
       n->prev->next = n->next; n->next->prev = n->prev.
     - No locking or lifetime management: the caller must not free an
       object while its link is still reachable.
     - C++ requires offsetof on a standard-layout type; this pattern is
       easiest in C.
   ============================================== */
