# Vertex AngelScript Scripting API

This document describes the AngelScript API that Vertex exposes to user scripts.
It is based on the current runtime registration in:

- `src/vertex/scripting/angelscript.cc`
- `src/vertex/scripting/stdlib/*`

## Examples

Examples can be found under `resources/templates`

## Quick Start

1. Open the Scripting view in Vertex.
2. Create a script (`.vscr`) in the runtime `Scripts/` directory (or open an existing one).
3. Implement `void main()`.
4. Execute it from the Scripting toolbar.

Minimal script:

```angelscript
void main()
{
    log_info("Hello from Vertex script");
}
```

## Execution Model

- A script context starts at `void main()`. If `main` is missing, execution fails.
- Script files are limited to 1 MiB.
- Scripts started from the Scripting view run against the current active plugin context.
- Execution is cooperative and time-sliced. Long loops should call one of:
  - `yield()`
  - `sleep(uint milliseconds)`
  - `wait_ticks(uint ticks)`
  - `wait_until(CoroutineCondition@ condition)`
  - `wait_while(CoroutineCondition@ condition)`
- The scripting editor uses 1-based line numbers for breakpoints.
- Variable inspection is available when a context is suspended.

## Built-In Script Types

### `string`

Vertex registers a custom `string` type with:

- assignment/equality
- concatenation with `string`, `int`, `uint`, `float`, `double`, `bool`
- `uint length() const`
- `bool is_empty() const`

### `array<T>`

AngelScript `array<T>` support is registered (scriptarray add-on).

## Global Functions

### Coroutine Helpers

```angelscript
void yield()
void sleep(uint milliseconds)
void wait_until(CoroutineCondition@ condition)
void wait_while(CoroutineCondition@ condition)
void wait_ticks(uint ticks)
```

Also available:

```angelscript
funcdef bool CoroutineCondition()
```

### Logging

```angelscript
void log_info(const string &in message)
void log_warn(const string &in message)
void log_error(const string &in message)
```

### Utility

```angelscript
string get_current_plugin()
string get_host_os()
string get_version()
string get_vendor()
string get_name()
```

### Process API

```angelscript
bool is_process_open()
string get_process_name()
uint get_process_id()
int close_process()
int kill_process()
int open_process(uint processId)
int refresh_process_list()
uint get_process_count()
ProcessInfo get_process_at(uint index)
int refresh_modules_list()
uint get_module_count()
ModuleInfo get_module_at(uint index)
```

Notes:

- Functions returning `int` return `StatusCode` values.
- `get_process_at` / `get_module_at` return default/empty structs for invalid indices.

### Memory API

```angelscript
int read_memory(uint64 address, uint size, string &out data)
int write_memory(uint64 address, const string &in data)
int bulk_read(array<BulkReadEntry>@ entries, array<BulkReadResult>@ &out results)
int bulk_write(array<BulkWriteEntry>@ entries, array<BulkWriteResult>@ &out results)
int allocate_memory(uint64 address, uint64 size, uint64 &out resultAddress)
int free_memory(uint64 address, uint64 size)
int get_pointer_size(uint64 &out pointerSize)
int get_min_address(uint64 &out minAddress)
int get_max_address(uint64 &out maxAddress)
int refresh_memory_regions()
uint get_region_count()
MemoryRegion get_region_at(uint index)
```

Notes:

- `string` payloads in `read_memory` / `write_memory` are raw bytes.
- Bulk operations fill per-entry result status (`BulkReadResult.status`, `BulkWriteResult.status`).
- `get_region_at` returns a default/empty `MemoryRegion` for invalid indices.

### UI API

Factories:

```angelscript
Frame@ ui_create_frame(const string &in title, int width, int height)
Dialog@ ui_create_dialog(const string &in title, int width, int height)
BoxSizer@ ui_create_box_sizer(int orientation)
void ui_message_box(const string &in message, const string &in caption, int flags)
```

Constants:

```angelscript
const int UI_VERTICAL
const int UI_HORIZONTAL
const int UI_EXPAND
const int UI_ALL
const int UI_LEFT
const int UI_RIGHT
const int UI_TOP
const int UI_BOTTOM
const int UI_CENTER
const int UI_OK
const int UI_OK_CANCEL
const int UI_YES_NO
const int UI_ICON_INFO
const int UI_ICON_WARNING
const int UI_ICON_ERROR
const int UI_ICON_QUESTION
```

Object APIs:

```angelscript
// Frame
void show()
void hide()
void close()
void set_title(const string &in title)
void set_size(int width, int height)
void set_sizer(BoxSizer@ sizer)
Panel@ create_panel()
int from_dip(int value)
bool is_open()

// Dialog
void show()
void hide()
int show_modal()
void end_modal(int returnCode)
void close()
void set_title(const string &in title)
void set_size(int width, int height)
void set_sizer(BoxSizer@ sizer)
Panel@ create_panel()
int from_dip(int value)
bool is_open()

// Panel
Button@ add_button(const string &in label)
TextInput@ add_text_input(const string &in value)
CheckBox@ add_checkbox(const string &in label)
ComboBox@ add_combobox()
Label@ add_label(const string &in text)
Panel@ add_panel()
GroupBox@ create_group_box(const string &in title, int orientation)
void set_sizer(BoxSizer@ sizer)
int from_dip(int value)

// Button
void set_label(const string &in label)
string get_label()
void enable(bool enabled)
bool is_enabled()

// TextInput
void set_value(const string &in value)
string get_value()
void set_read_only(bool readOnly)
void enable(bool enabled)
bool is_enabled()

// CheckBox
void set_checked(bool checked)
bool is_checked()
void set_label(const string &in label)
string get_label()
void enable(bool enabled)
bool is_enabled()

// ComboBox
void add_item(const string &in item)
void clear_items()
int get_selection()
void set_selection(int index)
string get_value()
void enable(bool enabled)
bool is_enabled()

// Label
void set_text(const string &in text)
string get_text()

// BoxSizer
void add(Panel@, int proportion, int flags, int border)
void add(Button@, int proportion, int flags, int border)
void add(TextInput@, int proportion, int flags, int border)
void add(CheckBox@, int proportion, int flags, int border)
void add(ComboBox@, int proportion, int flags, int border)
void add(Label@, int proportion, int flags, int border)
void add(BoxSizer@, int proportion, int flags, int border)
void add(GroupBox@, int proportion, int flags, int border)
void add_spacer(int size)
void add_stretch(int proportion)

// GroupBox
void add(Panel@, int proportion, int flags, int border)
void add(Button@, int proportion, int flags, int border)
void add(TextInput@, int proportion, int flags, int border)
void add(CheckBox@, int proportion, int flags, int border)
void add(ComboBox@, int proportion, int flags, int border)
void add(Label@, int proportion, int flags, int border)
void add(BoxSizer@, int proportion, int flags, int border)
void add_spacer(int size)
void add_stretch(int proportion)
```

Current UI limitation:

- There are no button/checkbox/combobox event callbacks in the script API yet.
- UI scripts should poll values in a loop and use `yield()` / `sleep(...)`.

## Script Struct Types

- `ProcessInfo`: `name`, `owner`, `id`, `parentId`
- `ModuleInfo`: `name`, `path`, `baseAddress`, `size`
- `BulkReadEntry`: `address`, `size`
- `BulkReadResult`: `status`, `data`
- `BulkWriteEntry`: `address`, `data`
- `BulkWriteResult`: `status`
- `MemoryRegion`: `moduleName`, `baseAddress`, `regionSize`

## `StatusCode` Enum Values Exposed To Scripts

Vertex registers this subset in scripts:

- `STATUS_OK`
- `STATUS_ERROR_GENERAL`
- `STATUS_ERROR_INVALID_PARAMETER`
- `STATUS_ERROR_NOT_IMPLEMENTED`
- `STATUS_ERROR_FUNCTION_NOT_FOUND`
- `STATUS_ERROR_PLUGIN_NOT_ACTIVE`
- `STATUS_ERROR_PLUGIN_NOT_LOADED`
- `STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED`
- `STATUS_ERROR_PROCESS_ACCESS_DENIED`
- `STATUS_ERROR_PROCESS_INVALID`
- `STATUS_ERROR_PROCESS_NOT_FOUND`
- `STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL`
- `STATUS_ERROR_MEMORY_OPERATION_ABORTED`
- `STATUS_ERROR_MEMORY_READ`
- `STATUS_ERROR_MEMORY_WRITE`
- `STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED`

## First Scripts

### 1) Hello World

```angelscript
void main()
{
    log_info("Script started");
}
```

### 2) Process List Snapshot

```angelscript
void main()
{
    int status = refresh_process_list();
    if (status != STATUS_OK)
    {
        log_error("refresh_process_list failed: " + status);
        return;
    }

    uint count = get_process_count();
    log_info("Process count: " + count);

    for (uint i = 0; i < count; ++i)
    {
        ProcessInfo p = get_process_at(i);
        log_info("[" + i + "] pid=" + p.id + " name=" + p.name);
    }
}
```

### 3) Memory Read Example

```angelscript
void main()
{
    if (!is_process_open())
    {
        log_warn("No process is open");
        return;
    }

    uint64 address = 0x1000;
    string data;
    int status = read_memory(address, 16, data);
    if (status != STATUS_OK)
    {
        log_error("read_memory failed: " + status);
        return;
    }

    log_info("Read " + data.length() + " bytes");
}
```

### 4) Basic UI Window

```angelscript
void main()
{
    Frame@ frame = ui_create_frame("Vertex UI Script", 480, 320);
    Panel@ panel = frame.create_panel();

    int pad = panel.from_dip(6);
    Label@ label = panel.add_label("Running...");
    Button@ button = panel.add_button("Close window manually");

    BoxSizer@ sizer = ui_create_box_sizer(UI_VERTICAL);
    sizer.add(label, 0, UI_ALL | UI_EXPAND, pad);
    sizer.add(button, 0, UI_ALL | UI_EXPAND, pad);
    panel.set_sizer(sizer);
    frame.show();

    while (frame.is_open())
    {
        label.set_text("Tick");
        sleep(250);
    }
}
```

## Troubleshooting

- `STATUS_ERROR_SCRIPT_COMPILE_FAILED`:
  - Check diagnostics in the Scripting output panel.
- `STATUS_ERROR_SCRIPT_PREPARE_FAILED`:
  - Confirm `void main()` exists and has the exact signature.
- `STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED`:
  - Engine/registration startup failed; check application logs.
- `STATUS_ERROR_PLUGIN_NOT_ACTIVE` / `STATUS_ERROR_PLUGIN_NOT_LOADED`:
  - Process/memory APIs require an active loaded plugin.

## Quick Notes

AngelScript utilizes a C++-style syntax.

Keep in mind that memory safety is not a concern of the scripter.

Vertex does the heavy lifting for you.