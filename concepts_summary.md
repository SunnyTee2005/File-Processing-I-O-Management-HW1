# File I/O Concepts Summary

## How to Actually Learn This

The reason concepts don't stick is that you're learning by **recognition** (reading and nodding) instead of **recall** (producing from memory).

**What actually works:**

1. **Active recall** — after finishing a session, close everything and write down what you just learned without looking. Struggling to recall = the moment learning happens.
2. **Spaced repetition** — revisit the same concept 1 day, 3 days, 1 week later. Each time, try to recall before re-reading.
3. **Ask "why", not just "what"** — don't just know "fsync flushes to disk." Know *why it matters*: if your program crashes after `write()` but before `fsync()`, data in page cache is lost. Stories stick.
4. **Use it yourself** — code you wrote yourself sticks more than code you read.

---

## The I/O Stack

```
Your program (user space)
    ↓  fwrite() / write()
[ stdio buffer ]      ← user-space, managed by C library (only for fwrite/fread)
    ↓  write() syscall
[ Page Cache ]        ← kernel RAM, OS-managed
    ↓  fsync() / OS decides
[ Disk ]
```

Two levels of buffering. Each level reduces the number of expensive operations below it.

---

## Key Concepts

### Page Cache
- Kernel-managed RAM that sits between your program and the disk
- `write()` stores data here first — NOT directly to disk
- `read()` checks here first before hitting disk
- **Warm cache**: file already in RAM → reads are fast (~nanoseconds)
- **Cold cache**: file not in RAM → must read from disk (~milliseconds)
- Flush cache on Linux: `sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"`

### System Calls vs Library Functions
| | Library function (e.g. `printf`, `fwrite`) | System call (e.g. `write`, `read`) |
|---|---|---|
| Runs in | User space | Kernel space |
| Cost | Cheap | Expensive (context switch) |
| Buffering | Yes (stdio buffer) | No |

`printf` → buffers in user space → eventually calls `write()` → kernel → screen.

### File Descriptors
- An integer (`3`, `4`, `5`...) the OS gives you when you call `open()`
- OS limit: typically 1024 per process
- **Always `close(fd)`** — not closing = FD leak → eventually can't open any more files
- `close()` also flushes kernel buffers and releases file table entry

### fsync()
- Forces page cache → disk (durable write)
- A system call (but has a C wrapper in `unistd.h`)
- Expensive — blocks until disk confirms the write
- Without it: data may be lost on power failure

### O_ flags for open()
| Flag | Meaning |
|---|---|
| `O_RDONLY` | read only |
| `O_WRONLY` | write only |
| `O_RDWR` | read and write |
| `O_CREAT` | create file if doesn't exist (needs 3rd arg: permissions e.g. `0644`) |
| `O_TRUNC` | wipe file to 0 bytes on open |
| `O_APPEND` | all writes go to end of file |
| `O_SYNC` | every write blocks until data hits disk (like fsync after every write) |

### 4KB Alignment
- OS page size = 4KB — smallest unit OS reads/writes from disk
- 4KB-aligned offset: byte position is a multiple of 4096
- Formula: `(rand() % (FILE_SIZE / FOUR_KB)) * FOUR_KB`
- Writing only 2KB to a 4KB-aligned position forces a **read-modify-write**: OS must read the full 4KB page, modify 2KB, then write back

### C library (stdio) vs System calls for random access
- stdio prefetches data into user buffer anticipating sequential access
- For **random access**, this prefetch is wasted → extra overhead
- System calls (`lseek` + `read`) read exactly what you ask for — better for random patterns

### srand() / rand()
- PRNG: deterministic sequence based on a seed
- `srand(time(NULL))` — sets seed using current time (different each run)
- Call `srand()` **once** only — calling it each iteration resets the sequence

---

## The 5 I/O Patterns (and why they differ in speed)

| Pattern | Why fast/slow |
|---|---|
| Sequential Read | OS prefetch helps, data read in order |
| Sequential Write | Batched by page cache, flushed once at end |
| Random Read | Each read seeks to a different location — cache misses hurt |
| Random Buffered Write | 50,000 writes batched in page cache, one fsync at end |
| Random Sync Write | fsync after every write → 50,000 disk flushes → slowest |

---

## HW Status

- **HW1_1** ✓ — C library interface (`fopen`, `fread`, `fwrite`, `fseek`, `fclose`)
- **HW1_2** ✓ — System call interface (`open`, `read`, `write`, `lseek`, `close`)
- **HW1_3** ✗ — Memory-mapped I/O (`mmap`, pointer access, `munmap`)
- **HW1_4** ✗ — Word report comparing all three, with cold-cache measurements

**Deadline: 2026/4/1 23:30**

---

## mmap (for HW1_3) — quick overview

Instead of `read()`/`write()`, you map the file directly into your process's address space and access it like a pointer/array.

```c
// setup
int fd = open("test.txt", O_RDWR);
char *map = mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

// read: just dereference the pointer
memcpy(buffer, map + offset, FOUR_KB);

// write: just write to the pointer
memcpy(map + offset, buffer, TWO_KB);

// cleanup
munmap(map, FILE_SIZE);
close(fd);
```

No `lseek` needed — offset is just pointer arithmetic.
For sync write: `fsync(fd)` after each write (fd still needed for fsync).
