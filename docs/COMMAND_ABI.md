# Graphics command ABI (v0.28.0)

Historical synchronous interface; v0.29 adds [asynchronous jobs and same-size updates](ASYNC_ABI.md).

This is a trusted native C ABI extension, not Xbox D3D12.X or a security sandbox. The five exported functions and ABI v1 request layout are unchanged. The Windows `RV_GFX_CUSTOM` flag reports a compiled backend, not hardware availability.

## Operations

| Opcode | Request fields | Buffer | Result |
|---|---|---|---|
| 20 `RV_RES_CREATE` | arg0 = kind, count = bytes, arg1 = texture width, offset = texture height (zero for other kinds) | input bytes | opaque resource handle |
| 21 `RV_RES_RELEASE` | handle | none | 0 |
| 22 `RV_CMD_VALIDATE` | command handle | none | draw count |
| 23 `RV_CMD_SUBMIT` | command handle, arg0 = width, arg1 = height | exact width × height × 4 output bytes | bytes written |
| 24 `RV_RES_COUNT` | no operands | none | live resource count |

Unused fields are zero. All input data is copied into immutable session storage. Resource kinds: 1 = 20-byte `rv_vertex` (five FP32 x,y,z,u,v, finite, positive z); 2 = uint16 indices; 3 = tightly packed RGBA8, width/height 1–512; 4 = 40-byte `rv_draw_header` plus 1–1,024 8-byte `rv_indexed_draw` records. Header holds vertex/index/texture/package handles, draw count, reserved zero. All records use native little-endian x64 layout. Each draw specifies first index and count, count divisible by three. Every index must reference an existing vertex. Only one mesh, texture and known RSP1 package per command list. Existing package handles are created through `RV_GFX_LOAD`. Command payload shape is checked at creation; references and draw bounds are checked at validation and submission.

Maximum 64 resource handles, 8 MiB total payload per session (separate from application memory and package budgets), 1 MiB per transfer. Releasing a referenced resource invalidates future validation/submission; handles are not reused. STOP clears all resources and packages. Calls serialize within the graphics session; do not close while calls are active. Linux validates but returns `RV_UNSUPPORTED` for a valid GPU submission.

## Rendering and tests

Windows custom render creates a device, root signature, fixed translated VS/PS, texture upload, vertex/index uploads and one direct command list. The pass clears colour/depth, binds one mesh and texture, emits the client draw ranges, transitions to copy source, reads back and waits on a fence before copying tightly packed RGBA8 to the client. The custom path does not execute the package's compute stage (the caller supplies texture bytes). It does not use the fixed-scene oracle. The separate test client checks two differently coloured draws with depth overlap, and submits a second command list using a replacement texture. The portable path exercises the client's reference checker and corruption rejection, not a software GPU simulation.

The output limit is width 320–512, height 240–512, chosen to stay within the existing transfer limit. No persistent GPU device/resource allocation across submissions, general pipeline creation, mutable textures, asynchronous queue, fence handles, multi-frame custom presentation or Xbox ABI. The previous continuous fixture remains unchanged in intent. Windows compilation and actual custom GPU framebuffer verification require hardware testing.

## Host resource transfers (v0.56.0)

Capability `RV_GFX_RESOURCE_TRANSFER` advertises five additive operations. `RV_RES_INFO` returns kind, byte length, width and height in an exact `rv_resource_info` buffer. `RV_RES_READ` reads a bounded byte range. `RV_RES_CLONE` duplicates a resource payload into a new session-local handle (command references remain unchanged). `RV_RES_COPY` copies matching non-command resource kinds with aligned byte ranges; overlapping self-copies are supported. `RV_RES_USAGE` reports live resource bytes, including zero after stop. All unused fields must be zero. See `native_abi.h` for field mapping. These are host snapshots, not GPU resource barriers or asynchronous DMA.
