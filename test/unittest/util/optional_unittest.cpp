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
#include "tablestore/util/optional.hpp"
#include "testa/testa.hpp"
#include <tr1/functional>
#include <deque>
#include <string>

using namespace std;
using namespace std::tr1;
using namespace std::tr1::placeholders;

namespace aliyun {
namespace tablestore {

void Optional_object(const string&)
{
    deque<int> xs;
    xs.push_back(1);
    util::Optional<deque<int> > opCopy(xs);
    TESTA_ASSERT(pp::prettyPrint(xs) == "[1]" && pp::prettyPrint(*opCopy) == "[1]")
        (xs)
        (*opCopy)
        .issue();

    util::Optional<deque<int> > opMove;
    opMove.reset(util::move(xs));
    TESTA_ASSERT(pp::prettyPrint(xs) == "[]" && pp::prettyPrint(*opMove) == "[1]")
        (xs)
        (*opMove)
        .issue();
}
TESTA_DEF_JUNIT_LIKE1(Optional_object);

void Optional_scalar(const string&)
{
    int x = 1;
    util::Optional<int> opCopy(x);
    TESTA_ASSERT(pp::prettyPrint(x) == "1" && pp::prettyPrint(*opCopy) == "1")
        (x)
        (*opCopy)
        .issue();

    util::Optional<int> opMove;
    opMove.reset(util::move(x));
    TESTA_ASSERT(pp::prettyPrint(x) == "1" && pp::prettyPrint(*opMove) == "1")
        (x)
        (*opMove)
        .issue();
}
TESTA_DEF_JUNIT_LIKE1(Optional_scalar);

void Optional_ref(const string&)
{
    int x = 1;
    int& rx = x;
    util::Optional<int&> op(rx);
    TESTA_ASSERT(pp::prettyPrint(*op) == "1")
        (x)
        (*op)
        .issue();
    rx = 2;
    TESTA_ASSERT(pp::prettyPrint(*op) == "2")
        (x)
        (*op)
        .issue();

    int y = -1;
    int& ry = y;
    op.reset(ry);
    TESTA_ASSERT(pp::prettyPrint(x) == "2" && pp::prettyPrint(*op) == "-1")
        (x)
        (y)
        (*op)
        .issue();
}
TESTA_DEF_JUNIT_LIKE1(Optional_ref);

namespace {

util::Optional<int> inc(int x)
{
    return util::Optional<int>(x + 1);
}

void Optional_apply(const string&)
{
    {
        util::Optional<int> in;
        util::Optional<int> res = in.apply(inc).apply(inc);
        TESTA_ASSERT(!res.present()).issue();
    }
    {
        function<util::Optional<int>(int)> xinc = bind(inc, _1);
        util::Optional<int> in(0);
        util::Optional<int> res = in.apply(xinc).apply(xinc);
        TESTA_ASSERT(*res == 2)
            (*res)
            .issue();
    }
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(Optional_apply);

namespace {
void Optional_equiv(const string&)
{
    deque<util::Optional<int> > ops;
    ops.push_back(util::Optional<int>());
    ops.push_back(util::Optional<int>(1));
    ops.push_back(util::Optional<int>(2));
    for(size_t i = 0; i < ops.size(); ++i) {
        for(size_t j = 0; j < ops.size(); ++j) {
            if (i == j) {
                TESTA_ASSERT(ops[i] == ops[j])(i)(j).issue();
            } else {
                TESTA_ASSERT(ops[i] != ops[j])(i)(j).issue();
            }
        }
    }
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(Optional_equiv);

} // namespace tablestore
} // namespace aliyun
