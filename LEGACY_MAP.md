# Legacy Code Map

## SocketManager header/body pipeline
- **Original location:** `srcs/server/SocketManager.cpp`
- **Moved to:** `srcs/legacy/legacy_SocketManager_pipeline.cpp`
- **Companion header:** `includes/legacy/legacy_SocketManager_pipeline.hpp`
- **Reason:** superseded by the new `SocketManagerHttp.cpp` header/body pipeline. Retained for reference but unused by the active request path.
- **To re-enable:** add `srcs/legacy/legacy_SocketManager_pipeline.cpp` to the build and include `includes/legacy/legacy_SocketManager_pipeline.hpp` (or ensure `SocketManager.hpp` declarations remain available).

## Legacy ServerConfig class
- **Original header:** `includes/to_delete_ServerConfig.hpp`
- **Moved to:** `includes/legacy/legacy_ServerConfig.hpp`
- **Original source:** `srcs/cfg/ServerConfig.cpp`
- **Moved to:** `srcs/legacy/legacy_ServerConfig.cpp`
- **Reason:** superseded by the simplified structs in `includes/Config.hpp`; not referenced by the active configuration parser.
- **To re-enable:** add `srcs/legacy/legacy_ServerConfig.cpp` to the build and include `legacy/legacy_ServerConfig.hpp` from interested translation units.
