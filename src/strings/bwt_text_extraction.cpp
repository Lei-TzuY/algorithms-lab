#include "algorithms/strings/bwt_index.hpp"

#include <bit>
#include <limits>
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

BwtPeriodicSampleTextExtractor::BwtPeriodicSampleTextExtractor(
    const BwtByteIndex& index)
    : index_(&index) {
  const std::size_t text_size = index.text_size_;
  const std::size_t sample_rate = index.locate_sample_rate_;
  if (sample_rate == 0U) {
    throw std::logic_error("BWT periodic extractor requires positive sample rate");
  }

  const std::size_t regular_sample_count = text_size / sample_rate + 1U;
  const bool has_tail_sample = text_size % sample_rate != 0U;
  if (has_tail_sample &&
      regular_sample_count == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("BWT periodic extractor sample count overflow");
  }
  const std::size_t expected_sample_count =
      regular_sample_count + (has_tail_sample ? 1U : 0U);
  if (index.sampled_positions_.size() != expected_sample_count ||
      index.sampled_rows_.one_count() != expected_sample_count) {
    throw std::logic_error("BWT periodic extractor sample cardinality invariant violated");
  }

  const std::size_t unset = std::numeric_limits<std::size_t>::max();
  sample_rows_by_position_.assign(expected_sample_count, unset);
  for (std::size_t ordinal = 0U; ordinal < expected_sample_count; ++ordinal) {
    const auto row = index.sampled_rows_.select1(ordinal);
    if (!row.has_value() || *row >= index.row_count()) {
      throw std::logic_error("BWT periodic extractor sampled row missing");
    }

    const std::size_t position = index.sampled_positions_[ordinal];
    if (position > text_size) {
      throw std::logic_error("BWT periodic extractor sampled position invalid");
    }

    const bool tail_sample = has_tail_sample && position == text_size;
    if (!tail_sample && position % sample_rate != 0U) {
      throw std::logic_error("BWT periodic extractor non-periodic sample");
    }
    const std::size_t slot =
        tail_sample ? regular_sample_count : position / sample_rate;
    if (slot >= sample_rows_by_position_.size() ||
        sample_rows_by_position_[slot] != unset) {
      throw std::logic_error("BWT periodic extractor duplicate sample slot");
    }
    sample_rows_by_position_[slot] = *row;
  }

  for (const std::size_t row : sample_rows_by_position_) {
    if (row == unset) {
      throw std::logic_error("BWT periodic extractor incomplete sample inverse");
    }
  }

  if (sample_rows_by_position_.size() >
      std::numeric_limits<std::size_t>::max() / sizeof(std::size_t)) {
    throw std::length_error("BWT periodic extractor payload size overflow");
  }
  inverse_sample_payload_bytes_ =
      sample_rows_by_position_.size() * sizeof(std::size_t);
}

std::size_t BwtPeriodicSampleTextExtractor::sample_count() const noexcept {
  return sample_rows_by_position_.size();
}

std::size_t BwtPeriodicSampleTextExtractor::inverse_sample_payload_bytes()
    const noexcept {
  return inverse_sample_payload_bytes_;
}

SampledBwtTextExtractionResult BwtPeriodicSampleTextExtractor::extract(
    std::size_t begin, std::size_t end) const {
  if (index_ == nullptr) {
    throw std::logic_error("BWT periodic extractor has no index");
  }
  const std::size_t text_size = index_->text_size_;
  if (begin > end || end > text_size) {
    throw std::out_of_range("BWT periodic extraction range out of bounds");
  }

  SampledBwtTextExtractionResult result;
  if (begin == end) {
    return result;
  }

  const std::size_t sample_rate = index_->locate_sample_rate_;
  const std::size_t regular_sample_count = text_size / sample_rate + 1U;
  const bool has_tail_sample = text_size % sample_rate != 0U;

  std::size_t anchor_position = end;
  std::size_t sample_slot = 0U;
  if (end == text_size) {
    sample_slot = has_tail_sample ? regular_sample_count
                                  : text_size / sample_rate;
  } else {
    const std::size_t remainder = end % sample_rate;
    if (remainder == 0U) {
      sample_slot = end / sample_rate;
    } else {
      const std::size_t delta = sample_rate - remainder;
      if (delta <= text_size - end) {
        anchor_position = end + delta;
        sample_slot = anchor_position / sample_rate;
      } else {
        anchor_position = text_size;
        if (!has_tail_sample) {
          throw std::logic_error("BWT periodic extractor tail sample missing");
        }
        sample_slot = regular_sample_count;
      }
    }
  }

  if (sample_slot >= sample_rows_by_position_.size()) {
    throw std::logic_error("BWT periodic extractor sample slot out of range");
  }

  result.bytes.resize(end - begin);
  std::size_t current = sample_rows_by_position_[sample_slot];
  std::size_t position = anchor_position;
  while (position > begin) {
    if (current == index_->sentinel_row_) {
      throw std::logic_error("BWT periodic extraction reached sentinel too early");
    }
    const char byte = std::bit_cast<char>(index_->bwt_byte_at_row(current));
    --position;
    if (position < end) {
      result.bytes[position - begin] = byte;
    }
    current = index_->lf(current);
    ++result.lf_steps;
  }

  const std::size_t range_length = end - begin;
  if (result.lf_steps < range_length ||
      result.lf_steps - range_length >= sample_rate) {
    throw std::logic_error("BWT periodic extraction LF-step bound violated");
  }
  return result;
}

}  // namespace algorithms::strings
