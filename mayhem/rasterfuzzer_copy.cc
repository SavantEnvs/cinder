// Upstream rasterfuzzer.cc edits its input buffer in place, which libFuzzer (clang 19) aborts on as
// "overwrites-const-input". This wrapper hands the upstream code a private copy.
#include <stdint.h>
#include <stddef.h>
#include <vector>

extern "C" int RasterFuzzerOneInput(const uint8_t *data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::vector<uint8_t> copy(data, data + size);
  copy.resize(size + 1);
  return RasterFuzzerOneInput(copy.data(), size);
}
