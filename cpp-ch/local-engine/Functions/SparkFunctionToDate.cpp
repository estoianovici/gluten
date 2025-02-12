/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <Common/LocalDate.h>
#include <Common/DateLUT.h>
#include <Common/DateLUTImpl.h>
#include <DataTypes/DataTypeDate32.h>
#include <Functions/FunctionsConversion.h>
#include <Functions/FunctionFactory.h>
#include <IO/ReadBufferFromMemory.h>
#include <IO/ReadHelpers.h>

namespace DB
{
namespace ErrorCodes
{
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
    extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
}
}

namespace local_engine
{
class SparkFunctionConvertToDate : public DB::FunctionToDate32OrNull
{
public:
    static constexpr auto name = "spark_to_date";
    static DB::FunctionPtr create(DB::ContextPtr) { return std::make_shared<SparkFunctionConvertToDate>(); }
    SparkFunctionConvertToDate() = default;
    ~SparkFunctionConvertToDate() override = default;
    DB::String getName() const override { return name; }

    bool checkDateFormat(DB::ReadBuffer & buf) const
    {
        auto checkNumbericASCII = [&](DB::ReadBuffer & rb, size_t start, size_t length) -> bool
        {
            for (size_t i = start; i < start + length; ++i)
            {
                if (!isNumericASCII(*(rb.position() + i)))
                    return false;
            }
            return true;
        };
        auto checkDelimiter = [&](DB::ReadBuffer & rb, size_t pos) -> bool
        {
            if (*(rb.position() + pos) != '-')
                return false;
            else
                return true;
        };
        if (!checkNumbericASCII(buf, 0, 4) 
            || !checkDelimiter(buf, 4) 
            || !checkNumbericASCII(buf, 5, 2) 
            || !checkDelimiter(buf, 7) 
            || !checkNumbericASCII(buf, 8, 2))
            return false;
        else
        {
            int month = (*(buf.position() + 5) - '0') * 10 + (*(buf.position() + 6) - '0');
            if (month <= 0 || month > 12)
                return false;
            int day = (*(buf.position() + 8) - '0') * 10 + (*(buf.position() + 9) - '0');
            if (day <= 0 || day > 31)
                return false;
            else if (day == 31 && (month == 2 || month == 4 || month == 6 || month == 9 || month == 11))
                return false;
            else if (day == 30 && month == 2)
                return false;
            else
            {
                int year = (*(buf.position() + 0) - '0') * 1000 + 
                    (*(buf.position() + 1) - '0') * 100 + 
                    (*(buf.position() + 2) - '0') * 10 + 
                    (*(buf.position() + 3) - '0');
                if (day == 29 && month == 2 && year % 4 != 0)
                    return false;
                else
                    return true;
            }
        }
    }

    DB::ColumnPtr executeImpl(const DB::ColumnsWithTypeAndName & arguments, const DB::DataTypePtr & result_type, size_t) const override
    {
        if (arguments.size() != 1)
            throw DB::Exception(DB::ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH, "Function {}'s arguments number must be 1.", name);
        
        const DB::ColumnWithTypeAndName arg1 = arguments[0];
        const auto * src_col = checkAndGetColumn<DB::ColumnString>(arg1.column.get());
        size_t size = src_col->size();

        if (!result_type->isNullable())
            throw DB::Exception(DB::ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Function {}'s return type must be nullable", name);
        
        if (!isDate32(removeNullable(result_type)))
            throw DB::Exception(DB::ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Function {}'s return type must be data32.", name);
        
        using ColVecTo = DB::DataTypeDate32::ColumnType;
        typename ColVecTo::MutablePtr result_column = ColVecTo::create(size);
        typename ColVecTo::Container & result_container = result_column->getData();
        DB::ColumnUInt8::MutablePtr null_map = DB::ColumnUInt8::create(size);
        typename DB::ColumnUInt8::Container & null_container = null_map->getData();
        const DateLUTImpl * utc_time_zone = &DateLUT::instance("UTC");

        for (size_t i = 0; i < size; ++i)
        {
            auto str = src_col->getDataAt(i);
            if (str.size < 10)
            {
                null_container[i] = true;
                result_container[i] = 0;
                continue;
            }
            else
            {
                DB::ReadBufferFromMemory buf(str.data, str.size);
                while(!buf.eof() && *buf.position() == ' ')
                {
                    buf.position() ++;
                }
                if(buf.buffer().end() - buf.position() < 10)
                {
                    null_container[i] = true;
                    result_container[i] = 0;
                    continue;
                }
                if (!checkDateFormat(buf))
                {
                    null_container[i] = true;
                    result_container[i] = 0;
                }
                else
                {
                    bool parsed = tryParseImpl<DB::DataTypeDate32>(result_container[i], buf, utc_time_zone, false);
                    null_container[i] = !parsed;
                }
            }
        }
        return DB::ColumnNullable::create(std::move(result_column), std::move(null_map));
    }
};

REGISTER_FUNCTION(SparkToDate)
{
    factory.registerFunction<SparkFunctionConvertToDate>();
}

}
