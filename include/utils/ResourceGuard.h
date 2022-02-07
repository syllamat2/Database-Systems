#ifndef UTILS_RESOURCEGUARD_H
#define UTILS_RESOURCEGUARD_H

#include "tdb.h"

namespace taco {

/*!
 * ResourceGuard is used for automatically releases some resource when it goes
 * out of scope. It is similar to std::unique_ptr for dynamic allocated memory,
 * but it allows one to enable RAII idiom on other resources such as buffer
 * frames.
 *
 * The resource is denoted as a \p T typed value, (usually an integral or a
 * pointer), and it may be released by calling the \p ReleaseFunc::operator(). Some
 * constraints apply: 1) \p T may not be a boolean value; 2) \p ReleaseFunc must
 * be trivially constrictible. The release functor is not called if the stored
 * value is an \p invalid value (see below).
 *
 * The third type parameter \p FlagType and the fourth value parameter \p
 * InvalidVal are used for denoting an \p invalid value. \p FlagType must be one of
 * the following:
 *
 *  - \p FlagType: there is a special sentinel value that serves as the
 *    \p invalid value
 *
 *  - \p bool with InvalidVal == false: we store an additional boolean flag
 *  (default for non-lvalue-refence \p T)
 *
 *  - \p bool with InvalidVal == true: the value is always (default for
 *  lvalue-reference \p T). (It is a bit confusing to reuse FlagType == bool
 *  for this specialization, but we can't set it to void as we need to use a
 *  conditional type as the type of InvalidVal which is N/A until C++20).
 *
 * There is an implicit conversion from \t ResourceGuard<T, ...> to \t T so
 * that it may be used in place of \t T in many places. Caution: be careful
 * with double release if you directly call the release function instead of \p
 * ResourceGuard::reset() before it goes out of scope, unless the release
 * function specifically consider for that (e.g., \p
 * BufferManager::UnpinPage()).
 *
 * Note: always prefer existing C++ standard library guard or type alias in
 * specific components. E.g., use std::unique_ptr for memory resource; use \p
 * taco::MutexGuard for std::mutex; use taco::ScopedBufferId in
 * stoarge/BufferManager.h.
 *
 * The main template implements the class when \p FlagType is \p T.
 */
template<class T,
         class ReleaseFunc,
         class FlagType = bool,
         FlagType InvalidVal = std::is_lvalue_reference<T>::value,
         class = typename std::enable_if<
             // T may not be bool
             !std::is_same<FlagType, bool>::value &&
             // FlagType must be bool, same as T
             (std::is_same<FlagType, bool>::value ||
              std::is_same<FlagType, T>::value) &&
             // ReleaseFunc must be trivially default constructible
             std::is_trivially_default_constructible<ReleaseFunc>::value
         >::type>
class ResourceGuard {
public:
    ResourceGuard(): m_val(InvalidVal) {}

    ResourceGuard(T val): m_val(val) {}

    ~ResourceGuard() {
        if (m_val != InvalidVal)
            ReleaseFunc()(m_val);
    }

    // no copy
    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;

    // move ctor and assignment
    ResourceGuard(ResourceGuard&& other):
        m_val(other.m_val) {
        other.m_val = InvalidVal;
    }

    ResourceGuard& operator=(ResourceGuard&& other) {
        if (m_val != InvalidVal) {
            ReleaseFunc()(m_val);
        }
        m_val = other.m_val;
        other.m_val = InvalidVal;
        return *this;
    }

    operator T() const {
        return m_val;
    }

    T
    Get() const {
        return m_val;
    }

    operator bool() const {
        return m_val != InvalidVal;
    }

    bool
    IsValid() const {
        return m_val != InvalidVal;
    }

    void
    Reset() {
        if (m_val != InvalidVal) {
            ReleaseFunc()(m_val);
            m_val = InvalidVal;
        }
    }

    T
    Release() {
        T ret = m_val;
        m_val = InvalidVal;
        return ret;
    }

    constexpr bool
    operator==(T rhs) const {
        return m_val == rhs;
    }

    constexpr bool
    operator!=(T rhs) const {
        return m_val != rhs;
    }

private:
    T   m_val;
};

/*!
 * Specialization of ResourceGuard when we use an additional boolean flag to
 * denote the invalid value.
 */
template<class T, class ReleaseFunc>
class ResourceGuard<T, ReleaseFunc, bool, false, void> {
public:
    ResourceGuard(): m_isvalid(false) {}

    ResourceGuard(T val):
        m_val(val), m_isvalid(true) {}

    ~ResourceGuard() {
        if (m_isvalid)
            ReleaseFunc()(m_val);
    }

    // no copy
    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;

    // move ctor and assignment
    ResourceGuard(ResourceGuard&& other):
        m_val(other.m_val),
        m_isvalid(other.m_isvalid) {
        other.m_val = T();
        other.m_isvalid = false;
    }

    ResourceGuard& operator=(ResourceGuard&& other) {
        if (m_isvalid) {
            ReleaseFunc()(m_val);
        }
        m_val = other.m_val;
        m_isvalid = other.m_isvalid;
        other.m_val = T();
        other.m_isvalid = false;
        return *this;
    }

    operator T() const {
        return m_val;
    }

    T
    Get() const {
        return m_val;
    }

    operator bool() const {
        return m_isvalid;
    }

    bool
    IsValid() const {
        return m_isvalid;
    }

    void
    Reset() {
        if (m_isvalid) {
            ReleaseFunc()(m_val);
            m_val = T();
            m_isvalid = false;
        }
    }

    T
    Release() {
        T ret = m_val;
        m_val = T();
        m_isvalid = false;
        return ret;
    }

    constexpr bool
    operator==(T rhs) const {
        return m_val == rhs;
    }

    constexpr bool
    operator!=(T rhs) const {
        return m_val != rhs;
    }


private:
    T       m_val;
    bool    m_isvalid;
};

/*!
 * Specialization of \p ResourceGuard where we have an always-valid value
 * (e.g., lvalue-ref). This specialization may not be default-constructed,
 * copied or moved, nor can it \p Reset() or ReleaseFunc().
 */
template<class T, class ReleaseFunc>
class ResourceGuard<T, ReleaseFunc, bool, true, void> {
public:
    // no default-ctor

    ResourceGuard(T val):
        m_val(val) {}

    ~ResourceGuard() {
        ReleaseFunc()(m_val);
    }

    // no copy
    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;

    // no move
    ResourceGuard(ResourceGuard&& other) = delete;
    ResourceGuard& operator=(ResourceGuard&& other) = delete;

    operator T() const {
        return m_val;
    }

    T
    Get() const {
        return m_val;
    }

    operator bool() const {
        return true;
    }

    bool
    IsValid() const {
        return true;
    }

    constexpr bool
    operator==(T rhs) const {
        return m_val == rhs;
    }

    constexpr bool
    operator!=(T rhs) const {
        return m_val != rhs;
    }

private:
    T       m_val;
};

}   // namespace stdb

#endif      // UTILS_RESOURCEGUARD_H
