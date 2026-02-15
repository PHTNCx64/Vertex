//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/registry.hh>

#include <algorithm>
#include <ranges>

namespace Vertex::Runtime
{
    StatusCode Registry::register_architecture(const ArchitectureInfo& archInfo)
    {
        std::scoped_lock lock(m_mutex);

        ArchInfo info;
        info.endianness = archInfo.endianness;
        info.preferredSyntax = archInfo.preferredSyntax;
        info.addressWidth = archInfo.addressWidth;
        info.maxHardwareBreakpoints = archInfo.maxHardwareBreakpoints;
        info.stackGrowsDown = archInfo.stackGrowsDown;
        info.architectureName = archInfo.architectureName;

        m_archInfo = info;
        return STATUS_OK;
    }

    std::optional<ArchInfo> Registry::get_architecture() const
    {
        std::scoped_lock lock(m_mutex);
        return m_archInfo;
    }

    StatusCode Registry::register_category(const RegisterCategoryDef& category)
    {
        std::scoped_lock lock(m_mutex);

        RegisterCategoryInfo info;
        info.categoryId = category.categoryId;
        info.displayName = category.displayName;
        info.displayOrder = category.displayOrder;
        info.collapsedByDefault = category.collapsedByDefault != 0;

        m_categories[info.categoryId] = info;
        return STATUS_OK;
    }

    StatusCode Registry::unregister_category(const std::string_view categoryId)
    {
        std::scoped_lock lock(m_mutex);

        const std::string categoryIdStr{categoryId};
        const auto it = m_categories.find(categoryIdStr);
        if (it == m_categories.end())
        {
            return STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        for (auto regIt = m_registers.begin(); regIt != m_registers.end();)
        {
            if (regIt->second.categoryId == categoryIdStr)
            {
                regIt = m_registers.erase(regIt);
            }
            else
            {
                ++regIt;
            }
        }

        m_categories.erase(it);
        m_specialRegistersCached = false;
        return STATUS_OK;
    }

    std::vector<RegisterCategoryInfo> Registry::get_categories() const
    {
        std::scoped_lock lock(m_mutex);

        std::vector<RegisterCategoryInfo> result;
        result.reserve(m_categories.size());

        for (const auto& info : m_categories | std::views::values)
        {
            result.push_back(info);
        }

        std::ranges::sort(result,
            [](const RegisterCategoryInfo& a, const RegisterCategoryInfo& b)
            {
                return a.displayOrder < b.displayOrder;
            });

        return result;
    }

    std::optional<RegisterCategoryInfo> Registry::get_category(const std::string_view categoryId) const
    {
        std::scoped_lock lock(m_mutex);

        const auto it = m_categories.find(std::string{categoryId});
        if (it != m_categories.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    StatusCode Registry::register_register(const RegisterDef& reg)
    {
        std::scoped_lock lock(m_mutex);

        RegisterInfo info;
        info.categoryId = reg.categoryId;
        info.name = reg.name;
        info.parentName = reg.parentName;
        info.bitWidth = reg.bitWidth;
        info.bitOffset = reg.bitOffset;
        info.flags = reg.flags;
        info.displayOrder = reg.displayOrder;
        info.registerId = reg.registerId;
        info.writeFunc = reg.write_func;
        info.readFunc = reg.read_func;

        m_registers[info.name] = info;
        m_specialRegistersCached = false;
        return STATUS_OK;
    }

    StatusCode Registry::unregister_register(const std::string_view registerName)
    {
        std::scoped_lock lock(m_mutex);

        const auto it = m_registers.find(std::string{registerName});
        if (it == m_registers.end())
        {
            return STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        m_registers.erase(it);
        m_specialRegistersCached = false;
        return STATUS_OK;
    }

    std::vector<RegisterInfo> Registry::get_registers() const
    {
        std::scoped_lock lock(m_mutex);

        std::vector<RegisterInfo> result;
        result.reserve(m_registers.size());

        for (const auto& info : m_registers | std::views::values)
        {
            result.push_back(info);
        }

        std::ranges::sort(result,
            [](const RegisterInfo& a, const RegisterInfo& b)
            {
                if (a.categoryId != b.categoryId)
                {
                    return a.categoryId < b.categoryId;
                }
                return a.displayOrder < b.displayOrder;
            });

        return result;
    }

    std::vector<RegisterInfo> Registry::get_registers_by_category(const std::string_view categoryId) const
    {
        std::scoped_lock lock(m_mutex);

        std::vector<RegisterInfo> result;

        for (const auto& info : m_registers | std::views::values)
        {
            if (info.categoryId == categoryId)
            {
                result.push_back(info);
            }
        }

        std::ranges::sort(result,
            [](const RegisterInfo& a, const RegisterInfo& b)
            {
                return a.displayOrder < b.displayOrder;
            });

        return result;
    }

    std::optional<RegisterInfo> Registry::get_register(const std::string_view registerName) const
    {
        std::scoped_lock lock(m_mutex);

        const auto it = m_registers.find(std::string{registerName});
        if (it != m_registers.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    StatusCode Registry::register_flag_bit(const FlagBitDef& flagBit)
    {
        std::scoped_lock lock(m_mutex);

        FlagBitInfo info;
        info.flagsRegisterName = flagBit.flagsRegisterName;
        info.bitName = flagBit.bitName;
        info.description = flagBit.description;
        info.bitPosition = flagBit.bitPosition;

        m_flagBits[info.flagsRegisterName].push_back(info);

        auto& bits = m_flagBits[info.flagsRegisterName];
        std::ranges::sort(bits,
            [](const FlagBitInfo& a, const FlagBitInfo& b)
            {
                return a.bitPosition < b.bitPosition;
            });

        return STATUS_OK;
    }

    std::vector<FlagBitInfo> Registry::get_flag_bits(const std::string_view flagsRegisterName) const
    {
        std::scoped_lock lock(m_mutex);

        const auto it = m_flagBits.find(std::string{flagsRegisterName});
        if (it != m_flagBits.end())
        {
            return it->second;
        }
        return {};
    }

    StatusCode Registry::register_exception_type(const ExceptionTypeDef& exceptionType)
    {
        std::scoped_lock lock(m_mutex);

        ExceptionTypeInfo info;
        info.exceptionCode = exceptionType.exceptionCode;
        info.name = exceptionType.name;
        info.description = exceptionType.description;
        info.isFatal = exceptionType.isFatal != 0;

        m_exceptionTypes[info.exceptionCode] = info;
        return STATUS_OK;
    }

    std::vector<ExceptionTypeInfo> Registry::get_exception_types() const
    {
        std::scoped_lock lock(m_mutex);

        std::vector<ExceptionTypeInfo> result;
        result.reserve(m_exceptionTypes.size());

        for (const auto& info : m_exceptionTypes | std::views::values)
        {
            result.push_back(info);
        }

        return result;
    }

    std::optional<ExceptionTypeInfo> Registry::get_exception_type(const std::uint32_t code) const
    {
        std::scoped_lock lock(m_mutex);

        const auto it = m_exceptionTypes.find(code);
        if (it != m_exceptionTypes.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    StatusCode Registry::register_calling_convention(const CallingConventionDef& callingConv)
    {
        std::scoped_lock lock(m_mutex);

        CallingConventionInfo info;
        info.name = callingConv.name;
        info.returnRegister = callingConv.returnRegister;
        info.stackCleanupByCallee = callingConv.stackCleanup != 0;

        for (std::uint8_t i = 0; i < callingConv.parameterRegisterCount && i < 8; ++i)
        {
            if (callingConv.parameterRegisters[i][0] != '\0')
            {
                info.parameterRegisters.emplace_back(callingConv.parameterRegisters[i]);
            }
        }

        m_callingConventions.push_back(info);
        return STATUS_OK;
    }

    std::vector<CallingConventionInfo> Registry::get_calling_conventions() const
    {
        std::scoped_lock lock(m_mutex);
        return m_callingConventions;
    }

    StatusCode Registry::register_snapshot(const RegistrySnapshot& snapshot)
    {
        register_architecture(snapshot.archInfo);

        for (std::uint32_t i = 0; i < snapshot.categoryCount; ++i)
        {
            register_category(snapshot.categories[i]);
        }

        for (std::uint32_t i = 0; i < snapshot.registerCount; ++i)
        {
            register_register(snapshot.registers[i]);
        }

        for (std::uint32_t i = 0; i < snapshot.flagBitCount; ++i)
        {
            register_flag_bit(snapshot.flagBits[i]);
        }

        for (std::uint32_t i = 0; i < snapshot.exceptionTypeCount; ++i)
        {
            register_exception_type(snapshot.exceptionTypes[i]);
        }

        for (std::uint32_t i = 0; i < snapshot.callingConventionCount; ++i)
        {
            register_calling_convention(snapshot.callingConventions[i]);
        }

        return STATUS_OK;
    }

    void Registry::clear()
    {
        std::scoped_lock lock(m_mutex);

        m_archInfo.reset();
        m_categories.clear();
        m_registers.clear();
        m_flagBits.clear();
        m_exceptionTypes.clear();
        m_callingConventions.clear();

        m_programCounterName.reset();
        m_stackPointerName.reset();
        m_framePointerName.reset();
        m_flagsRegisterName.reset();
        m_specialRegistersCached = false;
    }

    bool Registry::has_register(const std::string_view registerName) const
    {
        std::scoped_lock lock(m_mutex);
        return m_registers.contains(std::string{registerName});
    }

    void Registry::cache_special_registers() const
    {
        if (m_specialRegistersCached)
        {
            return;
        }

        m_programCounterName.reset();
        m_stackPointerName.reset();
        m_framePointerName.reset();
        m_flagsRegisterName.reset();

        for (const auto& [name, info] : m_registers)
        {
            if (info.flags & VERTEX_REG_FLAG_PROGRAM_COUNTER)
            {
                m_programCounterName = name;
            }
            if (info.flags & VERTEX_REG_FLAG_STACK_POINTER)
            {
                m_stackPointerName = name;
            }
            if (info.flags & VERTEX_REG_FLAG_FRAME_POINTER)
            {
                m_framePointerName = name;
            }
            if (info.flags & VERTEX_REG_FLAG_FLAGS_REGISTER)
            {
                m_flagsRegisterName = name;
            }
        }

        m_specialRegistersCached = true;
    }

    std::optional<RegisterInfo> Registry::get_program_counter() const
    {
        std::scoped_lock lock(m_mutex);
        cache_special_registers();

        if (m_programCounterName)
        {
            const auto it = m_registers.find(*m_programCounterName);
            if (it != m_registers.end())
            {
                return it->second;
            }
        }
        return std::nullopt;
    }

    std::optional<RegisterInfo> Registry::get_stack_pointer() const
    {
        std::scoped_lock lock(m_mutex);
        cache_special_registers();

        if (m_stackPointerName)
        {
            const auto it = m_registers.find(*m_stackPointerName);
            if (it != m_registers.end())
            {
                return it->second;
            }
        }
        return std::nullopt;
    }

    std::optional<RegisterInfo> Registry::get_frame_pointer() const
    {
        std::scoped_lock lock(m_mutex);
        cache_special_registers();

        if (m_framePointerName)
        {
            const auto it = m_registers.find(*m_framePointerName);
            if (it != m_registers.end())
            {
                return it->second;
            }
        }
        return std::nullopt;
    }

    std::optional<RegisterInfo> Registry::get_flags_register() const
    {
        std::scoped_lock lock(m_mutex);
        cache_special_registers();

        if (m_flagsRegisterName)
        {
            const auto it = m_registers.find(*m_flagsRegisterName);
            if (it != m_registers.end())
            {
                return it->second;
            }
        }
        return std::nullopt;
    }

}
