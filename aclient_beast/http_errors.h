#pragma once
#include <string>

namespace http_errors
{
    enum http_error_codes {
        success = 0,
        // 不能使用默认的零。错误码体系中，零表示 no error
        invalid_response = 1
    };

    class http_errors_category : public std::error_category
    {
    public:
        const char* name() const noexcept override
        {
            return "http_errors";
        }
        std::string message(int e) const override
        {
            switch (e)
            {
            case http_errors::invalid_response:
                return "Server response cannot be parsed.";
                break;
            default:
                return "Unknown error.";
                break;
            }
        }
    };

    const std::error_category& get_http_errors_category()
    {
        static http_errors_category cat;
        return cat;
    }

    std::error_code make_error_code(http_error_codes e)
    {
        return std::error_code(e, get_http_errors_category());
    }
}

namespace std
{
    template<>
    struct is_error_code_enum<http_errors::http_error_codes> : public true_type
    {

    };
}