
#include <gmock/gmock.h>
#include <string_view>

using namespace testing;

MATCHER_P(IsStrSame, expected_value, std::format("is equal to {}", expected_value))
{
    return Matches(StrEq(expected_value))(static_cast<std::string_view>(arg));
}

MATCHER_P(IsFloatSame, expected_value, std::format("is approximately equal to {}", expected_value))
{
    return Matches(FloatEq(expected_value))(static_cast<float>(arg));
}

MATCHER_P(IsDoubleSame, expected_value, std::format("is approximately equal to {}", expected_value))
{
    return Matches(DoubleEq(expected_value))(static_cast<double>(arg));
}
