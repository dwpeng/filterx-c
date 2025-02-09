#include "param.h"
#include "process.h"
#include <iostream>
#include <tuple>

int
main(int argc, char** argv) {

  filterx::GroupParamsList group_params;
  filterx::FileParamsList file_params;
  filterx::ProcessorParams process_params = {
    .min_count = 1,
    .max_count = INT32_MAX,
    .fmin_count = 0.0001,
    .fmax_count = 1.0,
    .output_path = std::string("-"),
    .output_limit = -1,
    .output_separator = '\t',
  };
  filterx::parse(argc, argv, &group_params, &file_params, &process_params);
  filterx::check_file_params(&file_params);
  filterx::Processor processor(process_params);
  for (auto file_param : file_params) {
    auto record = filterx::create_record(&file_param);
    processor.add_record(record);
  }
  processor.prepare();
  processor.process();
  return 0;
}
