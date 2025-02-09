#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace filterx {

class Row {
public:
  Row() = default;
  Row(char* row, char separator) : separator(separator), row(row) {}
  Row(std::string& row, char separator) : separator(separator), row(row) {}

  inline void
  _remove_newline() {
    if (this->row.empty()) {
      return;
    }
    if (this->row.back() == '\n') {
      this->row.pop_back();
    }
    if (this->row.empty()) {
      return;
    }
    if (this->row.back() == '\r') {
      this->row.pop_back();
    }
    if (this->row.empty()) {
      return;
    }
    while (this->row.back() == this->separator) {
      this->row.pop_back();
    }
  }

  inline void
  split() {
    // [placehoder, start, length, start, length, ...]

    if (this->splited) {
      return;
    }
    this->_remove_newline();
    this->separator_index.clear();
    this->separator_index.push_back(0);
    int start = 0;
    int length = 0;

    for (int i = 0; i < this->row.size(); i++) {
      if (this->row[i] == this->separator) {
        this->row[i] = '\0';
        this->separator_index.push_back(start);
        this->separator_index.push_back(length);
        start = i + 1;
        length = 0;
      } else {
        length++;
      }
    }
    if (length != 0) {
      this->separator_index.push_back(start);
      this->separator_index.push_back(length);
    }
    this->splited = true;
  }

  std::string_view
  data() {
    return std::string_view(this->row.data(), this->row.size());
  }

  std::optional<std::string_view>
  get_item(int index) {
    assert(this->splited);
    if (index < 0 || index >= this->separator_index.size() / 2) {
      return std::nullopt;
    }
    int start = this->separator_index[index * 2 + 1];
    int length = this->separator_index[index * 2 + 2];
    if (length == 0) {
      return std::nullopt;
    }
    return std::string_view(this->row.data() + start, length);
  }

  std::optional<std::string_view>
  operator[](int index) {
    return this->get_item(index);
  }

  void
  clear() {
    this->splited = false;
    this->separator_index.clear();
  }

  void
  set_separator(char separator) {
    this->splited = false;
    this->separator = separator;
  }

  void
  set_row(char* row) {
    this->splited = false;
    this->row.clear();
    this->row.append(row);
  }

  void
  set_row_idx(uint32_t row_idx) {
    this->row_idx = row_idx;
  }

  void
  set_row(std::string& row) {
    this->set_row(row.data());
  }

  size_t
  size() {
    return this->separator_index.size() / 2;
  }

private:
  uint32_t row_idx;
  std::string row;
  std::vector<uint32_t> separator_index;
  char separator;
  bool splited = false;
};

enum RowKeyType {
  RowKeyTypeFloat = 1 << 0,
  RowKeyTypeInt = 1 << 1,
  RowKeyTypeString = 1 << 2,
  RowKeyTypeUnknown = 1 << 3,
};

enum RowKeySortOrder {
  RowKeySortOrderAsc = 1 << 0,
  RowKeySortOrderDesc = 1 << 1,
  RowKeySortOrderUnknown = 1 << 2,
};

class TypedKey {
public:
  RowKeySortOrder sort_order;
  RowKeyType type;
  std::string_view key;
  union {
    int64_t int_value;
    double float_value;
  } value;
  bool cached = false;
  int error_code;

  TypedKey() = default;
  TypedKey(RowKeyType type, std::string_view key, RowKeySortOrder sort_order)
      : type(type), key(key), sort_order(sort_order) {}

  void
  update_key(std::string_view key) {
    this->error_code = 0;
    this->key = key;
    this->cached = false;
  }

  void
  update_type(RowKeyType type) {
    this->error_code = 0;
    this->type = type;
  }

  void
  update_sort_order(RowKeySortOrder sort_order) {
    this->error_code = 0;
    this->sort_order = sort_order;
  }

  bool
  has_error() {
    return this->error_code != 0;
  }

  char*
  error_message() {
    return std::strerror(this->error_code);
  }

  int64_t
  to_int() {
    assert(this->type == RowKeyTypeInt);
    if (this->cached) {
      return this->value.int_value;
    }
    this->cached = true;
    // check if can convert to int
    int i = 0;
    while (i < this->key.size()) {
      if (this->key[i] < '0' || this->key[i] > '9') {
        fprintf(stderr, "convert to int error: %s\n", this->key.data());
        fprintf(stderr, "Check separator(s) or key type(k)\n");
        exit(EXIT_FAILURE);
      }
      i++;
    }
    auto v = std::stoll(std::string(this->key));
    this->error_code = errno;
    this->value.int_value = v;
    return v;
  }

  double
  to_float() {
    assert(this->type == RowKeyTypeFloat);
    if (this->cached) {
      return this->value.float_value;
    }
    this->cached = true;
    auto v = std::stod(std::string(this->key));
    this->error_code = errno;
    this->value.float_value = v;
    return v;
  }

  std::string_view
  to_string() {
    return this->key;
  }

  bool
  equals(TypedKey* other) {
    assert(this->type == other->type);
    // match statement
    bool r = false;
    if (this->type == RowKeyTypeInt) {
      r = this->to_int() == other->to_int();
    }
    if (this->type == RowKeyTypeFloat) {
      r = this->to_float() == other->to_float();
    }
    if (this->type == RowKeyTypeString) {
      r = this->to_string() == other->to_string();
    }
    if (this->has_error() || other->has_error()) {
      return this->to_string() == other->to_string();
    }
    return r;
  }

  bool
  small_than(TypedKey* other) {
    assert(this->type == other->type);
    bool r = false;
    if (this->type == RowKeyTypeInt) {
      r = this->to_int() < other->to_int();
    }
    if (this->type == RowKeyTypeFloat) {
      r = this->to_float() < other->to_float();
    }
    if (this->type == RowKeyTypeString) {
      r = this->to_string() < other->to_string();
    }
    if (this->has_error() || other->has_error()) {
      return false;
    }
    return r;
  }

  bool
  big_than(TypedKey* other) {
    assert(this->type == other->type);
    bool r = false;
    if (this->type == RowKeyTypeInt) {
      r = this->to_int() > other->to_int();
    }
    if (this->type == RowKeyTypeFloat) {
      r = this->to_float() > other->to_float();
    }
    if (this->type == RowKeyTypeString) {
      r = this->to_string() > other->to_string();
    }
    if (this->has_error() || other->has_error()) {
      return false;
    }
    return r;
  }
};

class RowKey {
public:
  RowKey(std::vector<uint32_t>& keys, std::vector<RowKeyType>& key_types,
         std::vector<RowKeySortOrder>& sort_order)
      : keys(keys), key_types(key_types), sort_order(sort_order) {
    assert(keys.size() == key_types.size());
  }

  size_t
  size() {
    return this->keys.size();
  }

  void
  update_row(Row* row) {
    assert(row != nullptr);
    this->row = row;
  }

  bool
  equals(RowKey* other) {
    if (this->keys.size() != other->keys.size() || this->keys.empty()) {
      return false;
    }
    if (this->row == nullptr || other->row == nullptr) {
      return false;
    }
    for (int i = 0; i < this->keys.size(); i++) {
      auto left_typed_key = this->get_key(i).value_or(nullptr);
      auto right_typed_key = other->get_key(i).value_or(nullptr);
      if (left_typed_key == nullptr || right_typed_key == nullptr) {
        return false;
      }
      if (!left_typed_key->equals(right_typed_key)) {
        return false;
      }
    }
    return true;
  }

  std::optional<TypedKey*>
  get_key(int index) {
    if (index < 0 || index >= this->keys.size()) {
      return std::nullopt;
    }
    auto key = this->row->get_item(this->keys[index]);
    if (!key.has_value()) {
      return std::nullopt;
    }
    auto kt = this->key_types[index];
    auto so = this->sort_order[index];
    this->typed_key.update_key(key.value());
    this->typed_key.update_type(kt);
    this->typed_key.update_sort_order(so);
    return &this->typed_key;
  }

  void
  string_key() {
    printf("RowKey: ");
    for (int i = 0; i < this->keys.size(); i++) {
      auto key = this->get_key(i).value_or(nullptr);
      if (key == nullptr) {
        printf("null ");
        continue;
      }
      printf("%s ", key->to_string().data());
    }
    printf("\n");
  }

private:
  filterx::Row* row;
  std::vector<uint32_t> keys;
  std::vector<RowKeyType> key_types;
  std::vector<RowKeySortOrder> sort_order;
  TypedKey typed_key;
};
} // namespace filterx
