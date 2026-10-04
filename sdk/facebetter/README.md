# Facebetter SDK (vendored)

Vendored from the user's upload (repo `main`, folder `FacebetterSDK`) and
kept under the layout the HaoCam build expects:

    sdk/facebetter/include/facebetter/*.h   (public API headers)
    sdk/facebetter/lib/facebetter.lib       (link)
    sdk/facebetter/lib/facebetter.dll       (runtime - copied next to the exe by the build)
    sdk/facebetter/resource/*.fbd           (engine resources - copied next to the exe as facebetter_resources/)

Enable:

    cmake --preset windows-base -DHAOCAM_ENABLE_HUANFACE=ON -DHAOCAM_ENABLE_FACEBETTER=ON

config.json (%LOCALAPPDATA%\HaoCam\config.json):

    "facebetter": {
        "enabled": true,
        "app_id": "...",       // or leave empty and use license_token
        "app_key": "...",
        "license_token": "",
        "resource_path": ""    // empty = auto (<exeDir>/facebetter_resources/resource.fbd)
    }

When facebetter has credentials it takes priority over the HuanFace CPU
provider (GPU engine, full framerate, reshape features actually supported).
Without credentials the provider reports the exact reason in the Beauty
status line (honest Unavailable).
