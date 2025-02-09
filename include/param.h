#pragma once

#include "record.h"
#include "row.h"
#include <cstdint>
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
