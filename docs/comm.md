Hydra I/O Communication Standard
--------------------------------

The communication system uses a shared-memory circular buffer design, enabling multiple peers to exchange raw byte
streams with minimal overhead. Every peer can both send and receive, and all synchronization relies on atomic
operations to maintain lock-free, deterministic behavior.

# Peer Model

There is exactly **one master peer**. Its responsibilities include:

- Creating the shared memory region (context + buffers)
- Initializing and maintaining the peer registry
- Allocating global indices and buffer metadata
- Performing round-robin scheduling for fair access

All additional peers act as **clients**. They map the shared regions created by the master and verify the master and
peer context magic values before participating.

# Memory Layout

Shared memory contains two main zones: the **master context** and the **peer contexts**.
Each peer context owns a circular buffer for data exchange.
The master context holds global system metadata.

##### Fig 1. Shared Memory Layout

| Region       | Description                     |
|--------------|---------------------------------|
| master_ctx   | Master metadata block           |
| first_peer   | Master peer context (index 0)   |

##### Fig 2. Alignment Requirements

| Region       | Alignment          |
|--------------|--------------------|
| master_ctx   | 8 bytes            |
| peer_ctx     | 8 bytes            |
| buffer       | PAGE_SIZE (0x1000) |

# Master Context

Peers connect to the master using the master handle to discover the peer context region.
Peer context index **0** always corresponds to the master itself.

##### Fig 3. Master Context Structure

| Field       | Type  | Description                                                    |
|-------------|-------|----------------------------------------------------------------|
| magic       | u32   | Magic value (`'cMyH'`) for validation ("hy master ctx")        |
| max_peers   | u32   | Maximum number of peers supported                              |
| peers       | HANDLE| Handle to shared memory containing peer contexts               |

# Peer Context

##### Fig 4. Peer Context Structure

| Field        | Type  | Description                                                                 |
|--------------|--------|-----------------------------------------------------------------------------|
| magic        | u32   | Magic value (`'cPyH'`) for validation ("hy peer ctx")                        |
| id           | u32   | Unique peer identifier                                                       |
| buffer       | HANDLE| Handle to the peer’s shared-memory buffer                                    |
| size         | u64   | Size of the circular buffer                                                  |
| read_index   | u64   | Global read index                                                            |
| write_index  | u64   | Global write index                                                           |
| flags        | u32   | Peer state flags (e.g., “ready”)                                             |
| target       | u32   | Peer ID to send data to (`UINT32_MAX` indicates none)                        |

## Index Behavior

Both indices are **global across all peers**, ensuring consistent ordering in the circular buffer:
- **read_index** → byte position next to read
- **write_index** → byte position next to write

# Round-Robin Scheduling

The master ensures fair access to the shared buffer by cycling through peers in
round-robin order. Each peer has its own queue of pending send operations.

## Send Attempt

When a peer wants to send data to another peer:

- It marks its queue as non-empty by setting the `ready` flag.
- The master scans all peers repeatedly looking for ready peers.

## Master Handling

When the master finds a peer that is ready to send data:

1. It lazily maps the target peer’s buffer into virtual memory.
2. It checks space availability by verifying `write_index != read_index`.
   - If equal, the buffer is full and the master moves on.

# 5. Data Transfer

## Discovering Peers

- A peer queries the master context to obtain the active peer list.
- The peer chooses communication targets according to application logic.

### Sending Data

- The peer maps the target buffer into its virtual address space.
- The peer checks for space: if `write_index == read_index`, the buffer is full and the peer must wait.

