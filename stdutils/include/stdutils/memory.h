// Copyright (c) 2024 Pierre DEJOUE
// This code is distributed under the terms of the MIT License
#pragma once

#include <stdutils/macros.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace stdutils {

/**
 * Versions of memcpy with sanity checks
 *
 * - Check for null pointers
 * - Check if dest == src, in which case do nothing and return true
 * - Check that nb_bytes <= max_dest_sz * sizeof(T)
 * - In case of error, copy nothing and return false
 */
template <typename T>
bool memcpy(T* dest, const void* src);                                                   // Copy exactly sizeof(T) bytes

template <typename T>
bool memcpy(T* dest, std::size_t max_dest_sz, const void* src, std::size_t nb_bytes);    // Copy at most max_dest_sz * sizeof(T) bytes


/**
 * Versions of memset with sanity checks
 */
template <typename T>
bool memset(T* dest, int ch);                                                            // Set at exactly sizeof(T) bytes

template <typename T>
bool memset(T* dest, std::size_t max_dest_sz, int ch, std::size_t nb_bytes);             // Set at most max_dest_sz * sizeof(T) bytes


/**
 * Versions of memmove with sanity checks
 *
 * - Check for null pointers
 * - Check if dest == src, in which case do nothing and return true
 * - Check that nb_bytes <= max_dest_sz * sizeof(T)
 * - In case of error, move nothing and return false
 */
template <typename T>
bool memmove(T* dest, const void* src);                                                   // Move exactly sizeof(T) bytes

template <typename T>
bool memmove(T* dest, std::size_t max_dest_sz, const void* src, std::size_t nb_bytes);    // Move at most max_dest_sz * sizeof(T) bytes


/**
 * Scoped pointer to a local
 *
 * The pointer is reset (to its initial value) when this object leaves scope.
 * Useful when a pointer to a scoped variable is used.
 */
template <typename T>
class ScopedPtrToLocal {
public:
    ScopedPtrToLocal(T** local_ptr_ptr, T& local_var)
        : m_ptr_ptr{local_ptr_ptr}
        , m_ptr_restore{nullptr}
    {
        assert(m_ptr_ptr);
        if (m_ptr_ptr) {
            m_ptr_restore = *m_ptr_ptr;
            *m_ptr_ptr = &local_var;
        }
    }

    ~ScopedPtrToLocal()
    {
        // Restore the original pointer
        if (m_ptr_ptr) { *m_ptr_ptr = m_ptr_restore; }
    }

    // Not copyable, not moveable
    ScopedPtrToLocal(const ScopedPtrToLocal&) = delete;
    ScopedPtrToLocal& operator=(const ScopedPtrToLocal&) = delete;
    ScopedPtrToLocal(ScopedPtrToLocal&&) noexcept = delete;
    ScopedPtrToLocal& operator=(ScopedPtrToLocal&&) noexcept = delete;

private:
    T** m_ptr_ptr;
    T*  m_ptr_restore;
};


//
//
// Implementation
//
//


template <typename T>
bool memcpy(T* dest, const void* src)
{
    if (dest == nullptr || src == nullptr)
    {
        return false;
    }
    if (static_cast<void*>(dest) == src)
    {
        return true;
    }
    IGNORE_RETURN std::memcpy(static_cast<void*>(dest), src, sizeof(T));
    return true;
}

template <typename T>
bool memcpy(T* dest, std::size_t max_dest_sz, const void* src, std::size_t nb_bytes)
{
    if (dest == nullptr || src == nullptr)
    {
        return false;
    }
    if (nb_bytes > max_dest_sz * sizeof(T))
    {
        return false;
    }
    if (static_cast<void*>(dest) == src)
    {
        return true;
    }
    IGNORE_RETURN std::memcpy(static_cast<void*>(dest), src, nb_bytes);
    return true;
}

template <typename T>
bool memset(T* dest, int ch)
{
    if (dest == nullptr)
    {
        return false;
    }
    IGNORE_RETURN std::memset(static_cast<void*>(dest), ch, sizeof(T));
    return true;
}

template <typename T>
bool memset(T* dest, std::size_t max_dest_sz, int ch, std::size_t nb_bytes)
{
    if (dest == nullptr)
    {
        return false;
    }
    if (nb_bytes > max_dest_sz * sizeof(T))
    {
        return false;
    }
    IGNORE_RETURN std::memset(static_cast<void*>(dest), ch, nb_bytes);
    return true;
}

template <typename T>
bool memmove(T* dest, const void* src)
{
    if (dest == nullptr || src == nullptr)
    {
        return false;
    }
    if (static_cast<void*>(dest) == src)
    {
        return true;
    }
    IGNORE_RETURN std::memmove(static_cast<void*>(dest), src, sizeof(T));
    return true;
}

template <typename T>
bool memmove(T* dest, std::size_t max_dest_sz, const void* src, std::size_t nb_bytes)
{
    if (dest == nullptr || src == nullptr)
    {
        return false;
    }
    if (nb_bytes > max_dest_sz * sizeof(T))
    {
        return false;
    }
    if (static_cast<void*>(dest) == src)
    {
        return true;
    }
    IGNORE_RETURN std::memmove(static_cast<void*>(dest), src, nb_bytes);
    return true;
}

} // namespace stdutils
