# Facebetter SDK drop-in (Phase 2)

Proprietary SDK - NEVER committed (see sdk/README.md and the root .gitignore).

Expected layout for HAOCAM_ENABLE_FACEBETTER=ON (official quick start,
docs.facebetter.net/windows/quick-start):

    sdk/facebetter/
    ├── include/
    │   └── facebetter/
    │       ├── beauty_effect_engine.h
    │       ├── beauty_params.h
    │       ├── image_frame.h
    │       └── type_defines.h
    ├── lib/
    │   ├── facebetter.lib
    │   └── facebetter.dll
    └── resource/
        └── resource.fbd        (models/assets - required by the engine)

Credentials go into %LOCALAPPDATA%/HaoCam/config.json (see
config.example.json). resource_path defaults to
sdk/facebetter/resource/resource.fbd when left empty.

Without the drop-in, HaoCam builds fine and Beauty reports Unavailable.
