//  File:   file.cpp
//  Author: Ritchie Brannan
//  Date:   1 Mar 26
//
//  Basic load and save utility functions (file <=> memory blob)
//
//  Multi-threaded usage assumes that multiple threads will not be saving
//  files with the same name.

#include "io/file/file.hpp"
#include "io/file/internal/file_utils.hpp"

namespace io::file
{

CByteBuffer loadFile(const char* const utf8_path, const std::size_t pad) noexcept
{
    CByteBuffer buffer;
    void* data = nullptr;
    NativePath std_path = stdPath(utf8_path);
    if (!std_path.is_empty())
    {
        std::FILE* handle = openFile(std_path, OpenMode::BinaryRead);
        if (handle != nullptr)
        {
            bool success = false;
            std::size_t size = getFileSize(handle, pad);
            if (size != 0u)
            {
                if (buffer.allocate(size))
                {
                    uint8_t* data = buffer.data();
                    std::size_t file_size = size - pad;
                    if (std::fread(data, 1, file_size, handle) == file_size)
                    {
                        if (pad != 0u)
                        {
                            std::memset((data + file_size), 0, pad);
                        }
                        success = true;
                    }
                }
            }
            if (std::fclose(handle) != 0)
            {   //  close failed
                success = false;
            }
            if (!success)
            {
                buffer.deallocate();
            }
        }
    }
    return buffer;
}

bool saveFile(const char* const utf8_path, const CByteView& view) noexcept
{
    bool success = false;
    if (!view.is_empty())
    {
        NativePath std_path = stdPath(utf8_path);
        if (!std_path.is_empty())
        {
            NativePath tmp_path = tmpPath(std_path);
            if (!tmp_path.is_empty())
            {
                std::FILE* handle = openFile(tmp_path, OpenMode::BinaryWrite);
                if (handle != nullptr)
                {
                    const std::size_t size = view.size();
                    success = std::fwrite(view.data(), 1, size, handle) == size;
                    if (std::fflush(handle) != 0)
                    {   //  flush failed
                        success = false;
                    }
                    if (std::fclose(handle) != 0)
                    {   //  close failed
                        success = false;
                    }
                    if (!success || !renameFile(tmp_path, std_path))
                    {   //  rename failed or prior failure
                        success = false;
                        removeFile(tmp_path);
                    }
                }
            }
        }
    }
    return success;
}

}	//	namespace io::file
