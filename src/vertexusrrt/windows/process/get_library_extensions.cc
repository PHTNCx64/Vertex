//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <sdk/api.h>

#include <algorithm>
#include <array>

extern "C"
{
    // TODO: Unify implementation with the executable version of the extensions function, helper function needs to be added.

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_library_extensions(char** extensions, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        static constexpr std::array kLibraryExtensions = {
            ".dll",
          };

        constexpr auto kActualCount = static_cast<uint32_t>(std::size(kLibraryExtensions));

        if (!extensions)
        {
            *count = kActualCount;
            return StatusCode::STATUS_OK;
        }

        if (*count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint32_t bufferSize = *count;
        const uint32_t copyCount = std::min(bufferSize, kActualCount);

        for (uint32_t i = 0; i < copyCount; ++i)
        {
            extensions[i] = const_cast<char*>(kLibraryExtensions[i]);
        }

        *count = copyCount;

        if (kActualCount > bufferSize)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }
}