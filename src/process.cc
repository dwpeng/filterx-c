#include "process.h"

namespace filterx {

Processor::Processor(ProcessorParams& params) : params(params) {
  if (strcmp(params.output_path.c_str(), "-") == 0) {
    this->output_file = stdout;
    return;
  }
  this->output_file = fopen(params.output_path.c_str(), "w");
  if (this->output_file == nullptr) {
    fprintf(stderr, "Failed to open file: %s\n", params.output_path.c_str());
    fflush(stderr);
    exit(EXIT_FAILURE);
  }
}

void
Processor::add_record(Record* record) {
  this->records.push_back(record);
  record->set_id(this->records.size());
}

void
Processor::prepare() {
  for (int i = 0; i < this->records.size(); i++) {
    RecordStatus s;
    while (1) {
      s = this->records[i]->next();
      if (s == RecordStatusEof) {
        if (i == 0) {
          fprintf(stderr, "The first record is empty\n");
          fflush(stderr);
          exit(EXIT_FAILURE);
        }
        break;
      }
      if (s == RecordStatusWaitConsumption) {
        break;
      }
    }
  }
}

// column mode
void
Processor::flush_all_records_to_file() {
  int max_rows = 0;
  for (int i = 0; i < this->records.size(); i++) {
    if (this->records[i]->status() != RecordStatusWaitOutput) {
      continue;
    }
    auto record = this->records[i];
    if (record->get_cut_columns().empty()) {
      continue;
    }
    auto nrows = record->get_record_limit();
    if (nrows > max_rows) {
      max_rows = nrows;
    }
  }
  auto output_buffer = this->output_buffer;
  for (int n = 0; n < max_rows; n++) {
    for (int i = 0; i < this->records.size(); i++) {
      if (this->records[i]->status() != RecordStatusWaitOutput) {
        auto cut = this->records[i]->get_cut_columns();
        char placehoder = this->records[i]->get_placehoder();
        for (int j = 0; j < cut.size(); j++) {
          // fprintf(this->output_file, "%c%c", placehoder,
          //         this->params.output_separator);
          output_buffer.push_back(placehoder);
          output_buffer.push_back(this->params.output_separator);
        }
        continue;
      }
      auto record = this->records[i];
      auto buffer = record->buffer();
      auto cut = record->get_cut_columns();
      if (n >= record->get_record_limit()) {
        char placehoder = record->get_placehoder();
        for (int j = 0; j < cut.size(); j++) {
          // fprintf(this->output_file, "%c%c", placehoder,
          //         this->params.output_separator);
          output_buffer.push_back(placehoder);
          output_buffer.push_back(this->params.output_separator);
        }
        continue;
      }
      auto row = buffer->get_row(n).value_or(nullptr);
      char placehoder = record->get_placehoder();
      for (int j = 0; j < cut.size(); j++) {
        auto item = row->get_item(cut[j]);
        if (item.has_value()) {
          auto item_value = item.value();
          // fprintf(this->output_file, "%s%c", item_value.data(),
          //         this->params.output_separator);
          output_buffer.append(item_value.data());
          output_buffer.push_back(this->params.output_separator);
        } else {
          // fprintf(this->output_file, "%c%c", placehoder,
          //         this->params.output_separator);
          output_buffer.push_back(placehoder);
          output_buffer.push_back(this->params.output_separator);
        }
      }
    }
    // change the last separator to newline
    output_buffer.pop_back();
    output_buffer.push_back('\n');
  }
  fwrite(output_buffer.data(), 1, output_buffer.size(), this->output_file);
}

// row mode
void
Processor::flush_all_records_to_file_row_mode() {
  auto output_buffer = this->output_buffer;
  output_buffer.clear();
  for (int i = 0; i < this->records.size(); i++) {
    if (this->records[i]->status() != RecordStatusWaitOutput) {
      continue;
    }
    auto record = this->records[i];
    auto buffer = record->buffer();
    auto nrows = record->get_record_limit();
    auto placehoder = record->get_placehoder();
    auto cut = record->get_cut_columns();
    if (cut.empty()) {
      continue;
    }
    for (int n = 0; n < nrows; n++) {
      auto row = buffer->get_row(n).value_or(nullptr);
      int ncol = cut.size();
      if (this->params.full_mode) {
        ncol = row->size();
      }
      for (int j = 0; j < ncol; j++) {
        int item_index;
        if (this->params.full_mode) {
          item_index = j;
        } else {
          item_index = cut[j];
        }
        auto item = row->get_item(item_index);
        if (item.has_value()) {
          auto item_value = item.value();
          // fprintf(this->output_file, "%s%c", item_value.data(),
          //         this->params.output_separator);
          output_buffer.append(item_value.data());
          output_buffer.push_back(this->params.output_separator);
        } else {
          // fprintf(this->output_file, "%c%c", record->get_placehoder(),
          //         this->params.output_separator);
          output_buffer.push_back(record->get_placehoder());
          output_buffer.push_back(this->params.output_separator);
        }
      }
      // change the last separator to newline
      output_buffer.pop_back();
      output_buffer.push_back('\n');
    }
  }
  fwrite(output_buffer.data(), 1, output_buffer.size(), this->output_file);
}

void
Processor::drop_all_records_and_update_next() {
  for (int i = 0; i < this->records.size(); i++) {
    auto s = this->records[i]->status();
    if (s == RecordStatusWaitOutput || s == RecordStatusNotPassCondition
        || s == RecordStatusUnavailable || s == RecordStatusNotPassExists) {
      this->records[i]->next();
    }
  }
}

RowKey*
get_key(Record* r) {
  RowKey* row_key = nullptr;
  while (1) {
    row_key = r->key().value_or(nullptr);
    if (row_key == nullptr) {
      auto s = r->next();
      if (s == RecordStatusEof) {
        return nullptr;
      }
      assert(s == RecordStatusWaitConsumption);
    } else {
      assert(r->status() == RecordStatusWaitConsumption);
      int all_keys_have_value = 1;
      for (int i = 0; i < row_key->size(); i++) {
        auto key = row_key->get_key(i).value_or(nullptr);
        if (key == nullptr) {
          all_keys_have_value = 0;
          break;
        }
      }
      if (all_keys_have_value) {
        return row_key;
      } else {
        auto s = r->next();
        if (s == RecordStatusEof) {
          return nullptr;
        }
      }
    }
  }
  assert(r->status() == RecordStatusWaitConsumption);
  return row_key;
}

RowKey*
topest_of_2(RowKey* left_key, RowKey* right_key) {
  // if left key is more at the top return left key otherwise return right key
  for (int i = 0; i < left_key->size(); i++) {
    auto left_key_item = left_key->get_key(i).value_or(nullptr);
    auto right_key_item = right_key->get_key(i).value_or(nullptr);
    assert(left_key_item != nullptr);
    assert(right_key_item != nullptr);
    if (left_key_item->equals(right_key_item)) {
      continue;
    } else {
      // 升序
      if (left_key_item->sort_order == RowKeySortOrderAsc) {
        if (left_key_item->small_than(right_key_item)) {
          return left_key;
        } else {
          return right_key;
        }
      } else {
        // 降序
        if (left_key_item->big_than(right_key_item)) {
          return left_key;
        } else {
          return right_key;
        }
      }
    }
  }
  return left_key;
}

RowKey*
the_topest_key(std::vector<Record*>& records, int* ntop) {
  *ntop = 0;
  if (records.size() == 0) {
    return nullptr;
  }
  RowKey* top = nullptr;
  Record* r = nullptr;
  for (int i = 0; i < records.size(); i++) {
    r = records[i];
    if (r->status() != RecordStatusWaitConsumption) {
      continue;
    }
    top = get_key(r);
    if (top != nullptr) {
      break;
    }
  }
  if (top == nullptr) {
    return nullptr;
  }

  if (records.size() == 1) {
    *ntop = 1;
    r->set_status(RecordStatusWaitOutput);
    return top;
  }

  for (int i = 0; i < records.size(); i++) {
    auto r = records[i];
    if (r->status() != RecordStatusWaitConsumption) {
      continue;
    }
    RowKey* other = get_key(r);

    if (other == nullptr) {
      continue;
    }

    top = topest_of_2(top, other);
  }

  int n_top = 0;

  for (int i = 0; i < records.size(); i++) {
    auto r = records[i];
    if (r->status() != RecordStatusWaitConsumption) {
      continue;
    }
    RowKey* other = get_key(r);
    if (other == nullptr) {
      continue;
    }
    if (top->equals(other)) {
      n_top++;
      r->set_status(RecordStatusWaitOutput);
    }
  }
  *ntop = n_top;
  return top;
}

void
Processor::process() {
  if (this->records.size() < this->params.min_count) {
    fprintf(stderr, "The number of records is less than the minimum count\n");
    fflush(stderr);
    return;
  }
  uint32_t ouput_number = 0;
  int stop = false;
  while (!stop) {
    int c = 0;
    auto* topest_keys = the_topest_key(this->records, &c);
    if (topest_keys == nullptr) {
      break;
    }
    bool drop_all = false;
    for (int i = 0; i < this->records.size(); i++) {
      if (this->records[i]->status() != RecordStatusWaitOutput) {
        if (this->records[i]->get_exist() == ExistConditionMust) {
          drop_all = true;
          break;
        }
      } else {
        if (this->records[i]->get_exist() == ExistConditionNot) {
          drop_all = true;
          break;
        }
      }
    }

    if (drop_all) {
      this->drop_all_records_and_update_next();
      continue;
    }

    // check if the number of records is within the range
    if (c < this->params.min_count || c > this->params.max_count) {
      this->drop_all_records_and_update_next();
      continue;
    }
    float fc = 1.0 * c / this->records.size();
    if (fc < this->params.fmin_count || fc > this->params.fmax_count) {
      this->drop_all_records_and_update_next();
      continue;
    }
    if (this->params.row_mode) {
      this->flush_all_records_to_file_row_mode();
    } else {
      this->flush_all_records_to_file();
    }
    ouput_number++;
    if (this->params.output_limit > 0
        && ouput_number >= this->params.output_limit) {
      return;
    }

    for (int i = 0; i < this->records.size(); i++) {
      if (this->records[i]->status() != RecordStatusWaitOutput) {
        continue;
      }
      auto s = this->records[i]->next();
      if (s == RecordStatusEof
          && this->records[i]->get_exist() == ExistConditionMust) {
        stop = true;
        break;
      }
    }
  }
}

} // namespace filterx
