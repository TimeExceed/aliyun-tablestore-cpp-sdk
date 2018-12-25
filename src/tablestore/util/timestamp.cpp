/*
BSD 3-Clause License

Copyright (c) 2017, Alibaba Cloud
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "timestamp.hpp"
#include "tablestore/util/try.hpp"
#include "tablestore/util/prettyprint.hpp"
#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/system/error_code.hpp>
#include <deque>

using namespace std;
using namespace boost::chrono;

namespace aliyun {
namespace tablestore {
namespace util {

void sleepFor(const Duration& d)
{
    boost::this_thread::sleep_for(microseconds(d.toUsec()));
}

void sleepUntil(const MonotonicTime& target)
{
    Duration d = target - MonotonicTime::now();
    if (d > Duration::fromUsec(0)) {
        sleepFor(d);
    }
}

namespace {

void fill(string& out, int64_t val, int64_t width)
{
    deque<char> digits;
    int64_t i = 0;
    for(; val > 0; ++i, val /= 10) {
        digits.push_back('0' + (val % 10));
    }
    for(; i < width; ++i) {
        digits.push_back('0');
    }

    for(; !digits.empty(); digits.pop_back()) {
        out.push_back(digits.back());
    }
}

} // namespace

void Duration::prettyPrint(string& out) const
{
    pp::prettyPrint(out, mValue / kUsecPerHour);
    out.push_back(':');
    fill(out, (mValue % kUsecPerHour) / kUsecPerMin, 2);
    out.push_back(':');
    fill(out, (mValue % kUsecPerMin) / kUsecPerSec, 2);
    out.push_back('.');
    fill(out, mValue % kUsecPerSec, 6);
}

void MonotonicTime::prettyPrint(string& out) const
{
    pp::prettyPrint(out, mValue / kUsecPerHour);
    out.push_back(':');
    fill(out, (mValue % kUsecPerHour) / kUsecPerMin, 2);
    out.push_back(':');
    fill(out, (mValue % kUsecPerMin) / kUsecPerSec, 2);
    out.push_back('.');
    fill(out, mValue % kUsecPerSec, 6);
}

MonotonicTime MonotonicTime::now()
{
    boost::system::error_code err;
    steady_clock::time_point sc = steady_clock::now(err);
    OTS_ASSERT(!err)(err.message());
    steady_clock::duration d = sc.time_since_epoch();
    microseconds usec = duration_cast<microseconds>(d);
    return MonotonicTime(usec.count());
}


namespace {

struct TimeComponent
{
    TimeComponent()
      : mYear(0), mMonth(0), mDay(0), mHour(0),
        mMinute(0), mSec(0), mUsec(0)
    {}

    int64_t mYear;
    int64_t mMonth;
    int64_t mDay;
    int64_t mHour;
    int64_t mMinute;
    int64_t mSec;
    int64_t mUsec;

    bool isLeapYear() const
    {
        if (mYear % 4 != 0) {
            return false;
        }
        if (mYear % 400 == 0) {
            return true;
        }
        if (mYear % 100 == 0) {
            return false;
        }
        return true;
    }

    Optional<string> valid() const
    {
        if (mMonth < 1 || mMonth > 12) {
            return Optional<string>(" invalid month");
        }
        if (mDay < 1 || mDay > sDays[mMonth - 1]) {
            return Optional<string>(" invalid day");
        }
        if (mMonth == 2 && !isLeapYear()) {
            if (mDay > 28) {
                return Optional<string>(" invalid day");
            }
        }
        if (mHour < 0 || mHour >= 24) {
            return Optional<string>(" invalid hour");
        }
        if (mMinute < 0 || mMinute >= 60) {
            return Optional<string>(" invalid minute");
        }
        if (mSec < 0 || mSec >= 60) {
            return Optional<string>(" invalid second");
        }
        if (mUsec < 0) {
            return Optional<string>(" invalid subsecond");
        }
        if (mUsec >= 1000000) {
            return Optional<string>(" too precise");
        }
        return Optional<string>();
    }

    void incOneDay()
    {
        ++mDay;
        if (mMonth != 2) {
            if (mDay <= sDays[mMonth - 1]) {
                return;
            }
        } else {
            if (isLeapYear()) {
                if (mDay <= 29) {
                    return;
                }
            } else {
                if (mDay <= 28) {
                    return;
                }
            }
        }

        mDay = 1;
        ++mMonth;
        if (mMonth <= 12) {
            return;
        }

        mMonth = 1;
        ++mYear;
    }

private:
    static const int64_t sDays[12];
};

const int64_t TimeComponent::sDays[] = {
    31, // January
    29, // Febuary
    31, // March
    30, // April
    31, // May
    30, // June
    31, // July
    31, // Auguest
    30, // September
    31, // October
    30, // November
    31}; // December

int cmp(const TimeComponent& a, const TimeComponent& b)
{
    if (a.mYear < b.mYear) {
        return -1;
    } else if (a.mYear > b.mYear) {
        return 1;
    }
    if (a.mMonth < b.mMonth) {
        return -1;
    } else if (a.mMonth > b.mMonth) {
        return 1;
    }
    if (a.mDay < b.mDay) {
        return -1;
    } else if (a.mDay > b.mDay) {
        return 1;
    }
    if (a.mHour < b.mHour) {
        return -1;
    } else if (a.mHour > b.mHour) {
        return 1;
    }
    if (a.mMinute < b.mMinute) {
        return -1;
    } else if (a.mMinute > b.mMinute) {
        return 1;
    }
    if (a.mSec < b.mSec) {
        return -1;
    } else if (a.mSec > b.mSec) {
        return 1;
    }
    if (a.mUsec < b.mUsec) {
        return -1;
    } else if (a.mUsec > b.mUsec) {
        return 1;
    }
    return 0;
}

class FourCenturies
{
private:
    FourCenturies();
    
public:
    static const FourCenturies& get()
    {
        static const FourCenturies sFourCenturies;
        return sFourCenturies;
    }

    int64_t totalDays() const
    {
        return mDays.size();
    }

    int64_t period(const TimeComponent& tm) const
    {
        OTS_ASSERT(tm.mYear >= 1970)
            (tm.mYear);
        int64_t delta = tm.mYear - 1970;
        return delta / 400;
    }

    TimeComponent offset(const TimeComponent& tm) const
    {
        OTS_ASSERT(tm.mYear >= 1970)
            (tm.mYear);
        int64_t delta = tm.mYear - 1970;
        if (delta < 400) {
            return tm;
        }
        TimeComponent res = tm;
        res.mYear = 1970 + (delta % 400);
        return res;
    }

    const TimeComponent& offset(int64_t i) const
    {
        OTS_ASSERT(i >= 0 && i < totalDays())
            (i)
            (totalDays());
        return mDays[i];
    }
    
    int64_t days(const TimeComponent& tm) const
    {
        OTS_ASSERT(tm.mYear >= 1970 && tm.mYear < 1970 + 400)
            (tm.mYear);
        int64_t left = 0;
        int64_t right = mDays.size();
        for(; left <= right; ) {
            int64_t mid = (left + right) / 2;
            int c = cmp(mDays[mid], tm);
            if (c <= 0) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        return left - 1;
    }

private:
    vector<TimeComponent> mDays;
};

FourCenturies::FourCenturies()
{
    TimeComponent tm;
    tm.mYear = 1970;
    tm.mMonth = 1;
    tm.mDay = 1;
    for(;;) {
        mDays.push_back(tm);

        tm.incOneDay();
        if (tm.mYear >= 1970 + 400) {
            return;
        }
    }
}

TimeComponent decompose(const UtcTime& tm)
{
    OTS_ASSERT(tm.toUsec() >= 0)
        (tm.toUsec());

    TimeComponent res;
    int64_t t = tm.toUsec();
    res.mUsec = t % kUsecPerSec;
    t /= kUsecPerSec;

    res.mSec = t % 60;
    t /= 60;

    res.mMinute = t % 60;
    t /= 60;

    res.mHour = t % 24;
    t /= 24;

    const FourCenturies& fc = FourCenturies::get();

    int64_t period = t / fc.totalDays();
    const TimeComponent& tc = fc.offset(t % fc.totalDays());
    res.mYear = tc.mYear + period * 400;
    res.mMonth = tc.mMonth;
    res.mDay = tc.mDay;
    
    return res;
}

} // namespace

void UtcTime::toIso8601(string& out) const
{
    const TimeComponent& tc = decompose(*this);
    fill(out, tc.mYear, 4);
    out.push_back('-');
    fill(out, tc.mMonth, 2);
    out.push_back('-');
    fill(out, tc.mDay, 2);
    out.push_back('T');
    fill(out, tc.mHour, 2);
    out.push_back(':');
    fill(out, tc.mMinute, 2);
    out.push_back(':');
    fill(out, tc.mSec, 2);
    out.push_back('.');
    fill(out, tc.mUsec, 6);
    out.push_back('Z');
}

string UtcTime::toIso8601() const
{
    string res;
    res.reserve(sizeof("1970-01-01T00:00:00.000000Z"));
    toIso8601(res);
    return res;
}

void UtcTime::prettyPrint(string& out) const
{
    out.push_back('"');
    toIso8601(out);
    out.push_back('"');
}

UtcTime UtcTime::now()
{
    boost::system::error_code err;
    system_clock::time_point sc = system_clock::now(err);
    OTS_ASSERT(!err)(err.message());
    system_clock::duration d = sc.time_since_epoch();
    microseconds usec = duration_cast<microseconds>(d);
    return UtcTime(usec.count());
}

namespace {

Optional<string> goThroughChar(char exp, const uint8_t*& b, const uint8_t* e)
{
    if (b == e) {
        return Optional<string>(" premature ending");
    }
    if (*b != exp) {
        string r = " expect " + pp::prettyPrint(exp) + " got " + pp::prettyPrint(*b);
        return Optional<string>(r);
    }
    ++b;
    return Optional<string>();
}

Optional<string> parseNumber(int64_t& num, const uint8_t*& b, const uint8_t* e)
{
    num = 0;
    for(; b < e && *b >= '0' && *b <= '9'; ++b) {
        num *= 10;
        num += *b - '0';
        OTS_ASSERT(num >= 0)
            (num);
    }
    return Optional<string>();
}

Optional<string> parseSubsecond(int64_t& ssec, const uint8_t*& b, const uint8_t* e)
{
    if (b < e && *b == '.') {
        TRY(goThroughChar('.', b, e));
        int64_t cnt = 0;
        ssec = 0;
        for(; b < e && *b >= '0' && *b <= '9'; ++b, ++cnt) {
            ssec *= 10;
            ssec += *b - '0';
            OTS_ASSERT(ssec >= 0)
                (ssec);
        }
        for(; cnt < 6; ++cnt) {
            ssec *= 10;
        }
    }
    return Optional<string>();
}

Optional<string> parseIso8601(
    TimeComponent& st,
    const uint8_t* b,
    const uint8_t* e)
{
    TRY(parseNumber(st.mYear, b, e));
    TRY(goThroughChar('-', b, e));
    TRY(parseNumber(st.mMonth, b, e));
    TRY(goThroughChar('-', b, e));
    TRY(parseNumber(st.mDay, b, e));
    TRY(goThroughChar('T', b, e));
    TRY(parseNumber(st.mHour, b, e));
    TRY(goThroughChar(':', b, e));
    TRY(parseNumber(st.mMinute, b, e));
    TRY(goThroughChar(':', b, e));
    TRY(parseNumber(st.mSec, b, e));
    TRY(parseSubsecond(st.mUsec, b, e));
    TRY(goThroughChar('Z', b, e));
    if (b < e) {
        return Optional<string>("more chars than expected");
    }
    TRY(st.valid());
    return Optional<string>();
}

int64_t toTimestamp(const TimeComponent& st)
{
    const FourCenturies& fc = FourCenturies::get();
    int64_t period = fc.period(st);
    TimeComponent offset = fc.offset(st);
    int64_t dayOffset = fc.days(offset);
    int64_t day = period * fc.totalDays() + dayOffset;
    int64_t second = ((day * 24 + st.mHour) * 60 + st.mMinute) * 60 + st.mSec;
    return second * 1000000 + st.mUsec;
}

} // namespace

Result<UtcTime, string> UtcTime::parse(const MemPiece& in)
{
    Result<UtcTime, string> res;
    TimeComponent st;
    {
        const uint8_t* b = in.data();
        const uint8_t* e = b + in.length();
        Optional<string> err = parseIso8601(st, b, e);
        if (err.present()) {
            res.mutableErrValue() = pp::prettyPrint(in) + *err;
            return res;
        }
    }
    int64_t timestamp = toTimestamp(st);
    res.mutableOkValue() = UtcTime::fromUsec(timestamp);
    return res;
}

} // namespace util
} // namespace tablestore
} // namespace aliyun
