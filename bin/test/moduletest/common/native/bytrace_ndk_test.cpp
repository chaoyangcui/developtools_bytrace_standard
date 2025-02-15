/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <regex>
#include <string>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <hilog/log.h>
#include "bytrace.h"
#include "bytrace_capture.h"
#include "parameters.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS::HiviewDFX;
namespace OHOS {
namespace Developtools {
namespace BytraceTest {
const string TRACE_MARKER_PATH = "trace_marker";
const string TRACING_ON_PATH = "tracing_on";
const string TRACING_ON = "tracing_on";
const string TRACE_PATH = "trace";
const string TRACE_MARK_WRITE = "tracing_mark_write";
const string TRACE_PATTERN = "\\s*(.*?)-(.*?)\\s+(.*?)\\[(\\d+?)\\]\\s+(.*?)\\s+((\\d+).(\\d+)?):\\s+"
    + TRACE_MARK_WRITE + ": ";
const string TRACE_START = TRACE_PATTERN + "B\\|(.*?)\\|H:";
const string TRACE_FINISH = TRACE_PATTERN + "E\\|";
const string TRACE_ASYNC_START = TRACE_PATTERN + "S\\|(.*?)\\|H:";
const string TRACE_ASYNC_FINISH = TRACE_PATTERN + "F\\|(.*?)\\|H:";
const string TRACE_COUNT = TRACE_PATTERN + "C\\|(.*?)\\|H:";
const string TRACE_PROPERTY = "debug.bytrace.tags.enableflags";
constexpr uint32_t TASK = 1;
constexpr uint32_t TID = 2;
constexpr uint32_t TGID = 3;
constexpr uint32_t CPU = 4;
constexpr uint32_t DNH2 = 5;
constexpr uint32_t TIMESTAMP = 6;
constexpr uint32_t PID = 9;
constexpr uint32_t TRACE_NAME = 10;
constexpr uint32_t NUM = 11;

constexpr uint32_t TRACE_FMA11 = 11;
constexpr uint32_t TRACE_FMA12 = 12;

constexpr uint64_t TRACE_INVALIDATE_TAG = 0x1000000;
constexpr uint64_t BYTRACE_TAG =  0xd03301;
const constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, BYTRACE_TAG, "BYTRACE_TEST"};
const uint64_t TAG = BYTRACE_TAG_OHOS;
static string g_traceRootPath;

bool SetProperty(const string& property, const string& value);
string GetProperty(const string& property, const string& value);
bool CleanTrace();
bool CleanFtrace();
bool SetFtrace(const string& filename, bool enabled);

class BytraceNDKTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown(){};
};

void  BytraceNDKTest::SetUpTestCase()
{
    const string debugfsDir = "/sys/kernel/debug/tracing/";
    const string tracefsDir = "/sys/kernel/tracing/";
    if (access((debugfsDir + TRACE_MARKER_PATH).c_str(), F_OK) != -1) {
        g_traceRootPath = debugfsDir;
    } else if (access((tracefsDir + TRACE_MARKER_PATH).c_str(), F_OK) != -1) {
        g_traceRootPath = tracefsDir;
    } else {
        HiLog::Error(LABEL, "Error: Finding trace folder failed");
    }
    CleanFtrace();
}

void BytraceNDKTest::TearDownTestCase()
{
    SetProperty(TRACE_PROPERTY, "0");
    SetFtrace(TRACING_ON, false);
    CleanTrace();
}

void BytraceNDKTest::SetUp()
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    string value = to_string(TAG);
    SetProperty(TRACE_PROPERTY, value);
    HiLog::Info(LABEL, "current tag is %{public}s", GetProperty(TRACE_PROPERTY, "0").c_str());
    ASSERT_TRUE(GetProperty(TRACE_PROPERTY, "-123") == value);
    UpdateTraceLabel();
}

struct Param {
    string m_task;
    string m_tid;
    string m_tgid;
    string m_cpu;
    string m_dnh2;
    string m_timestamp;
    string m_pid;
    string m_traceName;
    string m_num;
};

class MyTrace {
    Param m_param;
    bool m_loaded;
public:
    MyTrace() : m_loaded(false)
    {
        m_param.m_task = "";
        m_param.m_tid = "";
        m_param.m_tgid = "";
        m_param.m_cpu = "";
        m_param.m_dnh2 = "";
        m_param.m_timestamp = "";
        m_param.m_pid = "";
        m_param.m_traceName = "";
        m_param.m_num = "";
    }

    ~MyTrace()
    {
    }

    // task-pid  ( tig) [cpu] ...1    timestamp: tracing_mark_write: B|pid|traceName
    // task-pid  ( tig) [cpu] ...1    timestamp: tracing_mark_write: E|pid
    void Load(const Param& param)
    {
        m_param.m_task = param.m_task;
        m_param.m_pid = param.m_pid;
        m_param.m_tid = param.m_tid;
        m_param.m_tgid = param.m_tgid;
        m_param.m_cpu = param.m_cpu;
        m_param.m_dnh2 = param.m_dnh2;
        m_param.m_timestamp = param.m_timestamp;
        m_param.m_traceName = param.m_traceName;
        m_param.m_num = param.m_num;
        m_loaded = true;
    }

    string GetTask()
    {
        return m_param.m_task;
    }

    string GetPid()
    {
        if (m_loaded) {
            return m_param.m_pid;
        }
        return "";
    }

    string GetTgid()
    {
        if (m_loaded) {
            return m_param.m_tgid;
        }
        return "";
    }

    string GetCpu()
    {
        if (m_loaded) {
            return m_param.m_cpu;
        }
        return "";
    }

    string GetDnh2()
    {
        if (m_loaded) {
            return m_param.m_dnh2;
        }
        return "";
    }

    string GetTimestamp()
    {
        if (m_loaded) {
            return m_param.m_timestamp;
        }
        return "";
    }

    string GetTraceName()
    {
        if (m_loaded) {
            return m_param.m_traceName;
        }
        return "";
    }

    string GetNum()
    {
        if (m_loaded) {
            return m_param.m_num;
        }
        return "";
    }

    string GetTid()
    {
        if (m_loaded) {
            return m_param.m_tid;
        }
        return "";
    }

    bool IsLoaded() const
    {
        return m_loaded;
    }
};

bool SetProperty(const string& property, const string& value)
{
    bool result = false;
    result = OHOS::system::SetParameter(property, value);
    if (!result) {
        HiLog::Error(LABEL, "Error: setting %s failed", property.c_str());
        return false;
    }
    return true;
}

string GetProperty(const string& property, const string& value)
{
    return OHOS::system::GetParameter(property, value);
}

bool GetTimeDuration(int64_t time1, int64_t time2, int64_t diffRange)
{
    int64_t duration = time2 - time1;
    return (duration > 0) && (duration <= diffRange ? true : false);
}

string& Trim(string& s)
{
    if (s.empty()) {
        return s;
    }
    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}

int64_t GetTimeStamp(string str)
{
    if (str == "") {
        return 0;
    }
    int64_t time;
    Trim(str);
    time = atol(str.erase(str.find("."), 1).c_str());
    return time;
}

MyTrace GetTraceResult(const string& checkContent, const vector<string>& list)
{
    MyTrace trace;
    if (list.empty() || checkContent == "") {
        return trace;
    }
    regex pattern(checkContent);
    smatch match;
    Param param {""};
    for (int i = list.size() - 1; i >= 0; i--) {
        if (regex_match(list[i],  match, pattern)) {
            param.m_task = match[TASK];
            param.m_tid =  match[TID];
            param.m_tgid = match[TGID];
            param.m_cpu = match[CPU];
            param.m_dnh2 = match[DNH2];
            param.m_timestamp = match[TIMESTAMP];
            param.m_pid = match[PID];
            if (match.size() == TRACE_FMA11) {
                param.m_traceName =   match[TRACE_NAME],
                param.m_num = "";
            } else if (match.size() == TRACE_FMA12) {
                param.m_traceName =   match[TRACE_NAME],
                param.m_num = match[NUM];
            } else {
                param.m_traceName = "";
                param.m_num = "";
            }
            trace.Load(param);
            break;
        }
    }
    return trace;
}

static bool WriteStringToFile(const string& fileName, const string& str)
{
    if (g_traceRootPath.empty()) {
        HiLog::Error(LABEL, "Error: trace path not found.");
        return false;
    }
    ofstream out;
    out.open(g_traceRootPath + fileName, ios::out);
    out << str;
    out.close();
    return true;
}

bool CleanTrace()
{
    if (g_traceRootPath.empty()) {
        HiLog::Error(LABEL, "Error: trace path not found.");
        return false;
    }
    ofstream ofs;
    ofs.open(g_traceRootPath + TRACE_PATH, ofstream::out);
    if (!ofs.is_open()) {
        HiLog::Error(LABEL, "Error: opening trace path failed.");
        return false;
    }
    ofs << "";
    ofs.close();
    return true;
}

static stringstream ReadFile(const string& filename)
{
    ifstream fin(filename.c_str());
    stringstream ss;
    if (!fin.is_open()) {
        fprintf(stderr, "opening file: %s failed!", filename.c_str());
        return ss;
    }
    ss << fin.rdbuf();
    fin.close();
    return ss;
}

static bool IsFileExisting(const string& filename)
{
    return access(filename.c_str(), F_OK) != -1;
}

bool SetFtrace(const string& filename, bool enabled)
{
    return WriteStringToFile(filename, enabled ? "1" : "0");
}

bool CleanFtrace()
{
    return WriteStringToFile("set_event", "");
}

string GetFinishTraceRegex(MyTrace& trace)
{
    if (!trace.IsLoaded()) {
        return "";
    } else {
        return "\\s*(.*?)-(" + trace.GetTid() + "?)\\s+(.*?)\\[(\\d+?)\\]\\s+(.*?)\\s+" + "((\\d+).(\\d+)?):\\s+" +
               TRACE_MARK_WRITE + ": E\\|(" + trace.GetPid() + ")|(.*)";
    }
}

vector<string> ReadFile2string(const string& filename)
{
    vector<string> list;
    if (IsFileExisting(filename)) {
        stringstream ss = ReadFile(filename);
        string line;
        while (getline(ss, line)) {
            list.emplace_back(move(line));
        }
    }
    return list;
}

vector<string> ReadTrace()
{
    return ReadFile2string(g_traceRootPath + TRACE_PATH);
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node normal output start tracing and end tracing.
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_001, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TAG, "StartTraceTest001");
    FinishTrace(TAG, "StartTraceTest001");
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_START + "(StartTraceTest001) ", list);
    ASSERT_TRUE(startTrace.IsLoaded()) << "Can't find \"B|pid|StartTraceTest001\" from trace.";
    MyTrace finishTrace = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    ASSERT_TRUE(finishTrace.IsLoaded()) << "Can't find \"E|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node has no output.
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_002, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TAG, "StartTraceTest002");
    FinishTrace(TAG, "StartTraceTest002");
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_START + "(StartTraceTest002) ", list);
    ASSERT_TRUE(startTrace.IsLoaded()) << "Can't find \"B|pid|StartTraceTest002\" from trace.";
    MyTrace finishTrace = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    ASSERT_TRUE(finishTrace.IsLoaded()) << "Can't find \"E|\" from trace.";
}

/**
  * @tc.name: bytrace
  * @tc.desc: tracing_mark_write file node normal output start trace and end trace.
  * @tc.type: FUNC
  */
HWTEST_F(BytraceNDKTest, StartTrace_003, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TAG, "StartTraceTest003 %s");
    FinishTrace(TAG, "StartTraceTest003 %s");
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_START + "(StartTraceTest003 %s) ", list);
    ASSERT_TRUE(startTrace.IsLoaded()) << "Can't find \"B|pid|StartTraceTest003 %s\" from trace.";
    MyTrace finishTrace = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    ASSERT_TRUE(finishTrace.IsLoaded()) << "Can't find \"E|\" from trace.";
    ASSERT_TRUE(CleanTrace());
    list.clear();
    StartTrace(TAG, "StartTraceTest003 %p");
    FinishTrace(TAG, "StartTraceTest003 %p");
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    list = ReadTrace();
    MyTrace startTrace2 = GetTraceResult(TRACE_START + "(StartTraceTest003 %p) ", list);
    MyTrace finishTrace2 = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    ASSERT_TRUE(finishTrace2.IsLoaded()) << "Can't find \"E|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: test Input and output interval 1ms execution, time fluctuation 1ms
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_004, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TAG, "StartTraceTest004");
    usleep(1000);
    FinishTrace(TAG, "StartTraceTest004");
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_START + "(StartTraceTest004) ", list);
    ASSERT_TRUE(startTrace.IsLoaded()) << "Can't find \"B|pid|StartTraceTest004\" from trace.";
    MyTrace finishTrace = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    ASSERT_TRUE(finishTrace.IsLoaded()) << "Can't find \"E|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_005, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTrace(TAG, "asyncTraceTest005", 123);
    FinishAsyncTrace(TAG, "asyncTraceTest005", 123);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_ASYNC_START + "(asyncTraceTest005) (.*)", list);
    ASSERT_TRUE(startTrace.IsLoaded()) << "Can't find \"S|pid|asyncTraceTest005\" from trace.";
    MyTrace finishTrace =
        GetTraceResult(TRACE_ASYNC_FINISH + startTrace.GetTraceName() + " " + startTrace.GetNum(), list);
    ASSERT_TRUE(finishTrace.IsLoaded()) << "Can't find \"F|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_006, TestSize.Level0)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    CountTrace(TAG, "countTraceTest006", 1);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace countTrace = GetTraceResult(TRACE_COUNT + "(countTraceTest006) (.*)", list);
    ASSERT_TRUE(countTrace.IsLoaded()) << "Can't find \"C|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace.
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_007, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TRACE_INVALIDATE_TAG, "StartTraceTest007");
    FinishTrace(TRACE_INVALIDATE_TAG, "StartTraceTest007");
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_START + "(StartTraceTest007)", list);
    EXPECT_FALSE(startTrace.IsLoaded()) << "Can't find \"B|pid|StartTraceTest007\" from trace.";
    MyTrace finishTrace = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    EXPECT_FALSE(finishTrace.IsLoaded()) << "Can't find \"E|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace.
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_008, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTrace(TRACE_INVALIDATE_TAG, "StartTraceTest008 %s");
    FinishTrace(TRACE_INVALIDATE_TAG, "StartTraceTest008 %s");
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_START + "(StartTraceTest008 %s)", list);
    EXPECT_FALSE(startTrace.IsLoaded()) << "Can't find \"B|pid|StartTraceTest008 %s\" from trace.";
    MyTrace finishTrace = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    EXPECT_FALSE(finishTrace.IsLoaded()) << "Can't find \"E|\" from trace.";
    ASSERT_TRUE(CleanTrace());
    list.clear();
    StartTrace(TRACE_INVALIDATE_TAG, "StartTraceTest008 %p");
    FinishTrace(TRACE_INVALIDATE_TAG, "StartTraceTest008 %p");
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    list = ReadTrace();
    MyTrace startTrace2 = GetTraceResult(TRACE_START + "(StartTraceTest008 %p)", list);
    MyTrace finishTrace2 = GetTraceResult(GetFinishTraceRegex(startTrace), list);
    EXPECT_FALSE(finishTrace2.IsLoaded()) << "Can't find \"E|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_009, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTrace(TRACE_INVALIDATE_TAG, "asyncTraceTest009", 123);
    FinishAsyncTrace(TRACE_INVALIDATE_TAG, "asyncTraceTest009", 123);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace startTrace = GetTraceResult(TRACE_ASYNC_START + "(asyncTraceTest009)\\|(.*)", list);
    EXPECT_FALSE(startTrace.IsLoaded()) << "Can't find \"S|pid|asyncTraceTest009\" from trace.";
    MyTrace finishTrace = GetTraceResult(TRACE_ASYNC_FINISH + startTrace.GetTraceName() + "\\|"
        + startTrace.GetNum(), list);
    EXPECT_FALSE(finishTrace.IsLoaded()) << "Can't find \"F|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node normal output start trace and end trace
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_010, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    CountTrace(TRACE_INVALIDATE_TAG, "countTraceTest010", 1);
    ASSERT_TRUE(SetFtrace(TRACING_ON, false)) << "Setting tracing_on failed.";
    vector<string> list = ReadTrace();
    MyTrace countTrace = GetTraceResult(TRACE_COUNT + "(countTraceTest010)\\|(.*)", list);
    EXPECT_FALSE(countTrace.IsLoaded()) << "Can't find \"C|\" from trace.";
}

/**
 * @tc.name: bytrace
 * @tc.desc: tracing_mark_write file node general output start and end tracing for debugging.
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_011, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTraceDebug(TAG, "StartTraceTest011");
    FinishTraceDebug(TAG, "StartTraceTest011");
}

/**
  * @tc.name: bytrace
  * @tc.desc: tracing_mark_write file node general output start and end tracing for debugging.
  * @tc.type: FUNC
  */
HWTEST_F(BytraceNDKTest, StartTrace_012, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartTraceDebug(TAG, "StartTraceTest012 %s");
    FinishTraceDebug(TAG, "StartTraceTest012 %s");
}

/**
 * @tc.name: bytrace
 * @tc.desc: Testing StartAsyncTraceDebug and FinishAsyncTraceDebug functions
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_013, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    StartAsyncTraceDebug(TAG, "asyncTraceTest013", 123);
    FinishAsyncTraceDebug(TAG, "asyncTraceTest013", 123);
}

/**
 * @tc.name: bytrace
 * @tc.desc: Testing CountTraceDebug function
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_014, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    CountTraceDebug(TAG, "countTraceTest014", 1);
}

/**
 * @tc.name: bytrace
 * @tc.desc: Testing MiddleTrace function
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_015, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    MiddleTrace(TAG, "MiddleTraceTest015", "050tseTecarTelddiM");
}

/**
 * @tc.name: bytrace
 * @tc.desc: Testing MiddleTraceDebug function
 * @tc.type: FUNC
 */
HWTEST_F(BytraceNDKTest, StartTrace_016, TestSize.Level1)
{
    ASSERT_TRUE(CleanTrace());
    ASSERT_TRUE(SetFtrace(TRACING_ON, true)) << "Setting tracing_on failed.";
    MiddleTraceDebug(TAG, "MiddleTraceTest016", "061tseTecarTelddiM");
}
} // namespace BytraceTest
} // namespace Developtools
} // namespace OHOS
