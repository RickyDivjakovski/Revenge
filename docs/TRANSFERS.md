# Resource transfer fixture (v0.16.0)

Single producer, single native D3D12 copy queue. One fixed triplet per slot: upload (GENERIC_READ), device/default (COMMON at each batch start), readback (COPY_DEST). Each list records COMMON -> COPY_DEST, upload, COPY_DEST -> COPY_SOURCE, readback. Buffer state decays at the end of each distinct ExecuteCommandLists operation. A native fence follows each submitted list. The CPU waits before readback or reusing the command allocator/upload buffer. The portable Pool tracks legal recording order and the monotonic fence timeline; final consumption retires each triplet. The VM arena stages bytes in both directions. Pattern generation and full verification use the submission sequence; every readback is checked. Peak outstanding means submitted but not consumed, not necessarily simultaneously executing on hardware.

`gpu-transfer-test [rounds=64] [slots=3] [MiB=4]`: 1–1024 rounds, 1–8 slots, 1–32 MiB per buffer, rounds >= slots. Maximum logical buffer payload is 768 MiB, further limited to half the reported available local budget. Host staging capacity is slots * bytes. Actual allocation and residency are controlled by the OS/driver. Timing includes CPU work, not raw PCIe throughput. The existing renderer and compute fixture are not migrated to this pool. No cross-queue synchronization, arbitrary resource dispatch, texture transfers, actual shared virtual addressing or console ABI. Error paths attempt a bounded queue drain; device removal/timeouts cannot guarantee successful draining. The Windows path is not tested in the local Linux environment.

Public references:
- https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
- https://learn.microsoft.com/en-us/windows/win32/direct3d12/readback-data-using-heaps
- https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management
- https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization
