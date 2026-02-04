#include <exception>
#include <string>

class CtSqlException : public std::exception
{
private:
    std::string message_;
    int error_code_;

public:
    CtSqlException(std::string msg, int code)
    : message_(std::move(msg))
    , error_code_(code)
    {
    }

    const char* what() const noexcept override
    {
        return message_.c_str();
    }

    int code() const
    {
        return error_code_;
    }
};
