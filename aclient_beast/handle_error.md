
`std::system_error` is the type of the exception thrown by various library functions when the exception has an associated `std::error_code`, which may be reported.

`std::error_code` is a platform-dependent 平台相关的 error code. Each `std::error_code` object holds an error code originating from 起源于 the operating system or some low-level interface and a pointer to an object of type `std::error_category`, which corresponds to the said interface. The error code values may be not unique across different error categories.

当我们使用 an object of `std::error_code` 与 a value of enum 比较时，其实依赖于 `std::error_code` 重载的模板构造函数：先用后者隐式构造 `std::error_code` 的对象，在于前者比较。 

所以我正在使用的 http_errors.h 文件中定义 `enum http_error_codes` ~~是不是错误的？它的实现并没有采用类型安全的 `enum class`~~ 没有问题，但还是建议改用 `enum class`。弱类型也是类型，两个普通的枚举类型：也是能够重载，用作模板的类型参数也是能区分的。

```cpp
enum Color {red, yellow};
enum Month {Jan, Feb};
```

`std::error_condition` is a platform-independent 与平台无关的 error code. Like `std::error_code`, it is uniquely identified by an integer value and a `std::error_category`, but unlike `std::error_code`, the value is not platform-dependent.

`std::error_condtion` 也包含 `std::error_category`，那么它和 `std::error_code` 似乎可以共用完全相同的实现，价值在哪里？用来统一不同平台的相同错误吗？

`std::error_category` serves as the base class for specific error category types, such as `std::system_category`, `std::iostream_category`, etc. Each specific category class defines the `error_code` - `error_condition` mapping and holds the explanatory strings for all `error_condition`s. The objects of error category classes are treated as singletons, passed by reference.

