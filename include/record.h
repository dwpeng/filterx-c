#pragma once

#include <optional>
#include <string>

#include "data_provider.h"
#include "row_buffer.h"

namespace filterx {

enum RecordStatus {
  RecordStatusEmpty = 0,
  RecordStatusTryToReadNext = 1,
  RecordStatusWaitConsumption = 2,
  RecordStatusNotPassCondition = 4,
  RecordStatusUnavailable = 5,
  RecordStatusWaitOutput = 6,
  RecordStatusNotPassExists = 7,
  RecordStatusEof = 8,
};

enum ExistCondition {
  ExistConditionOptional = 0,
  ExistConditionMust = 1,
  ExistConditionNot = 2,
};

class Record {
public:
  Record(char* path, char separator, std::vector<uint32_t>& row_keys,
         std::vector<RowKeyType>& key_types,
         std::vector<RowKeySortOrder>& sort_order,
         std::optional<char> comment = std::nullopt)
      : row_buffer(row_keys, key_types, sort_order, separator), path(path) {
    this->data_provider = createDataProvider(path);
    this->min_count = 1;
    this->max_count = INT32_MAX;
    this->must_exist = ExistConditionOptional;
    this->cut_columns = std::vector<int>();
  }
  Record(std::string& path, char separator, std::vector<uint32_t>& row_keys,
         std::vector<RowKeyType>& key_types,
         std::vector<RowKeySortOrder>& sort_order,
         std::optional<char> comment = std::nullopt)
      : Record(path.data(), separator, row_keys, key_types, sort_order,
               comment) {}

  ~Record() { delete this->data_provider; }

  void
  set_cut_columns(std::vector<int>& cut_columns) {
    this->cut_columns = cut_columns;
  }

  void
  set_count(int min_count = 1, int max_count = INT32_MAX) {
    if (min_count < 0 || max_count < 0) {
      fprintf(stderr,
              "min_count and max_count must be greater than 0\n min_count: %d, "
              "max_count: %d\n",
              min_count, max_count);
      fflush(stderr);
      exit(EXIT_FAILURE);
    }
    if (max_count > 0 && min_count > max_count) {
      fprintf(stderr,
              "min_count must be less than or equal to max_count\n min_count: "
              "%d, max_count: %d\n",
              min_count, max_count);
      fflush(stderr);
      exit(EXIT_FAILURE);
    }
    this->min_count = min_count;
    this->max_count = max_count;
  }

  void
  set_must_exist(ExistCondition must_exist) {
    this->must_exist = must_exist;
  }

  ExistCondition
  get_exist() {
    return this->must_exist;
  }

  bool
  equals(Record& other) {
    auto key_key = this->row_buffer.key();
    auto record_key = other.row_buffer.key();
    return key_key == record_key;
  }

  RecordStatus
  __next() {
    assert(this->data_provider != nullptr);
    if (this->record_status == RecordStatusEof) {
      return RecordStatusEof;
    }
    this->__consume();
    assert(this->record_status == RecordStatusEmpty
           || this->record_status == RecordStatusTryToReadNext
           || this->record_status == RecordStatusNotPassCondition);
    while (1) {
      auto line = this->data_provider->readline();
      if (!line.has_value()) {
        this->record_status = RecordStatusEof;
        break;
      }
      if (line.value()[0] == this->comment) {
        continue;
      }
      // printf("line: %s\n", line.value().data());
      if (!this->row_buffer.add_row(const_cast<char*>(line.value().data()),
                                    this->data_provider->line_number)) {
        break;
      }
    }
    if (this->row_buffer.size() > 0) {
      this->record_status = RecordStatusWaitConsumption;
    } else {
      this->record_status = RecordStatusEof;
      return RecordStatusEof;
    }
    return this->record_status;
  }

  RecordStatus
  next() {
    while (1) {
      auto s = this->__next();
      if (s == RecordStatusEof) {
        return RecordStatusEof;
      }
      if (this->check_count_condition()) {
        return RecordStatusWaitConsumption;
      }
      auto key = this->key().value_or(nullptr);
      if (key == nullptr) {
        continue;
      }
      int pass = 1;
      for (int i = 0; i < key->size(); i++) {
        auto k = key->get_key(i).value_or(nullptr);
        if (k == nullptr) {
          pass = 0;
          break;
        }
      }
      if (pass) {
        return RecordStatusWaitConsumption;
      }
    }
  }

  RecordStatus
  status() {
    return this->record_status;
  }

  RowBuffer*
  __consume() {
    if (this->record_status == RecordStatusEof
        || this->record_status == RecordStatusEmpty) {
      return nullptr;
    }
    assert(this->record_status == RecordStatusWaitConsumption
           || this->record_status == RecordStatusNotPassCondition
           || this->record_status == RecordStatusUnavailable
           || this->record_status == RecordStatusWaitOutput
           || this->record_status == RecordStatusNotPassExists);
    this->row_buffer.consume();
    if (this->row_buffer.size() == 0) {
      this->record_status = RecordStatusEmpty;
    } else {
      this->record_status = RecordStatusTryToReadNext;
    }
    return &this->row_buffer;
  }

  bool
  check_count_condition() {
    if (this->record_status == RecordStatusEof) {
      return false;
    }
    assert(this->record_status == RecordStatusWaitConsumption);
    if (this->row_buffer.size() >= this->min_count
        && this->row_buffer.size() <= this->max_count) {
      return true;
    }
    this->record_status = RecordStatusNotPassCondition;
    return false;
  }

  std::optional<RowKey*>
  key() {
    return this->row_buffer.key();
  }

  RowBuffer*
  buffer() {
    return &this->row_buffer;
  }

  std::vector<int>&
  get_cut_columns() {
    return this->cut_columns;
  }

  void
  set_status(RecordStatus status) {
    this->record_status = status;
  }

  void
  set_record_limit(int limit) {
    this->record_limit = limit;
  }

  int
  get_record_limit() {
    if (this->record_limit == -1) {
      return this->row_buffer.size();
    }
    if (this->record_limit > this->row_buffer.size()) {
      return this->row_buffer.size();
    }
    return this->record_limit;
  }

  void
  set_comment(char comment) {
    this->comment = comment;
  }

  char
  get_comment() {
    return this->comment;
  }

  void
  set_placehoder(char placehoder) {
    this->placehoder = placehoder;
  }

  char
  get_placehoder() {
    return this->placehoder;
  }

private:
  uint32_t min_count;
  uint32_t max_count;
  std::string path;
  RowBuffer row_buffer;
  DataProvider* data_provider;
  RecordStatus record_status = RecordStatusEmpty;
  std::vector<int> cut_columns;
  ExistCondition must_exist;
  int record_limit = -1;
  char comment = '#';
  char placehoder = '-';
};

} // namespace filterx
