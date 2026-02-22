//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#include <stdint.h>
#include "macro.h"
#include "statuscode.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===============================================================================================================//
// UI CONSTANTS                                                                                                   //
// ===============================================================================================================//

#define VERTEX_UI_MAX_FIELD_ID_LENGTH 64
#define VERTEX_UI_MAX_LABEL_LENGTH 128
#define VERTEX_UI_MAX_TOOLTIP_LENGTH 256
#define VERTEX_UI_MAX_STRING_VALUE_LENGTH 260
#define VERTEX_UI_MAX_OPTION_LABEL_LENGTH 128
#define VERTEX_UI_MAX_PANEL_ID_LENGTH 64
#define VERTEX_UI_MAX_PANEL_TITLE_LENGTH 128
#define VERTEX_UI_MAX_SECTION_TITLE_LENGTH 128

// ===============================================================================================================//
// UI FIELD TYPES                                                                                                 //
// ===============================================================================================================//

typedef enum VertexUIFieldType : int32_t
{
    VERTEX_UI_FIELD_TEXT = 0,
    VERTEX_UI_FIELD_NUMBER_INT,
    VERTEX_UI_FIELD_NUMBER_FLOAT,
    VERTEX_UI_FIELD_CHECKBOX,
    VERTEX_UI_FIELD_DROPDOWN,
    VERTEX_UI_FIELD_SLIDER_INT,
    VERTEX_UI_FIELD_SLIDER_FLOAT,
    VERTEX_UI_FIELD_PATH_FILE,
    VERTEX_UI_FIELD_PATH_DIR,
    VERTEX_UI_FIELD_SEPARATOR,
    VERTEX_UI_FIELD_LABEL,
    VERTEX_UI_FIELD_BUTTON
} UIFieldType;

// ===============================================================================================================//
// UI VALUE                                                                                                       //
// ===============================================================================================================//

typedef union VertexUIValue
{
    int64_t intValue;
    double floatValue;
    uint8_t boolValue; // bool
    char stringValue[VERTEX_UI_MAX_STRING_VALUE_LENGTH];
} UIValue;

// ===============================================================================================================//
// UI OPTION (for dropdowns)                                                                                      //
// ===============================================================================================================//

typedef struct VertexUIOption
{
    char label[VERTEX_UI_MAX_OPTION_LABEL_LENGTH];
    UIValue value;
} UIOption;

// ===============================================================================================================//
// UI LAYOUT ORIENTATION                                                                                          //
// ===============================================================================================================//

typedef enum VertexUILayoutOrientation : int32_t
{
    VERTEX_UI_LAYOUT_VERTICAL = 0,
    VERTEX_UI_LAYOUT_HORIZONTAL
} UILayoutOrientation;

// ===============================================================================================================//
// UI FIELD                                                                                                       //
// ===============================================================================================================//

typedef struct VertexUIField
{
    char fieldId[VERTEX_UI_MAX_FIELD_ID_LENGTH];
    char label[VERTEX_UI_MAX_LABEL_LENGTH];
    char tooltip[VERTEX_UI_MAX_TOOLTIP_LENGTH];
    UIFieldType type;
    UIValue defaultValue;
    UIValue minValue;
    UIValue maxValue;
    UIOption* options;
    uint32_t optionCount;
    uint8_t required; // bool
    uint8_t reserved[3];
    UILayoutOrientation layoutOrientation;
    int32_t layoutBorder;
    int32_t layoutProportion;
} UIField;

// ===============================================================================================================//
// UI SECTION                                                                                                     //
// ===============================================================================================================//

typedef struct VertexUISection
{
    char title[VERTEX_UI_MAX_SECTION_TITLE_LENGTH];
    UIField* fields;
    uint32_t fieldCount;
    uint8_t reserved[4];
} UISection;

// ===============================================================================================================//
// UI CALLBACKS                                                                                                   //
// ===============================================================================================================//

typedef void (VERTEX_API *VertexOnUIApply_t)(const char* fieldId, const UIValue* value, void* userData);
typedef void (VERTEX_API *VertexOnUIReset_t)(void* userData);

// ===============================================================================================================//
// UI PANEL                                                                                                       //
// ===============================================================================================================//

typedef struct VertexUIPanel
{
    char panelId[VERTEX_UI_MAX_PANEL_ID_LENGTH];
    char title[VERTEX_UI_MAX_PANEL_TITLE_LENGTH];
    UISection* sections;
    uint32_t sectionCount;
    VertexOnUIApply_t onApply;
    VertexOnUIReset_t onReset;
    void* userData;
    uint8_t reserved[8];
} UIPanel;

// ===============================================================================================================//
// UI REGISTRATION FUNCTION POINTER TYPES                                                                         //
// ===============================================================================================================//

typedef StatusCode (VERTEX_API *VertexRegisterUIPanel_t)(const UIPanel* panel);
typedef StatusCode (VERTEX_API *VertexGetUIValue_t)(const char* panelId, const char* fieldId, UIValue* outValue);

// ===============================================================================================================//
// UI REGISTRY FUNCTIONS (called by Vertex core to set instance, by plugins via Runtime struct)                   //
// ===============================================================================================================//

VERTEX_EXPORT StatusCode VERTEX_API vertex_ui_registry_set_instance(void* handle);
VERTEX_EXPORT void* VERTEX_API vertex_ui_registry_get_instance();

VERTEX_EXPORT StatusCode VERTEX_API vertex_register_ui_panel(const UIPanel* panel);
VERTEX_EXPORT StatusCode VERTEX_API vertex_get_ui_value(const char* panelId, const char* fieldId, UIValue* outValue);

#ifdef __cplusplus
}
#endif
