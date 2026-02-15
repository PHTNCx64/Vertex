# Overview

Vertex utilizes enumerations to determine the status of a function. This allows for better readability and to determine
better when and where a function succeeds or fails.

# Plugin API Status Codes

This document describes the status codes returned by Plugin API functions. All API functions return a `StatusCode`
enumeration value to indicate the result of the operation.

## Status Code List

### General Status

| Enumerator                                   | Description                                                  |
|:---------------------------------------------|:-------------------------------------------------------------|
| `STATUS_OK`                                  | Operation completed successfully.                            |
| `STATUS_ERROR_GENERAL`                       | Generic error - operation failed for unspecified reason.     |
| `STATUS_ERROR_GENERAL_UNSUPPORTED_OPERATION` | The requested operation is not supported.                    |
| `STATUS_ERROR_GENERAL_ALREADY_EXISTS`        | The item already exists.                                     |
| `STATUS_ERROR_GENERAL_NOT_FOUND`             | The item was not found.                                      |
| `STATUS_ERROR_GENERAL_OUT_OF_BOUNDS`         | Index or value out of bounds.                                |
| `STATUS_ERROR_NOT_IMPLEMENTED`               | Functionality not implemented.                               |
| `STATUS_ERROR_FEATURE_DEACTIVATED`           | Feature is currently deactivated.                            |
| `STATUS_ERROR_INSUFFICIENT_PERMISSION`       | Insufficient permissions to perform the requested operation. |
| `STATUS_ERROR_INVALID_HANDLE`                | Invalid or corrupted handle provided.                        |
| `STATUS_ERROR_INVALID_PARAMETER`             | One or more parameters are invalid or out of range.          |
| `STATUS_ERROR_INVALID_STATE`                 | Object is in an invalid state for the operation.             |
| `STATUS_ERROR_NO_VALUES_FOUND`               | No matching values or data found.                            |
| `STATUS_ERROR_FUNCTION_NOT_FOUND`            | Requested function does not exist or is not available.       |
| `STATUS_ERROR_TIMEOUT`                       | The operation timed out.                                     |
| `STATUS_ERROR_QUEUE_FULL`                    | The internal event queue is full.                            |

### File & Filesystem

| Enumerator                                       | Description                           |
|:-------------------------------------------------|:--------------------------------------|
| `STATUS_ERROR_FILE_CONFIGURATION_INVALID`        | Configuration file is invalid.        |
| `STATUS_ERROR_FILE_CREATION_FAILED`              | Unable to create the specified file.  |
| `STATUS_ERROR_FILE_NOT_FOUND`                    | Specified file does not exist.        |
| `STATUS_ERROR_FILE_MAPPING_FAILED`               | Failed to map file to memory.         |
| `STATUS_ERROR_FILE_UNMAPPING_FAILED`             | Failed to unmap file.                 |
| `STATUS_ERROR_FILE_RESIZE_FAILED`                | Failed to resize file.                |
| `STATUS_ERROR_FILE_TRIM_FAILED`                  | Failed to trim file.                  |
| `STATUS_ERROR_FILE_SYNC_FAILED`                  | Failed to sync file to disk.          |
| `STATUS_ERROR_FILE_MAP_RESIZE_NOT_REQUIRED`      | File map resize was not required.     |
| `STATUS_ERROR_FILE_READ_FAILED`                  | Failed to read file.                  |
| `STATUS_ERROR_FILE_WRITE_FAILED`                 | Failed to write to file.              |
| `STATUS_ERROR_DIRECTORY_CREATION_FAILED`         | Failed to create directory.           |
| `STATUS_ERROR_FS_DIR_CREATION_FAILED`            | Filesystem directory creation failed. |
| `STATUS_ERROR_FS_FILE_INVALID_CONTENT`           | File contains invalid content.        |
| `STATUS_ERROR_FS_FILE_INVALID_OFFSET`            | Invalid file offset.                  |
| `STATUS_ERROR_FS_FILE_OPEN_FAILED`               | Failed to open file.                  |
| `STATUS_ERROR_FS_FILE_READ_FAILED`               | Failed to read from file.             |
| `STATUS_ERROR_FS_FILE_WRITE_FAILED`              | Failed to write to file.              |
| `STATUS_ERROR_FS_DIRECTORY_COULD_NOT_BE_CREATED` | Directory could not be created.       |
| `STATUS_ERROR_FS_FILE_COULD_NOT_BE_SAVED`        | File could not be saved.              |
| `STATUS_ERROR_FS_FILE_COULD_NOT_BE_OPENED`       | File could not be opened.             |
| `STATUS_ERROR_FS_UNEXPECTED_FILE_TYPE`           | Unexpected file type encountered.     |

### JSON Operations

| Enumerator                               | Description                                   |
|:-----------------------------------------|:----------------------------------------------|
| `STATUS_ERROR_FS_JSON_KEY_NOT_FOUND`     | JSON key does not exist.                      |
| `STATUS_ERROR_FS_JSON_PARSE_FAILED`      | Failed to parse JSON content.                 |
| `STATUS_ERROR_FS_JSON_TYPE_MISMATCH`     | JSON value type does not match expected type. |
| `STATUS_ERROR_JSON_SERIALIZATION_FAILED` | Failed to serialize data to JSON.             |

### Memory Operations

| Enumerator                              | Description                                    |
|:----------------------------------------|:-----------------------------------------------|
| `STATUS_ERROR_MEMORY_ALLOCATION_FAILED` | Unable to allocate required memory.            |
| `STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL`  | Provided buffer is too small.                  |
| `STATUS_ERROR_MEMORY_OPERATION_ABORTED` | Memory operation was interrupted or cancelled. |
| `STATUS_ERROR_MEMORY_OUT_OF_BOUNDS`     | Accessing memory out of bounds.                |
| `STATUS_ERROR_MEMORY_READ`              | Failed to read from memory location.           |
| `STATUS_ERROR_MEMORY_WRITE`             | Failed to write to memory location.            |
| `STATUS_ERROR_MEMORY_READ_FAILED`       | Memory read operation failed.                  |
| `STATUS_ERROR_MEMORY_WRITE_FAILED`      | Memory write operation failed.                 |
| `STATUS_ERROR_OUT_OF_MEMORY`            | System out of memory.                          |

### Process Operations

| Enumerator                           | Description                                  |
|:-------------------------------------|:---------------------------------------------|
| `STATUS_ERROR_PROCESS_ACCESS_DENIED` | Access to the target process was denied.     |
| `STATUS_ERROR_PROCESS_INVALID`       | Process handle is invalid.                   |
| `STATUS_ERROR_PROCESS_MISMATCH`      | Process does not match the expected target.  |
| `STATUS_ERROR_PROCESS_NOT_FOUND`     | Target process could not be found.           |
| `STATUS_ERROR_PROCESS_OPEN_INVALID`  | Failed to open or access the target process. |

### Plugin System

| Enumerator                                        | Description                                        |
|:--------------------------------------------------|:---------------------------------------------------|
| `STATUS_ERROR_PLUGIN_ALREADY_LOADED`              | Plugin is already loaded.                          |
| `STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED`    | Required plugin function is missing.               |
| `STATUS_ERROR_PLUGIN_LOAD_FAILED`                 | Plugin failed to load.                             |
| `STATUS_ERROR_PLUGIN_NOT_ACTIVE`                  | Plugin is not in an active state.                  |
| `STATUS_ERROR_PLUGIN_NOT_FOUND`                   | Plugin could not be found.                         |
| `STATUS_ERROR_PLUGIN_NOT_LOADED`                  | Plugin has not been loaded.                        |
| `STATUS_ERROR_PLUGIN_RESOLVE_FAILURE`             | Failed to resolve plugin symbols.                  |
| `STATUS_ERROR_PLUGIN_NO_PLUGIN_ACTIVE`            | No plugin is currently active.                     |
| `STATUS_ERROR_PLUGIN_NO_MEMORY_ATTRIBUTE_OPTIONS` | No memory attribute options available from plugin. |

### Debugger

| Enumerator                               | Description                                                  |
|:-----------------------------------------|:-------------------------------------------------------------|
| `STATUS_ERROR_DEBUGGER_NOT_ATTACHED`     | Debugger is not attached to a process.                       |
| `STATUS_ERROR_DEBUGGER_NOT_RUNNING`      | Debugger is not currently running.                           |
| `STATUS_ERROR_DEBUGGER_ALREADY_RUNNING`  | Debugger loop is already running.                            |
| `STATUS_ERROR_DEBUGGER_ALREADY_ATTACHED` | Debugger is already attached to a process.                   |
| `STATUS_ERROR_DEBUGGER_ATTACH_PENDING`   | A debugger attach operation is already pending.              |
| `STATUS_ERROR_DEBUGGER_ATTACH_FAILED`    | Failed to attach debugger to the target process.             |
| `STATUS_ERROR_DEBUGGER_DETACH_FAILED`    | Failed to detach debugger from the process.                  |
| `STATUS_ERROR_DEBUGGER_INVALID_STATE`    | Debugger is in an invalid state for the requested operation. |
| `STATUS_ERROR_DEBUGGER_NO_DEBUG_EVENT`   | No debug event available.                                    |
| `STATUS_ERROR_DEBUGGER_CONTINUE_FAILED`  | Failed to continue execution.                                |
| `STATUS_ERROR_DEBUGGER_BREAK_FAILED`     | Failed to break/pause execution.                             |
| `STATUS_ERROR_DEBUGGER_CONTEXT_FAILED`   | Failed to get or set thread context.                         |
| `STATUS_ERROR_DEBUGGER_PROCESS_DEAD`     | The debugged process has terminated.                         |

### Breakpoints

| Enumerator                                   | Description                                  |
|:---------------------------------------------|:---------------------------------------------|
| `STATUS_ERROR_BREAKPOINT_NOT_FOUND`          | Specified breakpoint does not exist.         |
| `STATUS_ERROR_BREAKPOINT_LIMIT_REACHED`      | Maximum number of breakpoints reached.       |
| `STATUS_ERROR_BREAKPOINT_ADDRESS_MISALIGNED` | Breakpoint address is not properly aligned.  |
| `STATUS_ERROR_BREAKPOINT_ALREADY_EXISTS`     | A breakpoint already exists at this address. |
| `STATUS_ERROR_BREAKPOINT_SET_FAILED`         | Failed to set the breakpoint.                |

### Registers

| Enumerator                           | Description                        |
|:-------------------------------------|:-----------------------------------|
| `STATUS_ERROR_REGISTER_WRITE_FAILED` | Failed to write register value.    |
| `STATUS_ERROR_REGISTER_NOT_FOUND`    | Specified register does not exist. |

### Threads

| Enumerator                           | Description                          |
|:-------------------------------------|:-------------------------------------|
| `STATUS_ERROR_THREAD_NOT_FOUND`      | Specified thread could not be found. |
| `STATUS_ERROR_THREAD_SUSPEND_FAILED` | Failed to suspend the thread.        |
| `STATUS_ERROR_THREAD_RESUME_FAILED`  | Failed to resume the thread.         |
| `STATUS_ERROR_THREAD_IS_BUSY`        | Thread is currently busy.            |
| `STATUS_ERROR_THREAD_IS_NOT_RUNNING` | Thread is not in a running state.    |
| `STATUS_ERROR_THREAD_INVALID_TASK`   | Invalid task assigned to thread.     |
| `STATUS_ERROR_THREAD_INVALID_ID`     | Invalid thread identifier.           |
| `STATUS_ERROR_THREAD_CONTEXT_FAILED` | Failed to get or set thread context. |

### Formatting & Strings

| Enumerator                                | Description                                      |
|:------------------------------------------|:-------------------------------------------------|
| `STATUS_ERROR_FMT_EMPTY_DATA`             | Formatting input data is empty.                  |
| `STATUS_ERROR_FMT_INVALID_CONVERSION`     | Invalid format conversion.                       |
| `STATUS_ERROR_FMT_INVALID_FORMAT`         | Invalid format string.                           |
| `STATUS_ERROR_STRING_INVALID_CHARACTER`   | String contains invalid characters.              |
| `STATUS_ERROR_STRING_NO_CHARACTERS_FOUND` | String is empty or contains no valid characters. |


### Library

| Enumerator                     | Description                 |
|:-------------------------------|:----------------------------|
| `STATUS_ERROR_LIBRARY_INVALID` | Invalid or corrupt library. |

## Usage Examples

### Checking Return Values

```c
StatusCode vertex_init(PluginInformation* pluginInfo, Runtime* runtime)
{
    return STATUS_OK;
}
```

### Error Handling Pattern

```c
static Runtime* pVertexRuntime;

const char* status_to_string(StatusCode code)
{
    switch (code)
    {
        case STATUS_OK: return "OK";
        case STATUS_ERROR_GENERAL: return "General Error";
        case STATUS_ERROR_DEBUGGER_NOT_ATTACHED: return "Debugger Not Attached";
        case STATUS_ERROR_BREAKPOINT_NOT_FOUND: return "Breakpoint Not Found";
        default: return "Unknown";
    }
}
```
