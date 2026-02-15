# Vertex Plugin SDK

This directory contains the public API headers for creating Vertex plugins.

## Licensing Exception

Plugins that use **only** these SDK headers may be distributed under any license
of your choosing, including proprietary and commercial licenses.

This exception is granted under section 7 of the GNU GPL version 3 that covers
Vertex itself. See `PLUGIN_EXCEPTION.txt` in src/vertex for complete terms.

## SDK Headers

The following headers define the public plugin API:

- **api.h** - Core API definitions and plugin interface
- **debugger.h** - Debugger functionality interface
- **disassembler.h** - Disassembly API
- **event.h** - Event system for plugin communication
- **log.h** - Logging interface
- **macro.h** - Common macros and definitions
- **memory.h** - Memory access and manipulation
- **process.h** - Process control and inspection
- **registry.h** - Registry/configuration access
- **statuscode.h** - Status codes and error handling

## Important Restrictions

✅ **Allowed:**
- Use any license for your plugin (MIT, Apache, proprietary, etc.)
- Distribute plugins commercially
- Keep plugin source code closed
- Link dynamically to Vertex at runtime

❌ **Not Allowed:**
- Using internal Vertex APIs outside of include/sdk/
- Copying Vertex implementation code into your plugin
- Modifying Vertex core and claiming the exception
- Static linking of plugins into Vertex executable

## Building Plugins

Your plugin should:

1. Include only headers from `include/sdk/`
2. Compile as a separate dynamic library (DLL/SO)
3. Export the required plugin interface functions
4. Be loaded by Vertex at runtime through the plugin system

## Example License Notice

You can include this notice in your plugin source:

```cpp
// This plugin for Vertex uses only the SDK API (include/sdk/)
// and is distributed under [YOUR LICENSE NAME].
// 
// This is permitted by the Vertex Plugin Exception.
// Vertex itself is licensed under GNU GPL version 3.
```

## Questions?

For questions about plugin licensing, see `PLUGIN_EXCEPTION.txt` or contact
the Vertex project maintainers.

---

**Note:** Vertex core remains under GNU GPL version 3. This exception applies
only to plugins that strictly adhere to the SDK API boundary.

