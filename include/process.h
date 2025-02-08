#include "record.h"

namespace filterx {

struct ProcessorParams {
  uint32_t min_count;
  uint32_t max_count;
  float fmin_count;
  float fmax_count;
  char output_separator;
  char placehoder;
  char* output_path;
};

class Processor {
public:
  Processor(ProcessorParams& params);
  ~Processor() {
    if (this->output_file == stdout) {
      return;
    }
    if (this->output_file != nullptr) {
      fflush(this->output_file);
      fclose(this->output_file);
      this->output_file = nullptr;
    }
  }

  void add_record(Record* record);
  void prepare();
  void flush_add_records_to_file();
  void flush_root_record_to_file();
  void drop_all_records_and_update_next();

  void process();

private:
  FILE* output_file;
  std::vector<Record*> records;
  ProcessorParams params;
};

} // namespace filterx
