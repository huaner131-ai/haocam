# Tools

* diagnostics/ - developer overlay lives in-app (F3); extended harnesses arrive with later phases.
* shader_compiler/ - Phase 1 compiles HLSL at runtime via D3DCompile (shaders are embedded by cmake/EmbedFile.cmake); an offline fxc/dxc pipeline lands when D3D12 support is added.
* asset_packer/ - planned for LUT/sticker/lens packaging (later phases).
