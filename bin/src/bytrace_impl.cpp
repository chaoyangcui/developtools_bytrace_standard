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
#include <atomic>
#include <climits>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>
#include "bytrace.h"
#include "hilog/log.h"
#include "parameters.h"

using namespace std;
using namespace OHOS::HiviewDFX;

#define EXPECTANTLY(exp) (__builtin_expect(!!(exp), true))
#define UNEXPECTANTLY(exp) (__builtin_expect(!!(exp), false))

namespace {
int g_markerFd = -1;
std::once_flag g_onceFlag;

std::atomic<bool> g_isBytraceInit(false);
std::atomic<uint64_t> g_tagsProperty(BYTRACE_TAG_NOT_READY);

const std::string KEY_TRACE_TAG = "debug.bytrace.tags.enableflags";
const std::string KEY_APP_NUMBER = "debug.bytrace.app_number";
const std::string KEY_RO_DEBUGGABLE = "ro.debuggable";

constexpr int NAME_MAX_SIZE = 1000;
static std::vector<std::string> g_markTypes = {"B", "E", "S", "F", "C"};
enum MarkerType { MARKER_BEGIN, MARKER_END, MARKER_ASYNC_BEGIN, MARKER_ASYNC_END, MARKER_INT, MARKER_MAX };

bool IsAppValid()
{
    // Judge if application-level tracing is enabled.
    if (OHOS::system::GetBoolParameter(KEY_RO_DEBUGGABLE, 0)) {
        std::ifstream fs;
        fs.open("/proc/self/cmdline");
        if (!fs.is_open()) {
            fprintf(stderr, "IsAppValid, open /proc/self/cmdline failed.\n");
            return false;
        }

        std::string lineStr;
        std::getline(fs, lineStr);
        std::string keyPrefix = "debug.bytrace.app_";
        int nums = OHOS::system::GetIntParameter<int>(KEY_APP_NUMBER, 0);
        for (int i = 0; i < nums; i++) {
            std::string keyStr = keyPrefix + std::to_string(i);
            std::string val = OHOS::system::GetParameter(keyStr, "");
            if (val == "*" || val == lineStr) {
                fs.close();
                return true;
            }
        }
    }
    return false;
}

uint64_t GetSysParamTags()
{
    // Get the system parameters of KEY_TRACE_TAG.
    uint64_t tags = OHOS::system::GetUintParameter<uint64_t>(KEY_TRACE_TAG, 0);
    if (tags == 0) {
        fprintf(stderr, "GetUintParameter %s error.\n", KEY_TRACE_TAG.c_str());
        return 0;
    }

    IsAppValid();
    return (tags | BYTRACE_TAG_ALWAYS) & BYTRACE_TAG_VALID_MASK;
}

// open file "trace_marker".
void OpenTraceMarkerFile()
{
    const std::string debugFile = "/sys/kernel/debug/tracing/trace_marker";
    const std::string traceFile = "/sys/kernel/tracing/trace_marker";
    g_markerFd = open(debugFile.c_str(), O_WRONLY | O_CLOEXEC);
    if (g_markerFd == -1) {
        g_markerFd = open(traceFile.c_str(), O_WRONLY | O_CLOEXEC);
        if (g_markerFd == -1) {
            fprintf(stderr, "Error opening trace file.\n");
            g_tagsProperty = 0;
            return;
        }
    }
    g_tagsProperty = GetSysParamTags();
    g_isBytraceInit = true;
}
}; // namespace

inline void AddBytraceMarker(MarkerType type, uint64_t tag, std::string name, std::string value)
{
    if (UNEXPECTANTLY(!g_isBytraceInit)) {
        std::call_once(g_onceFlag, OpenTraceMarkerFile);
    }
    if (UNEXPECTANTLY(g_tagsProperty & tag)) {
        // record fomart: "type|pid|name value".
        std::string record = g_markTypes[type] + "|";
        record += std::to_string(getpid()) + "|";
        record += (name.size() < NAME_MAX_SIZE) ? name : name.substr(0, NAME_MAX_SIZE);
        record += " " + value;
        write(g_markerFd, record.c_str(), record.size());
    }
}

void UpdateTraceLabel()
{
    if (!g_isBytraceInit) {
        return;
    }
    g_tagsProperty = GetSysParamTags();
}

void StartTrace(uint64_t label, const string& value, float limit)
{
    string traceName = "H:" + value;
    AddBytraceMarker(MARKER_BEGIN, label, traceName, "");
}

void StartTraceDebug(uint64_t label, const string& value, float limit)
{
#if (TRACE_LEVEL >= DEBUG_LEVEL)
    string traceName = "H:" + value + GetHiTraceInfo();
    AddBytraceMarker(MARKER_BEGIN, label, traceName, "");
#endif
}

void FinishTrace(uint64_t label, const string& value)
{
    AddBytraceMarker(MARKER_END, label, "", "");
}

void FinishTraceDebug(uint64_t label, const string& value)
{
#if (TRACE_LEVEL >= DEBUG_LEVEL)
    AddBytraceMarker(MARKER_END, label, "", "");
#endif
}

void StartAsyncTrace(uint64_t label, const string& value, int32_t taskId, float limit)
{
    string traceName = "H:" + value;
    AddBytraceMarker(MARKER_ASYNC_BEGIN, label, traceName, std::to_string(taskId));
}

void StartAsyncTraceDebug(uint64_t label, const string& value, int32_t taskId, float limit)
{
#if (TRACE_LEVEL >= DEBUG_LEVEL)
    string traceName = "H:" + value;
    AddBytraceMarker(MARKER_ASYNC_BEGIN, label, traceName, std::to_string(taskId));
#endif
}

void FinishAsyncTrace(uint64_t label, const string& value, int32_t taskId)
{
    string traceName = "H:" + value;
    AddBytraceMarker(MARKER_ASYNC_END, label, traceName, std::to_string(taskId));
}

void FinishAsyncTraceDebug(uint64_t label, const string& value, int32_t taskId)
{
#if (TRACE_LEVEL >= DEBUG_LEVEL)
    string traceName = "H:" + value;
    AddBytraceMarker(MARKER_ASYNC_END, label, traceName, std::to_string(taskId));
#endif
}

void MiddleTrace(uint64_t label, const string& beforeValue, const std::string& afterValue)
{
    string traceName = "H:" + afterValue;
    AddBytraceMarker(MARKER_END, label, "", "");
    AddBytraceMarker(MARKER_BEGIN, label, traceName, "");
}

void MiddleTraceDebug(uint64_t label, const string& beforeValue, const std::string& afterValue)
{
#if (TRACE_LEVEL >= DEBUG_LEVEL)
    string traceName = "H:" + afterValue + GetTraceInfo();
    AddBytraceMarker(MARKER_END, label, "", "");
    AddBytraceMarker(MARKER_BEGIN, label, traceName, "");
#endif
}

void CountTrace(uint64_t label, const string& name, int64_t count)
{
    string traceName = "H:" + name;
    AddBytraceMarker(MARKER_INT, label, traceName, std::to_string(count));
}

void CountTraceDebug(uint64_t label, const string& name, int64_t count)
{
#if (TRACE_LEVEL >= DEBUG_LEVEL)
    string traceName = "H:" + name;
    AddBytraceMarker(MARKER_INT, label, traceName, std::to_string(count));
#endif
}
