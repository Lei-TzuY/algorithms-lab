#include "algorithms/strings/bwt_index.hpp"

#include <bit>
#include <stdexcept>

namespace algorithms::strings {

BwtTextReconstructionResult BwtByteIndex::reconstruct_text() const {
  BwtTextReconstructionResult result;
  result.text.resize(text_size_);

  // Conceptual row zero is the empty suffix at SA=n. For every non-sentinel
  // row with suffix position p>0, the BWT byte is text[p-1] and LF moves to
  // suffix position p-1. Reading before each LF step therefore reconstructs
  // the source bytes from right to left.
  std::size_t current = 0U;
  for (std::size_t remaining = text_size_; remaining > 0U; --remaining) {
    if (current == sentinel_row_) {
      throw std::logic_error("BWT reconstruction reached sentinel too early");
    }
    result.text[remaining - 1U] = std::bit_cast<char>(bwt_byte_at_row(current));
    current = lf(current);
    ++result.lf_steps;
  }

  if (current != sentinel_row_) {
    throw std::logic_error("BWT reconstruction did not end at sentinel");
  }
  return result;
}

BwtTextExtractionResult BwtByteIndex::extract_text(std::size_t begin,
                                                   std::size_t end) const {
  if (begin > end || end > text_size_) {
    throw std::out_of_range("BWT extraction range out of bounds");
  }

  BwtTextReconstructionResult reconstructed = reconstruct_text();
  BwtTextExtractionResult result;
  result.bytes = reconstructed.text.substr(begin, end - begin);
  result.lf_steps = reconstructed.lf_steps;
  result.reconstructed_bytes = reconstructed.text.size();
  return result;
}

}  // namespace algorithms::strings
