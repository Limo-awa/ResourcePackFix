#pragma once

// Binary-compatible RemoteCall types for ResourcePackFix
// Follows the same pattern as AsyncDB's SimpleRemoteCall.h
// Must match the exact layout of LegacyRemoteCall's ValueType

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <cstdint>
#include "ll/api/utils/SystemUtils.h"

namespace RemoteCall {

// NumberType - must match exactly
struct NumberType {
    __int64 i = 0;
    double  f = 0;
    NumberType() = default;
    NumberType(__int64 i, double f) : i(i), f(f) {}
    NumberType(double v) : i(static_cast<__int64>(v)), f(v) {}
    NumberType(__int64 v) : i(v), f(static_cast<double>(v)) {}
    NumberType(int v) : i(v), f(static_cast<double>(v)) {}
    
    template <typename T>
    T get() const {
        if constexpr (std::is_integral_v<T>) return static_cast<T>(i);
        else return static_cast<T>(f);
    }
};

// Placeholder types for MC objects we don't actually use
struct WorldPosType {
    float pos[3] = {0, 0, 0};
    int dimId = 0;
};

struct BlockPosType {
    int pos[3] = {0, 0, 0};
    int dimId = 0;
};

struct ItemType {
    void* ptr = nullptr;
    bool own = false;
};

struct BlockType {
    void* block = nullptr;
    int blockPos[3] = {0, 0, 0};
    int dimension = 0;
};

struct NbtType {
    void* ptr = nullptr;
    bool own = false;
};

// Placeholder pointers for MC types
using PlayerPtr = void*;
using ActorPtr = void*;
using BlockActorPtr = void*;
using ContainerPtr = void*;

// Value type - MUST match the variant order in official RemoteCallAPI.h
using Value = std::variant<
    bool,           // 0
    std::string,    // 1
    std::nullptr_t, // 2
    NumberType,     // 3
    PlayerPtr,      // 4 (Player*)
    ActorPtr,       // 5 (Actor*)
    BlockActorPtr,  // 6 (BlockActor*)
    ContainerPtr,   // 7 (Container*)
    WorldPosType,   // 8
    BlockPosType,   // 9
    ItemType,       // 10
    BlockType,      // 11
    NbtType         // 12
>;

struct ValueType;
using ArrayType = std::vector<ValueType>;
using ObjectType = std::unordered_map<std::string, ValueType>;

struct ValueType {
    using Type = std::variant<Value, ArrayType, ObjectType>;
    Type value;
    
    ValueType() : value(Value{}) {}
    ValueType(Value v) : value(std::move(v)) {}
    ValueType(ArrayType v) : value(std::move(v)) {}
    ValueType(ObjectType v) : value(std::move(v)) {}
    
    // Basic type constructors
    ValueType(bool v) : value(Value(v)) {}
    ValueType(int v) : value(Value(NumberType(v))) {}
    ValueType(__int64 v) : value(Value(NumberType(v))) {}
    ValueType(double v) : value(Value(NumberType(v))) {}
    ValueType(std::string const& v) : value(Value(v)) {}
    ValueType(const char* v) : value(Value(std::string(v))) {}
    ValueType(std::nullptr_t) : value(Value(nullptr)) {}
};

using CallbackFn = std::function<ValueType(std::vector<ValueType>)>;

// Import from legacyremotecall library
__declspec(dllimport) extern CallbackFn const EMPTY_FUNC;
__declspec(dllimport) bool exportFunc(
    std::string const& nameSpace,
    std::string const& funcName,
    CallbackFn&&       callback,
    void*              handle = ll::sys_utils::getCurrentModuleHandle()
);
__declspec(dllimport) CallbackFn const& importFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllimport) bool hasFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllimport) bool removeFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllimport) int removeNameSpace(std::string const& nameSpace);

// Safe extraction for basic types
template <typename T>
T extract(ValueType&& val) {
    if (!std::holds_alternative<Value>(val.value)) return T{};
    
    auto& v = std::get<Value>(val.value);
    
    if constexpr (std::is_same_v<T, std::string>) {
        if (v.index() == 1) return std::get<1>(v);
        if (v.index() == 2) return "";
        if (v.index() == 3) return std::to_string(std::get<3>(v).get<__int64>());
        if (v.index() == 0) return std::get<0>(v) ? "true" : "false";
        return "";
    } else if constexpr (std::is_same_v<T, bool>) {
        if (v.index() == 0) return std::get<0>(v);
        if (v.index() == 3) return std::get<3>(v).get<int>() != 0;
        return false;
    } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
        if (v.index() == 3) return std::get<3>(v).get<T>();
        if (v.index() == 0) return static_cast<T>(std::get<0>(v));
        return T{};
    }
    return T{};
}

// Pack basic types
template <typename T>
ValueType pack(T val) {
    return ValueType(val);
}

// Export helper for callbacks
template <typename RTN, typename... Args>
inline bool _exportAs(std::string const& nameSpace, std::string const& funcName, std::function<RTN(Args...)>&& callback) {
    CallbackFn cb = [callback = std::move(callback)](std::vector<ValueType> args) -> ValueType {
        if (args.size() < sizeof...(Args)) {
            args.resize(sizeof...(Args));
        }
        
        if constexpr (sizeof...(Args) == 0) {
            if constexpr (std::is_void_v<RTN>) {
                callback();
                return ValueType();
            } else {
                return pack(callback());
            }
        } 
        else if constexpr (sizeof...(Args) == 1) {
            using Arg0 = std::tuple_element_t<0, std::tuple<Args...>>;
            if constexpr (std::is_void_v<RTN>) {
                callback(extract<std::remove_cv_t<std::remove_reference_t<Arg0>>>(std::move(args[0])));
                return ValueType();
            } else {
                return pack(callback(extract<std::remove_cv_t<std::remove_reference_t<Arg0>>>(std::move(args[0]))));
            }
        }
        else if constexpr (sizeof...(Args) == 2) {
            using Arg0 = std::tuple_element_t<0, std::tuple<Args...>>;
            using Arg1 = std::tuple_element_t<1, std::tuple<Args...>>;
            if constexpr (std::is_void_v<RTN>) {
                callback(
                    extract<std::remove_cv_t<std::remove_reference_t<Arg0>>>(std::move(args[0])),
                    extract<std::remove_cv_t<std::remove_reference_t<Arg1>>>(std::move(args[1]))
                );
                return ValueType();
            } else {
                return pack(callback(
                    extract<std::remove_cv_t<std::remove_reference_t<Arg0>>>(std::move(args[0])),
                    extract<std::remove_cv_t<std::remove_reference_t<Arg1>>>(std::move(args[1]))
                ));
            }
        }
        return ValueType();
    };
    return exportFunc(nameSpace, funcName, std::move(cb), ll::sys_utils::getCurrentModuleHandle());
}

template <typename CB>
inline bool exportAs(std::string const& nameSpace, std::string const& funcName, CB&& callback) {
    return _exportAs(nameSpace, funcName, std::function(std::forward<CB>(callback)));
}

} // namespace RemoteCall
