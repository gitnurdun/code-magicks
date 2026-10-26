"""MAGIC: a candidate asks for votes in a new term; a majority wins, and
any higher term makes the old leader step down."""
class Node:
    def __init__(self, name):
        self.name = name
        self.term = 0
        self.voted_for = None
        self.state = "follower"

    def request_vote(self, term, candidate):
        if term > self.term:
            self.term = term
            self.voted_for = None
            self.state = "follower"
        if term == self.term and self.voted_for in (None, candidate):
            self.voted_for = candidate
            return True
        return False

def elect(nodes, candidate):
    candidate.term += 1
    candidate.state = "candidate"
    candidate.voted_for = candidate.name
    votes = 1
    for node in nodes:
        if node is not candidate and node.request_vote(candidate.term, candidate.name):
            votes += 1
    if votes >= 2:
        candidate.state = "leader"
    print(f"{candidate.name} becomes {candidate.state} in term "
          f"{candidate.term} with {votes} votes")
    return candidate.state == "leader"

a, b, c = Node("A"), Node("B"), Node("C")
nodes = [a, b, c]
elect(nodes, a)
elect(nodes, b)
print(f"A is {a.state} in term {a.term}")

# ================= EXPLANATION =================
# What it does
#   Simulates two Raft elections among three nodes. A wins term 1; later B
#   starts an election in term 2, receives a majority, and A steps down
#   because it sees the higher term.
#
# Step by step
#   1. A increments its term to 1, votes for itself, and requests votes
#      from B and C. Both have term 0, so they update to term 1, grant
#      their vote to A, and A has 3 votes (a majority of 3 is 2).
#   2. A becomes leader for term 1. In real Raft it would immediately send
#      heartbeats to suppress other elections.
#   3. B times out and starts an election in term 2. A and C see term 2 >
#      their current term, so they become followers, clear their old vote,
#      and grant it to B. B has 3 votes and becomes leader.
#   4. A's state is now follower in term 2. It did not need a special
#      "leader resignation" message; the higher term in B's request is
#      enough to make A step down.
#   5. The demo omits logs, so it does not check the "candidate's log is at
#      least as up-to-date" condition. That condition is essential for
#      safety in a real Raft cluster.
#
# Output
#   "A becomes leader in term 1 with 3 votes"
#   "B becomes leader in term 2 with 3 votes"
#   "A is follower in term 2"
#
# Why it matters
#   Raft makes distributed consensus understandable: terms are logical
#   epochs, a leader needs a majority, and higher terms override older
#   ones. The same quorum math underpins replicated logs, configuration
#   stores, and leader election in Kubernetes-style systems. Randomized
#   election timeouts break ties and prevent repeated split votes.
#
# Caveats
#   - This is a toy: no AppendEntries, logs, commit index, heartbeats,
#     persistence, or network partitions. It is not Raft and must not be
#     used for real consensus.
#   - A candidate must have an up-to-date log to win; majority alone is
#     not enough for safety.
#   - Terms and votes must be persisted before responding, or a crash can
#     violate election safety.
#   - Simultaneous candidates can split the vote; randomized timeouts and
#     retries resolve it, but liveness is not guaranteed under arbitrary
#     partitions.
#   - A majority of 3 is 2. With 4 nodes it is 3; quorum size is
#     floor(n/2)+1, and even-sized clusters do not improve fault tolerance.
#   - Real RPCs can be delayed, duplicated, or reordered; handlers must
#     reject stale terms and idempotently process retries.
#   - Leader election is only half of Raft; log replication and commitment
#     provide the actual state-machine safety.
# ==============================================
