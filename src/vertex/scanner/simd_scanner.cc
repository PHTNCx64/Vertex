//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/simd/simd_scanner.hh>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE <vertex/scanner/simd/simd_scanner-inl.hh>
#include <hwy/foreach_target.h>
#include <vertex/scanner/simd/simd_scanner-inl.hh>

#include <hwy/highway.h>

#if HWY_ONCE
namespace Vertex::Scanner::Simd
{
    HWY_EXPORT(simd_scan_exact_i8);
    HWY_EXPORT(simd_scan_exact_i16);
    HWY_EXPORT(simd_scan_exact_i32);
    HWY_EXPORT(simd_scan_exact_i64);
    HWY_EXPORT(simd_scan_exact_u8);
    HWY_EXPORT(simd_scan_exact_u16);
    HWY_EXPORT(simd_scan_exact_u32);
    HWY_EXPORT(simd_scan_exact_u64);
    HWY_EXPORT(simd_scan_exact_f32);
    HWY_EXPORT(simd_scan_exact_f64);

    HWY_EXPORT(simd_scan_greater_than_i8);
    HWY_EXPORT(simd_scan_greater_than_i16);
    HWY_EXPORT(simd_scan_greater_than_i32);
    HWY_EXPORT(simd_scan_greater_than_i64);
    HWY_EXPORT(simd_scan_greater_than_u8);
    HWY_EXPORT(simd_scan_greater_than_u16);
    HWY_EXPORT(simd_scan_greater_than_u32);
    HWY_EXPORT(simd_scan_greater_than_u64);
    HWY_EXPORT(simd_scan_greater_than_f32);
    HWY_EXPORT(simd_scan_greater_than_f64);

    HWY_EXPORT(simd_scan_less_than_i8);
    HWY_EXPORT(simd_scan_less_than_i16);
    HWY_EXPORT(simd_scan_less_than_i32);
    HWY_EXPORT(simd_scan_less_than_i64);
    HWY_EXPORT(simd_scan_less_than_u8);
    HWY_EXPORT(simd_scan_less_than_u16);
    HWY_EXPORT(simd_scan_less_than_u32);
    HWY_EXPORT(simd_scan_less_than_u64);
    HWY_EXPORT(simd_scan_less_than_f32);
    HWY_EXPORT(simd_scan_less_than_f64);

    HWY_EXPORT(simd_scan_between_i8);
    HWY_EXPORT(simd_scan_between_i16);
    HWY_EXPORT(simd_scan_between_i32);
    HWY_EXPORT(simd_scan_between_i64);
    HWY_EXPORT(simd_scan_between_u8);
    HWY_EXPORT(simd_scan_between_u16);
    HWY_EXPORT(simd_scan_between_u32);
    HWY_EXPORT(simd_scan_between_u64);
    HWY_EXPORT(simd_scan_between_f32);
    HWY_EXPORT(simd_scan_between_f64);

#define VERTEX_DISPATCH_SIMD_BY_TYPE(PREFIX)                \
    switch (type)                                            \
    {                                                        \
        case ValueType::Int8:                               \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_i8), true};  \
        case ValueType::Int16:                              \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_i16), true}; \
        case ValueType::Int32:                              \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_i32), true}; \
        case ValueType::Int64:                              \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_i64), true}; \
        case ValueType::UInt8:                              \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_u8), true};  \
        case ValueType::UInt16:                             \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_u16), true}; \
        case ValueType::UInt32:                             \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_u32), true}; \
        case ValueType::UInt64:                             \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_u64), true}; \
        case ValueType::Float:                              \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_f32), true}; \
        case ValueType::Double:                             \
            return {HWY_DYNAMIC_DISPATCH(PREFIX##_f64), true}; \
        default:                                             \
            return {};                                       \
    }

    SimdScanCapability resolve_simd_scanner(const ValueType type, const NumericScanMode mode)
    {
        switch (mode)
        {
            case NumericScanMode::Exact:
                VERTEX_DISPATCH_SIMD_BY_TYPE(simd_scan_exact);
            case NumericScanMode::GreaterThan:
                VERTEX_DISPATCH_SIMD_BY_TYPE(simd_scan_greater_than);
            case NumericScanMode::LessThan:
                VERTEX_DISPATCH_SIMD_BY_TYPE(simd_scan_less_than);
            case NumericScanMode::Between:
                VERTEX_DISPATCH_SIMD_BY_TYPE(simd_scan_between);
            default:
                return {};
        }
    }
#undef VERTEX_DISPATCH_SIMD_BY_TYPE
} // namespace Vertex::Scanner::Simd
#endif
