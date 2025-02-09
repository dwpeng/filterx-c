#include "param.h"
#include "process.h"

namespace filterx {

static GroupParams defaultGroupParams = {
  .row_keys = { UINT32_MAX },
  .key_types = { RowKeyTypeUnknown },
  .sort_order = { RowKeySortOrderUnknown },
  .cut_columns = { -1 },
  .separator = '\0',
  .record_limit = -1,
  .must_exist = ExistConditionOptional,
  .min_count = 1,
  .max_count = INT32_MAX,
  .comment = '#',
  .placehoder = '-',
};

static FileParams defaultFileParams = {
  .path = "",
  .separator = '\0',
  .row_keys = { UINT32_MAX },
  .key_types = { RowKeyTypeUnknown },
  .sort_order = { RowKeySortOrderUnknown },
  .cut_columns = { -1 },
  .must_exist = ExistConditionOptional,
  .record_limit = -1,
  .min_count = 1,
  .max_count = INT32_MAX,
  .comment = '#',
  .placehoder = '-',
};

Record*
create_record(FileParams* params) {
  auto record = new Record(params->path, params->separator, params->row_keys,
                           params->key_types, params->sort_order);
  record->set_cut_columns(params->cut_columns);
  record->set_must_exist(params->must_exist);
  record->set_record_limit(params->record_limit);
  record->set_count(params->min_count, params->max_count);
  record->set_comment(params->comment);
  record->set_placehoder(params->placehoder);
  return record;
}

void
apply_group_to_file(FileParams* file_params, GroupParams* group_params) {
  // if not set, use the group configuration
  if (group_params->separator != defaultFileParams.separator) {
    file_params->separator = group_params->separator;
  }
  if (group_params->row_keys != defaultFileParams.row_keys) {
    file_params->row_keys = group_params->row_keys;
  }
  if (group_params->key_types != defaultFileParams.key_types) {
    file_params->key_types = group_params->key_types;
  }
  if (group_params->sort_order != defaultFileParams.sort_order) {
    file_params->sort_order = group_params->sort_order;
  }
  if (group_params->cut_columns != defaultFileParams.cut_columns) {
    file_params->cut_columns = group_params->cut_columns;
  }
  if (group_params->must_exist != defaultFileParams.must_exist) {
    file_params->must_exist = group_params->must_exist;
  }
  if (group_params->record_limit != defaultFileParams.record_limit) {
    file_params->record_limit = group_params->record_limit;
  }
  if (group_params->min_count != defaultFileParams.min_count) {
    file_params->min_count = group_params->min_count;
  }
  if (group_params->max_count != defaultFileParams.max_count) {
    file_params->max_count = group_params->max_count;
  }
  if (group_params->comment != defaultFileParams.comment) {
    file_params->comment = group_params->comment;
  }
  if (group_params->placehoder != defaultFileParams.placehoder) {
    file_params->placehoder = group_params->placehoder;
  }
}

static const char ARG_SEPARATOR = ':';

typedef struct {
  char separator;
  char comment;
  int min_count;
  int max_count;
  int record_limit;
  char placehoder;
  ExistCondition exist;
  std::vector<uint32_t> row_keys;
  std::vector<RowKeyType> key_types;
  std::vector<RowKeySortOrder> sort_order;
  std::vector<int> cut_columns;
  std::vector<int> group_numbers;

} ParseAges;

static char SPERAATOR[] = { ',', ':', '\t', ' ' };

char
query_separator(const char* arg) {
  for (int i = 0; i < sizeof(SPERAATOR) / sizeof(SPERAATOR[0]); i++) {
    if (strchr(arg, SPERAATOR[i]) != nullptr) {
      return SPERAATOR[i];
    }
  }
  return arg[0];
}

bool
parse_args(ParseAges* A, const char* arg, std::string* error) {
  size_t len = strlen(arg);
  if (len == 0) {
    *error = "args is empty";
    return false;
  }
  size_t idx = 0;
  // attribute: [key1][op][value1]:[key2][op][value2]:[key3]:[key4][op][value4]
  // key: single character or pure number or multiple characters
  // op: = (optional)
  // value: single character or pure number or multiple characters (optional)

  // key op value
  // c   =  single character
  // s   =  single character
  // m   =  pure number
  // M   =  pure number
  // l   =  pure number
  // p   =  single character
  // cut =  1,2,3,4 or 1-4 or 1,2-4 or 1-2,3-4
  // group number * *
  // freq = 0.0001,1.0
  // cnt = 1,222,2147483647

  while (idx < len) {
    size_t start = idx;
    while (idx < len && arg[idx] != ARG_SEPARATOR) {
      idx++;
    }
    size_t end = idx;
    if (end == start) {
      break;
    }
    std::string_view attr(arg + start, end - start);

    // parse cut columns
    // cut=1,2,3,4 or 1,2,3,4, or cut=1-4 or cut=1,2-4 or cut=1-2,3-4 or cut=:
    // or cut=
    if (attr.size() > 3 && attr[0] == 'c' && attr[1] == 'u' && attr[2] == 't') {
      // remove last : or ,
      while (attr.back() == ':' || attr.back() == ',') {
        attr.remove_suffix(1);
      }

      std::string_view value = attr.substr(4);
      if (value.empty()) {
        idx++;
        continue;
      }

      std::string_view::size_type pos = 0;
      while (pos < value.size()) {
        std::string_view::size_type start = pos;
        while (pos < value.size() && value[pos] != ',') {
          pos++;
        }
        std::string_view cut = value.substr(start, pos - start);
        if (cut.find('-') == std::string::npos) {
          auto c = std::stoi(std::string(cut));
          if (c == 0) {
            *error = "cut column number starts from 1, but got 0";
            return false;
          }
          A->cut_columns.push_back(c - 1);
        } else {
          std::string_view::size_type dash = cut.find('-');
          int start = std::stoi(std::string(cut.substr(0, dash)));
          int end = std::stoi(std::string(cut.substr(dash + 1)));
          if (start < 0 || end < 0) {
            *error = "cut column number starts from 1, but got minus";
            return false;
          }
          if (start == 0 || end == 0) {
            *error = "cut column number starts from 1, but got 0";
            return false;
          }
          start--;
          end--;
          if (start > end) {
            *error = "cut column start is greater than end";
            return false;
          }
          for (int i = start; i <= end; i++) {
            A->cut_columns.push_back(i);
          }
        }
        pos++;
      }
      idx++;
      continue;
    }

    // if attr contains =
    if (attr.find('=') == std::string::npos) {
      // group number, pure number
      if (attr[0] >= '0' && attr[0] <= '9') {
        int group_number = std::stoi(std::string(attr));
        if (group_number < 0) {
          *error = "group number is invalid";
          return false;
        }
        A->group_numbers.push_back(group_number);
        idx++;
        continue;
      } else {
        if (attr.size() < 3) {
          *error = "attribute is too short";
          return false;
        }
        if (attr[1] != '=') {
          *error = "attribute is invalid";
          return false;
        }
      }
    }
    char key = attr[0];
    std::string_view value = attr.substr(2);
    switch (key) {
    case 's':
      A->separator = query_separator(value.data());
      break;
    case 'c':
      A->comment = value[0];
      break;
    case 'm':
      A->min_count = std::stoi(std::string(value));
      break;
    case 'M':
      A->max_count = std::stoi(std::string(value));
      break;
    case 'l':
      A->record_limit = std::stoi(std::string(value));
      break;
    case 'p':
      A->placehoder = value[0];
      break;
    case 'e':
      if (value == "Y") {
        A->exist = ExistConditionMust;
      } else if (value == "N") {
        A->exist = ExistConditionNot;
      } else {
        A->exist = ExistConditionOptional;
      }
      break;
    case 'k': {
      // parse keys
      // k=[value]
      // value: [col_number][type]
      // type: f float, i int, s string (asecending) from small to large
      // type: F float, I int, S string (descending) from large to small
      // example: 1f2i3S
      // 1f: column 1 is float, ascending
      // 2i: column 2 is int, ascending
      // 3s: column 3 is string, descending

      // col_number: pure number e.g. 1, 222, ...
      // type: f, i, s, F, I, S

      // parse keys

      std::string_view::size_type pos = 0;
      while (pos < value.size()) {
        std::string_view::size_type start = pos;
        while (pos < value.size() && value[pos] >= '0' && value[pos] <= '9') {
          pos++;
        }
        std::string_view col = value.substr(start, pos - start);
        if (col.empty()) {
          *error = "key column is empty";
          return false;
        }
        start = pos;
        while (pos < value.size()
               && (value[pos] == 'f' || value[pos] == 'i' || value[pos] == 's'
                   || value[pos] == 'F' || value[pos] == 'I'
                   || value[pos] == 'S')) {
          pos++;
        }
        std::string_view type = value.substr(start, pos - start);
        if (type.empty()) {
          *error = "key type is empty";
          return false;
        }
        uint32_t col_number = std::stoi(std::string(col));
        RowKeyType key_type = RowKeyTypeUnknown;
        RowKeySortOrder sort_order = RowKeySortOrderUnknown;
        switch (type[0]) {
        case 'f':
          key_type = RowKeyTypeFloat;
          sort_order = RowKeySortOrderAsc;
          break;
        case 'i':
          key_type = RowKeyTypeInt;
          sort_order = RowKeySortOrderAsc;
          break;
        case 's':
          key_type = RowKeyTypeString;
          sort_order = RowKeySortOrderAsc;
          break;
        case 'F':
          key_type = RowKeyTypeFloat;
          sort_order = RowKeySortOrderDesc;
          break;
        case 'I':
          key_type = RowKeyTypeInt;
          sort_order = RowKeySortOrderDesc;
          break;
        case 'S':
          key_type = RowKeyTypeString;
          sort_order = RowKeySortOrderDesc;
          break;
        default:
          *error = "key type is unknown";
          return false;
        }
        if (col_number == 0) {
          *error = "key column number starts from 1, but got 0";
          return false;
        }
        A->row_keys.push_back(col_number - 1);
        A->key_types.push_back(key_type);
        A->sort_order.push_back(sort_order);
        pos++;
      }
      break;
    }
    default:
      *error = "unknown attribute";
      return false;
    }
    idx++;
  }
  return true;
}

FileParams
parse_file_params(const char* arg, GroupParamsList* group_params_list) {
  ParseAges A = {
    .separator = ',',
    .comment = '#',
    .min_count = 1,
    .max_count = INT32_MAX,
    .record_limit = -1,
    .placehoder = '-',
    .exist = ExistConditionOptional,
  };

  // parse path
  size_t len = strlen(arg);
  size_t idx = 0;
  while (idx < len && arg[idx] != ARG_SEPARATOR) {
    idx++;
  }
  std::string path(arg, idx);

  arg += idx + 1;

  std::string error;
  if (idx < len && !parse_args(&A, arg, &error)) {
    fprintf(stderr, "parse file params error: %s\n", error.c_str());
    exit(EXIT_FAILURE);
  }
  FileParams file_params = defaultFileParams;
  file_params.path = path;
  file_params.separator = A.separator;
  file_params.min_count = A.min_count;
  file_params.max_count = A.max_count;
  file_params.record_limit = A.record_limit;
  file_params.comment = A.comment;
  file_params.placehoder = A.placehoder;
  file_params.cut_columns = A.cut_columns;
  file_params.must_exist = A.exist;
  if (A.row_keys.size() > 0) {
    file_params.row_keys = A.row_keys;
    file_params.key_types = A.key_types;
    file_params.sort_order = A.sort_order;
  }

  // check if 1 in group_numbers
  int has_one = 0;
  for (auto group_number : A.group_numbers) {
    if (group_number == 1) {
      has_one = 1;
      break;
    }
  }

  if (!has_one) {
    // insert 1 to group_numbers's head
    A.group_numbers.insert(A.group_numbers.begin(), 1);
  }

  for (auto group_number : A.group_numbers) {
    int found = 0;
    for (auto group_params : *group_params_list) {
      auto group_id = std::get<0>(group_params);
      if (group_id == group_number) {
        apply_group_to_file(&file_params, &std::get<1>(group_params));
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "group number %d is not found\n", group_number);
      exit(EXIT_FAILURE);
    }
  }
  return file_params;
}

GroupParams
parse_group_params(const char* arg) {
  ParseAges A = {
    .separator = ',',
    .comment = '#',
    .min_count = 1,
    .max_count = INT32_MAX,
    .record_limit = -1,
    .placehoder = '-',
    .cut_columns = {},
    .group_numbers = {},
  };

  std::string error;
  if (!parse_args(&A, arg, &error)) {
    fprintf(stderr, "parse group params error: %s\n", error.c_str());
    exit(EXIT_FAILURE);
  }
  GroupParams group_params = defaultGroupParams;
  group_params.separator = A.separator;
  group_params.min_count = A.min_count;
  group_params.max_count = A.max_count;
  group_params.record_limit = A.record_limit;
  group_params.comment = A.comment;
  group_params.placehoder = A.placehoder;
  group_params.cut_columns = A.cut_columns;
  group_params.must_exist = A.exist;
  if (A.row_keys.size() > 0) {
    group_params.row_keys = A.row_keys;
    group_params.key_types = A.key_types;
    group_params.sort_order = A.sort_order;
  }
  return group_params;
}

void
check_file_params(FileParamsList* file_params_list) {
  for (auto file_params : *file_params_list) {
    if (file_params.path.empty()) {
      fprintf(stderr, "file path is empty\n");
      exit(EXIT_FAILURE);
    }
    // check if file exists
    FILE* file = fopen(file_params.path.data(), "r");
    if (file == nullptr) {
      fprintf(stderr, "file %s does not exist\n", file_params.path.data());
      exit(EXIT_FAILURE);
    }
    fclose(file);
    if (file_params.separator == '\0') {
      fprintf(stderr, "file separator is empty\n");
      exit(EXIT_FAILURE);
    }
    if (file_params.row_keys.empty()) {
      fprintf(stderr, "file row keys is empty\n");
      exit(EXIT_FAILURE);
    }
    if (file_params.key_types.empty()) {
      fprintf(stderr, "file key types is empty\n");
      exit(EXIT_FAILURE);
    }
    if (file_params.sort_order.empty()) {
      fprintf(stderr, "file sort order is empty\n");
      exit(EXIT_FAILURE);
    }
  }
  // ensure all file's param keys are the same
  for (int i = 1; i < file_params_list->size(); i++) {
    auto prev = file_params_list->at(i - 1);
    auto curr = file_params_list->at(i);
    if (prev.row_keys.size() != curr.row_keys.size()) {
      fprintf(stderr, "file row keys size is not the same\n");
      exit(EXIT_FAILURE);
    }
    if (prev.key_types.size() != curr.key_types.size()) {
      fprintf(stderr, "file key types size is not the same\n");
      exit(EXIT_FAILURE);
    }
    if (prev.sort_order.size() != curr.sort_order.size()) {
      fprintf(stderr, "file sort order size is not the same\n");
      exit(EXIT_FAILURE);
    }
    for (int j = 0; j < prev.row_keys.size(); j++) {
      if (prev.row_keys[j] != curr.row_keys[j]) {
        fprintf(stderr, "file row keys is not the same\n");
        exit(EXIT_FAILURE);
      }
      if (prev.key_types[j] != curr.key_types[j]) {
        fprintf(stderr, "file key types is not the same\n");
        exit(EXIT_FAILURE);
      }
      if (prev.sort_order[j] != curr.sort_order[j]) {
        fprintf(stderr, "file sort order is not the same\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

void
help() {
  fprintf(stderr, "Usage: filterx [options] [file:attribute]\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -[1-9]+ <group>  Group filter conditions\n");
  fprintf(stderr,
          "  -L <limit>       Output [L] record limit, default is -1\n");
  fprintf(stderr, "  -o, --output <file>  Output file, default is stdout\n");
  fprintf(stderr, "  -s, --separator <separator>  Separator, default is ,\n");
  fprintf(stderr, "  -h, --help       Show this help message\n");
  fprintf(stderr, "  -cnt=<min>,<max>  Count range, default is 1,2147483647\n");
  fprintf(stderr,
          "  -freq=<min>,<max> Frequency range, default is 0.0001,1.0\n");
  fprintf(stderr, "List of attributes:\n");
  fprintf(stderr, "  s=<separator>    Separator, default is ,\n");
  fprintf(stderr, "  c=<comment>      Comment, default is #\n");
  fprintf(stderr, "  m=<min_count>    Min count, default is 1\n");
  fprintf(stderr, "  M=<max_count>    Max count, default is 2147483647\n");
  fprintf(stderr, "  l=<record_limit> Record limit, default is -1\n");
  fprintf(stderr, "  p=<placehoder>   Placehoder, default is -\n");
  fprintf(stderr,
          "  e=[Y|N]        Y: must exist, N: not exist, default is either\n");
  fprintf(stderr, "  k=<key>          Key, e.g. 1f2i3S\n");
  fprintf(
      stderr,
      "    There are three types of keys: float(f/F), int(i/I), string(s/S\n");
  fprintf(stderr,
          "    f: float, i: int, s: string (ascending) from small to large\n");
  fprintf(stderr,
          "    F: float, I: int, S: string (descending) from large to small\n");
  fprintf(stderr, "  cut=<columns>    Cut columns, e.g. 1,2,3,4 or 1,2-6 "
                  "default only output key columns\n");
  fprintf(stderr, "  <group>          Group number, default is all\n");
  fprintf(stderr, "Examples:\n");
  fprintf(stderr, "  filterx -1 \"k=1s:cut=1,2,3,4\" "
                  "file1\n");
}

void
apply_key_column_to_file(FileParams* file_params) {
  if (!file_params->cut_columns.empty()) return;
  for (int i = 0; i < file_params->row_keys.size(); i++) {
    file_params->cut_columns.push_back(file_params->row_keys[i]);
  }
}

void
parse(int argc, char** argv, GroupParamsList* group_params_list,
      FileParamsList* file_params_list, ProcessorParams* processor_params) {

  if (argc == 1) {
    help();
    exit(EXIT_SUCCESS);
  }

  // parse process params
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      continue;
    }
    if (argv[i][1] >= '0' && argv[i][1] <= '9') {
      continue;
    }
    if (strcmp(argv[i], "-L") == 0) {
      processor_params->output_limit = std::stoi(argv[i + 1]);
      i++;
      continue;
    }
    // output_separator
    if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--separator") == 0) {
      processor_params->output_separator = query_separator(argv[i + 1]);
      i++;
      continue;
    }
    if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
      processor_params->output_path = argv[i + 1];
      i++;
      continue;
    }
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      help();
      exit(EXIT_SUCCESS);
    }

    // parse -cnt=1,100 or -cnt=1, or -cnt=,100
    if (strcmp(argv[i], "-cnt") == 0) {
      std::string cnt(argv[i + 1]);
      processor_params->min_count = 1;
      processor_params->max_count = INT32_MAX;
      std::string::size_type pos = cnt.find(',');
      if (pos == std::string::npos) {
        processor_params->min_count = std::stoll(cnt);
        processor_params->max_count = std::stoll(cnt);
      } else {
        std::string start = "1";
        std::string end = "2147483647";
        std::string s = cnt.substr(0, pos);
        if (s.size() > 0) {
          start = s;
        }
        s = cnt.substr(pos + 1);
        if (s.size() > 0) {
          end = s;
        }
        processor_params->min_count = std::stoll(start);
        processor_params->max_count = std::stoll(end);
        if (processor_params->min_count < 0) {
          fprintf(
              stderr,
              "min_count should be greater than or equal to 0, but got %d\n",
              processor_params->min_count);
          exit(EXIT_FAILURE);
        }
        if (processor_params->max_count < 0) {
          fprintf(
              stderr,
              "max_count should be greater than or equal to 0, but got %d\n",
              processor_params->max_count);
          exit(EXIT_FAILURE);
        }
        if (processor_params->min_count > processor_params->max_count) {
          fprintf(stderr,
                  "min_count should be less than or equal to max_count, but "
                  "got min_count: %d, max_count: %d\n",
                  processor_params->min_count, processor_params->max_count);
          exit(EXIT_FAILURE);
        }
      }
      i++;
      continue;
    }

    // parse -freq=0.0001,1.0 or -freq=0.0001, or -freq=,1.0
    if (strcmp(argv[i], "-freq") == 0) {
      std::string freq(argv[i + 1]);
      processor_params->fmin_count = 0.0001;
      processor_params->fmax_count = 1.0;
      std::string::size_type pos = freq.find(',');
      if (pos == std::string::npos) {
        processor_params->fmin_count = std::stof(freq);
        processor_params->fmax_count = std::stof(freq);
      } else {
        std::string start = "0.0001";
        std::string end = "1.0";
        std::string s = freq.substr(0, pos);
        if (s.size() > 0) {
          start = s;
        }
        s = freq.substr(pos + 1);
        if (s.size() > 0) {
          end = s;
        }
        processor_params->fmin_count = std::stof(start);
        processor_params->fmax_count = std::stof(end);
        if (processor_params->fmin_count < 0.0
            || processor_params->fmin_count > 1.0) {
          fprintf(stderr,
                  "fmin_count should be in range [0.0, 1.0], but got %f\n",
                  processor_params->fmin_count);
          exit(EXIT_FAILURE);
        }
        if (processor_params->fmax_count < 0.0
            || processor_params->fmax_count > 1.0) {
          fprintf(stderr,
                  "fmax_count should be in range [0.0, 1.0], but got %f\n",
                  processor_params->fmax_count);
          exit(EXIT_FAILURE);
        }
        if (processor_params->fmin_count > processor_params->fmax_count) {
          fprintf(stderr,
                  "fmin_count should be less than or equal to fmax_count, but "
                  "got fmin_count: %f, fmax_count: %f\n",
                  processor_params->fmin_count, processor_params->fmax_count);
          exit(EXIT_FAILURE);
        }
      }
      i++;
      continue;
    }
  }

  // parse group params first
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      continue;
    }
    if (argv[i][1] >= '0' && argv[i][1] <= '9') {
      auto group_params = parse_group_params(argv[i + 1]);
      // parse group number
      int group_id = std::stoi(std::string(argv[i] + 1));
      if (group_id < 0) {
        fprintf(stderr, "group number should start from 1, but got %d\n",
                group_id);
        exit(EXIT_FAILURE);
      }
      group_params_list->push_back(std::make_tuple(group_id, group_params));
      i++;
    }
  }

  // parse file params
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      i++;
      continue;
    }
    if (argv[i][1] >= '0' && argv[i][1] <= '9') {
      i++;
      continue;
    }
    auto file_params = parse_file_params(argv[i], group_params_list);
    apply_key_column_to_file(&file_params);
    file_params_list->push_back(file_params);
  }
}

} // namespace filterx
