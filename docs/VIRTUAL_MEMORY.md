# Native virtual memory foundation (v0.15.0)

`revenge::vm::Arena` owns a private, non-executable host address reservation. It is not an Xbox API implementation or a guest address mapper. The existing CPU `MemoryArena` page/alias model is unchanged.

- Windows: `VirtualAlloc(MEM_RESERVE/MEM_COMMIT)`, `VirtualProtect`, `VirtualFree(MEM_DECOMMIT/MEM_RELEASE)`.
- Linux: anonymous private `mmap(PROT_NONE)`, `mprotect`, `madvise(MADV_DONTNEED)`, `munmap`. Linux commitment is logical accounting, not a guarantee against OS overcommit or OOM.
- Page size is queried from the host. Capacity and commit budget must be page multiples, capacity at most 64 GiB. Allocation sizes round up to pages; power-of-two alignment from page size up to 1 GiB is supported subject to available address space.
- First-fit suballocation and implicit gap coalescing; stats include logical reserved/committed bytes, peak committed, free bytes, largest free range and retirement counts. The entire host arena stays reserved until destruction.
- Handles carry an offset and process-local generation; stale and foreign handles are rejected. No raw host pointers are exported. All operations serialize on one mutex. Checked reads/writes validate the whole range before copying. Page operations require aligned nonempty ranges. Only committed pages may be protected/decommitted; only reserved pages may be committed. Read-only, read-write and committed no-access states are supported. Recommit yields zero-filled data. No executable permissions.
- Retirement defers reclamation until a caller-supplied monotonically increasing logical fence is collected. Retired handles are unavailable to callers but retain their reservation and committed budget until collection. This is a CPU bookkeeping model, **not** an actual D3D12 fence or asynchronous transfer implementation.
- Invalid requests are rejected before mutation. An OS memory operation failure disables the arena; destruction releases the backing reservation. A budget is a project limit, not a system memory availability guarantee. Destruction must not race with callers.

## Run

```
Revenge.exe vm-bench 8 4
ctest --test-dir build -C Release --output-on-failure
```

The benchmark accepts 1–256 MiB per worker, 1–16 workers, and at most 512 MiB total payload. It allocates, commits, writes, reads, compares and releases each worker's buffer. Time includes allocation, copies, verification and scheduling; it is not raw RAM bandwidth or game performance. Temporary source and destination buffers require additional memory. The suite tests protections via the checked interface, not deliberately faulting native pointers.

## Public references

- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree
- https://learn.microsoft.com/en-au/windows/win32/memory/memory-management-functions
- https://www.man7.org/linux/man-pages/man2/madvise.2.html

No console ABI, native Xbox address mapping, GPU shared memory/coherence, memory API import dispatch, actual GPU resource retirement or async resource transfers are implemented by this release.
