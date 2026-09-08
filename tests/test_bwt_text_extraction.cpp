#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <string>

namespace {

std::string make_extraction_bytes(std::initializer_list<unsigned int> values) {
  std::string result;
  result.reserve(values.size());
  for (const unsigned int value : values) {
    REQUIRE(value <= 255U);
    result.push_back(
        static_cast<char>(static_cast<unsigned char>(value)));
  }
  return result;
}

std::string random_extraction_bytes(std::mt19937_64& rng,
                                    std::size_t length) {
  std::uniform_int_distribution<unsigned int> byte_dist(0U, 255U);
  std::string result(length, '\0');
  for (char& byte : result) {
    byte = static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
  }
  return result;
}

template <typename Function>
bool throws_out_of_range(Function&& function) {
  try {
    function();
  } catch (const std::out_of_range&) {
    return true;
  } catch (...) {
  }
  return false;
}

void require_reconstruction(const algorithms::strings::BwtByteIndex& index,
                            const std::string& text) {
  const auto first = index.reconstruct_text();
  const auto second = index.reconstruct_text();
  REQUIRE_EQ(first.text, text);
  REQUIRE_EQ(second.text, text);
  REQUIRE_EQ(first.lf_steps, text.size());
  REQUIRE_EQ(second.lf_steps, text.size());
}

std::size_t expected_periodic_sample_count(std::size_t text_size,
                                           std::size_t sample_rate) {
  return text_size / sample_rate + 1U +
         (text_size % sample_rate == 0U ? 0U : 1U);
}

void require_sampled_extraction(
    const algorithms::strings::BwtPeriodicSampleTextExtractor& extractor,
    const algorithms::strings::BwtByteIndex& index, const std::string& text,
    std::size_t begin, std::size_t end) {
  const auto extracted = extractor.extract(begin, end);
  REQUIRE_EQ(extracted.bytes, text.substr(begin, end - begin));
  if (begin == end) {
    REQUIRE_EQ(extracted.lf_steps, std::size_t{0});
    return;
  }

  const std::size_t range_length = end - begin;
  REQUIRE(extracted.lf_steps >= range_length);
  REQUIRE(extracted.lf_steps - range_length < index.locate_sample_rate());
}

}  // namespace

TEST_CASE(bwt_text_reconstruction_empty_classic_and_ranges) {
  const algorithms::strings::BwtByteIndex empty{std::string_view{}};
  const auto empty_reconstructed = empty.reconstruct_text();
  REQUIRE(empty_reconstructed.text.empty());
  REQUIRE_EQ(empty_reconstructed.lf_steps, std::size_t{0});
  const auto empty_slice = empty.extract_text(0U, 0U);
  REQUIRE(empty_slice.bytes.empty());
  REQUIRE_EQ(empty_slice.lf_steps, std::size_t{0});
  REQUIRE_EQ(empty_slice.reconstructed_bytes, std::size_t{0});

  const std::string text = "banana";
  const algorithms::strings::BwtByteIndex index{text};
  require_reconstruction(index, text);

  const auto middle = index.extract_text(1U, 5U);
  REQUIRE_EQ(middle.bytes, std::string{"anan"});
  REQUIRE_EQ(middle.lf_steps, text.size());
  REQUIRE_EQ(middle.reconstructed_bytes, text.size());
  REQUIRE_EQ(index.extract_text(0U, text.size()).bytes, text);
  REQUIRE(index.extract_text(0U, 0U).bytes.empty());
  REQUIRE(index.extract_text(text.size(), text.size()).bytes.empty());
  REQUIRE(throws_out_of_range(
      [&] { static_cast<void>(index.extract_text(4U, 3U)); }));
  REQUIRE(throws_out_of_range([&] {
    static_cast<void>(index.extract_text(0U, text.size() + 1U));
  }));
}

TEST_CASE(bwt_text_reconstruction_arbitrary_bytes_and_sample_rate_independence) {
  const std::string text =
      make_extraction_bytes({0U, 255U, 128U, 1U, 0U, 254U, 127U});
  for (const std::size_t rate : {1U, 2U, 3U, 32U, 100U}) {
    const algorithms::strings::BwtByteIndex index{text, rate};
    require_reconstruction(index, text);
    const auto slice = index.extract_text(1U, 6U);
    REQUIRE_EQ(slice.bytes, text.substr(1U, 5U));
    REQUIRE_EQ(slice.lf_steps, text.size());
    REQUIRE_EQ(slice.reconstructed_bytes, text.size());
  }

  std::string all_bytes;
  all_bytes.reserve(256U);
  for (unsigned int value = 0U; value <= 255U; ++value) {
    all_bytes.push_back(
        static_cast<char>(static_cast<unsigned char>(value)));
  }
  const algorithms::strings::BwtByteIndex all_index{all_bytes, 17U};
  require_reconstruction(all_index, all_bytes);
  REQUIRE_EQ(all_index.extract_text(64U, 192U).bytes,
             all_bytes.substr(64U, 128U));
}

TEST_CASE(bwt_text_reconstruction_randomized_differential) {
  std::mt19937_64 rng(0x29B17EAC7ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 128U);
  std::uniform_int_distribution<std::size_t> rate_dist(1U, 96U);

  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::string text = random_extraction_bytes(rng, text_length_dist(rng));
    const algorithms::strings::BwtByteIndex index{text, rate_dist(rng)};
    require_reconstruction(index, text);

    for (std::size_t query = 0U; query < 40U; ++query) {
      std::uniform_int_distribution<std::size_t> begin_dist(0U, text.size());
      const std::size_t begin = begin_dist(rng);
      std::uniform_int_distribution<std::size_t> end_dist(begin, text.size());
      const std::size_t end = end_dist(rng);
      const auto extracted = index.extract_text(begin, end);
      REQUIRE_EQ(extracted.bytes, text.substr(begin, end - begin));
      REQUIRE_EQ(extracted.lf_steps, text.size());
      REQUIRE_EQ(extracted.reconstructed_bytes, text.size());
    }
  }
}

TEST_CASE(bwt_periodic_sample_extraction_classic_bounds_and_payload) {
  const std::string text = "banana";
  const algorithms::strings::BwtByteIndex index{text, 3U};
  const algorithms::strings::BwtPeriodicSampleTextExtractor extractor{index};

  REQUIRE_EQ(extractor.sample_count(), std::size_t{3});
  REQUIRE_EQ(extractor.sample_count(), index.sampled_row_count());
  REQUIRE_EQ(extractor.inverse_sample_payload_bytes(),
             extractor.sample_count() * sizeof(std::size_t));

  require_sampled_extraction(extractor, index, text, 0U, text.size());
  require_sampled_extraction(extractor, index, text, 1U, 5U);
  const auto short_range = extractor.extract(4U, 5U);
  REQUIRE_EQ(short_range.bytes, std::string{"n"});
  REQUIRE_EQ(short_range.lf_steps, std::size_t{2});
  REQUIRE(short_range.lf_steps < index.extract_text(4U, 5U).lf_steps);

  REQUIRE(extractor.extract(0U, 0U).bytes.empty());
  REQUIRE(extractor.extract(text.size(), text.size()).bytes.empty());
  REQUIRE(throws_out_of_range(
      [&] { static_cast<void>(extractor.extract(5U, 4U)); }));
  REQUIRE(throws_out_of_range([&] {
    static_cast<void>(extractor.extract(0U, text.size() + 1U));
  }));

  const algorithms::strings::BwtByteIndex rate_one_index{text, 1U};
  const algorithms::strings::BwtPeriodicSampleTextExtractor rate_one{
      rate_one_index};
  const auto rate_one_range = rate_one.extract(1U, 5U);
  REQUIRE_EQ(rate_one_range.bytes, std::string{"anan"});
  REQUIRE_EQ(rate_one_range.lf_steps, std::size_t{4});
}

TEST_CASE(bwt_periodic_sample_extraction_tail_and_arbitrary_bytes) {
  const std::string text =
      make_extraction_bytes({0U, 255U, 128U, 1U, 0U, 254U, 127U});
  for (const std::size_t rate : {1U, 2U, 3U, 4U, 32U, 100U}) {
    const algorithms::strings::BwtByteIndex index{text, rate};
    const algorithms::strings::BwtPeriodicSampleTextExtractor extractor{index};
    REQUIRE_EQ(extractor.sample_count(),
               expected_periodic_sample_count(text.size(), rate));
    REQUIRE_EQ(extractor.sample_count(), index.sampled_row_count());
    require_sampled_extraction(extractor, index, text, 0U, text.size());
    require_sampled_extraction(extractor, index, text, 1U, 6U);
    require_sampled_extraction(extractor, index, text, 5U, 7U);
  }

  const algorithms::strings::BwtByteIndex empty{std::string_view{}, 7U};
  const algorithms::strings::BwtPeriodicSampleTextExtractor empty_extractor{
      empty};
  REQUIRE_EQ(empty_extractor.sample_count(), std::size_t{1});
  REQUIRE_EQ(empty_extractor.inverse_sample_payload_bytes(),
             sizeof(std::size_t));
  REQUIRE(empty_extractor.extract(0U, 0U).bytes.empty());
}

TEST_CASE(bwt_periodic_sample_extraction_randomized_differential) {
  std::mt19937_64 rng(0x31B17EAC7ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 160U);
  std::uniform_int_distribution<std::size_t> rate_dist(1U, 96U);

  for (std::size_t trial = 0U; trial < 300U; ++trial) {
    const std::string text = random_extraction_bytes(rng, text_length_dist(rng));
    const std::size_t rate = rate_dist(rng);
    const algorithms::strings::BwtByteIndex index{text, rate};
    const algorithms::strings::BwtPeriodicSampleTextExtractor extractor{index};

    REQUIRE_EQ(extractor.sample_count(),
               expected_periodic_sample_count(text.size(), rate));
    REQUIRE_EQ(extractor.sample_count(), index.sampled_row_count());
    REQUIRE_EQ(extractor.inverse_sample_payload_bytes(),
               extractor.sample_count() * sizeof(std::size_t));

    for (std::size_t query = 0U; query < 30U; ++query) {
      std::uniform_int_distribution<std::size_t> begin_dist(0U, text.size());
      const std::size_t begin = begin_dist(rng);
      std::uniform_int_distribution<std::size_t> end_dist(begin, text.size());
      const std::size_t end = end_dist(rng);
      require_sampled_extraction(extractor, index, text, begin, end);
    }
  }
}
