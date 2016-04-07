/*
 *
 * Copyright 2015-2016, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <memory>
#include <set>

#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>

#include <gflags/gflags.h>
#include <grpc/support/log.h>

#include "test/cpp/qps/driver.h"
#include "test/cpp/qps/report.h"
#include "test/cpp/util/benchmark_config.h"

DEFINE_string(scenarios_file, "",
              "JSON file containing an array of Scenario objects");
DEFINE_string(scenarios_json, "",
              "JSON string containing an array of Scenario objects");

namespace grpc {
namespace testing {

static void QpsDriver() {
  grpc::string json;

  if (FLAGS_scenarios_file != "") {
    if (FLAGS_scenarios_json != "") {
      gpr_log(GPR_ERROR,
              "Only one of --scenarios_file or --scenarios_json must be set");
      abort();
    }
    // Read the json data from disk
    FILE *json_file = fopen(FLAGS_scenarios_file.c_str(), "r");
    GPR_ASSERT(json_file != NULL);
    fseek(json_file, 0, SEEK_END);
    long len = ftell(json_file);
    char *data = new char[len];
    fseek(json_file, 0, SEEK_SET);
    GPR_ASSERT(len == (long)fread(data, 1, len, json_file));
    fclose(json_file);
    json = grpc::string(data, data + len);
    delete[] data;
  } else if (FLAGS_scenarios_json != "") {
    json = FLAGS_scenarios_json.c_str();
  } else {
    gpr_log(GPR_ERROR,
            "One of --scenarios_file or --scenarios_json must be set");
    abort();
  }

  // Parse into an array of scenarios
  Scenarios scenarios;
  std::unique_ptr<google::protobuf::util::TypeResolver> type_resolver(
      google::protobuf::util::NewTypeResolverForDescriptorPool(
          "type.googleapis.com",
          google::protobuf::DescriptorPool::generated_pool()));
  grpc::string binary;
  auto status = JsonToBinaryString(type_resolver.get(),
                                   "type.googleapis.com/grpc.testing.Scenarios",
                                   json, &binary);
  if (!status.ok()) {
    grpc::string msg(status.error_message());
    gpr_log(GPR_ERROR, "Failed to convert json to binary: errcode=%d msg=%s",
            status.error_code(), msg.c_str());
    gpr_log(GPR_ERROR, "JSON: ", json.c_str());
    abort();
  }
  GPR_ASSERT(scenarios.ParseFromString(binary));

  for (int i = 0; i < scenarios.scenarios_size(); i++) {
    const Scenario &scenario = scenarios.scenarios(i);
    std::cerr << "RUNNING SCENARIO: " << scenario.name() << "\n";
    const auto result =
        RunScenario(scenario.client_config(), scenario.num_clients(),
                    scenario.server_config(), scenario.num_servers(),
                    scenario.warmup_seconds(), scenario.benchmark_seconds(),
                    scenario.spawn_local_worker_count());

    GetReporter()->ReportQPS(*result);
    GetReporter()->ReportQPSPerCore(*result);
    GetReporter()->ReportLatency(*result);
    GetReporter()->ReportTimes(*result);
  }
}

}  // namespace testing
}  // namespace grpc

int main(int argc, char **argv) {
  grpc::testing::InitBenchmark(&argc, &argv, true);

  grpc::testing::QpsDriver();

  return 0;
}