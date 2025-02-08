#include "process.h"
#include "record.h"
#include <iostream>

int
main(int argc, char** argv) {
  auto keys = std::vector<uint32_t>{ 1 };
  auto key_types = std::vector<filterx::RowKeyType>{
    filterx::RowKeyTypeInt,
  };
  auto sort_order = std::vector<filterx::RowKeySortOrder>{
    filterx::RowKeySortOrderDesc,
  };

  auto cut1 = std::vector<int>{ 1 };
  auto cut2 = std::vector<int>{ 1 };
  auto cut3 = std::vector<int>{ 1 };
  auto path1 = std::string("/home/dwp/c/filterx/test-data/1.csv");
  auto path2 = std::string("/home/dwp/c/filterx/test-data/2.csv");
  auto path3 = std::string("/home/dwp/c/filterx/test-data/3.csv");
  auto output = std::string("-");
  auto record1 = filterx::Record(path1, ',', keys, key_types, sort_order);
  auto record2 = filterx::Record(path2, ',', keys, key_types, sort_order);
  auto record3 = filterx::Record(path3, ',', keys, key_types, sort_order);
  record1.set_cut_columns(cut1);
  record2.set_cut_columns(cut2);
  record3.set_cut_columns(cut3);

  // record3.set_must_exist(true);

  auto processor_params = filterx::ProcessorParams{
    .min_count = 2,
    .max_count = 10,
    .fmin_count = 0.0,
    .fmax_count = 1.0,
    .output_separator = '\t',
    .placehoder = '-',
    .output_path = const_cast<char*>(output.c_str()),
  };
  auto p = filterx::Processor(processor_params);
  p.add_record(&record1);
  p.add_record(&record2);
  p.add_record(&record3);
  p.prepare();
  p.process();
  return 0;
}
