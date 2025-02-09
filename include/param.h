#pragma once

#include "record.h"
#include "row.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace filterx {

struct GroupParams {
  std::vector<uint32_t> row_keys;
  std::vector<RowKeyType> key_types;
  std::vector<RowKeySortOrder> sort_order;
  std::vector<int> cut_columns;
  char separator;
  int record_limit;
  ExistCondition must_exist;
  int min_count;
  int max_count;
  char comment;
  char placehoder;
};

struct FileParams {
  std::string path;
  char separator;
  std::vector<uint32_t> row_keys;
  std::vector<RowKeyType> key_types;
  std::vector<RowKeySortOrder> sort_order;
  std::vector<int> cut_columns;
  ExistCondition must_exist;
  int record_limit;
  int min_count;
  int max_count;
  char comment;
  char placehoder;
};

static void
print_file_params(FileParams* file_params) {
  std::cout << "path: " << file_params->path << std::endl;
  std::cout << "separator: " << file_params->separator << std::endl;
  std::cout << "row_keys: ";
  for (auto key : file_params->row_keys) {
    std::cout << key << " ";
  }
  std::cout << std::endl;
  std::cout << "key_types: ";
  for (auto key : file_params->key_types) {
    std::cout << key << " ";
  }
  std::cout << std::endl;
  std::cout << "sort_order: ";
  for (auto key : file_params->sort_order) {
    std::cout << key << " ";
  }
  std::cout << std::endl;
  std::cout << "cut_columns: ";
  for (auto key : file_params->cut_columns) {
    std::cout << key << " ";
  }
  std::cout << std::endl;
  std::cout << "must_exist: " << file_params->must_exist << std::endl;
  std::cout << "record_limit: " << file_params->record_limit << std::endl;
  std::cout << "min_count: " << file_params->min_count << std::endl;
  std::cout << "max_count: " << file_params->max_count << std::endl;
  std::cout << "comment: " << file_params->comment << std::endl;
  std::cout << "placehoder: " << file_params->placehoder << std::endl;
}

struct ProcessorParams {
  uint32_t min_count;
  uint32_t max_count;
  float fmin_count;
  float fmax_count;
  std::string output_path;
  int output_limit;
  char output_separator;
};

typedef std::vector<std::tuple<int, filterx::GroupParams> > GroupParamsList;
typedef std::vector<filterx::FileParams> FileParamsList;

Record* create_record(FileParams* params);
void apply_group_to_file(FileParams* file_params, GroupParams* group_params);
FileParams parse_file_params(const char* arg,
                             GroupParamsList* group_params_list);
GroupParams parse_group_params(const char* arg);
void check_file_params(FileParamsList* file_params_list);

void parse(int argc, char** argv, GroupParamsList* group_params_list,
           FileParamsList* file_params_list, ProcessorParams* processor_params);

} // namespace filterx
