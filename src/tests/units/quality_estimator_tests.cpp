#include "quality_estimator_tests.h"

#include "catch.hpp"
#include "translator/quality_estimator.h"

using namespace marian::bergamot;

SCENARIO("Unsupervised Quality Estimator test", "[QualityEstimator]") {
  GIVEN("A quality, features and target") {
    // AnnotatedText Target
    std::string input = "This is an example.";
    marian::string_view prefix(input.data(), 0);

    std::string target = "- Este es un ejemplo.";

    std::vector<marian::string_view> sentencesView = {
        marian::string_view(target.data(), 1),       // "-"
        marian::string_view(target.data() + 1, 5),   // " Este"
        marian::string_view(target.data() + 6, 3),   // "  es"
        marian::string_view(target.data() + 9, 3),   // " un"
        marian::string_view(target.data() + 12, 8),  // " ejemplo",
        marian::string_view(target.data() + 20, 1),  // ".",
        marian::string_view(target.data() + 21, 0),  // "",
    };

    Response response;

    response.target.appendSentence(prefix, sentencesView.begin(), sentencesView.end());

    // Histories - LogProbs

    const std::vector<float> logProbs = {-0.3, -0.0001, -0.002, -0.5, -0.2, -0.1, -0.001};

    auto histories = logProbsToHistories(logProbs);

    AND_GIVEN("Quality Estimator without aligned memory") {
      auto qualityEstimator = createQualityEstimator({});

      WHEN("It's call computeQualityScores") {
        qualityEstimator->computeQualityScores(histories, response);

        THEN("It's returned WordsQualityEstimate") {
          REQUIRE(response.qualityScores.size() == 1);

          const auto& sentenceQualityEstimate = response.qualityScores[0];

          CHECK(sentenceQualityEstimate.wordByteRanges ==
                std::vector<ByteRange>({{0, 1}, {2, 6}, {7, 9}, {10, 12}, {13, 21}}));

          CHECK(sentenceQualityEstimate.wordScores == std::vector<float>{-0.3, -0.0001, -0.002, -0.5, -0.15});
          CHECK(sentenceQualityEstimate.sentenceScore == Approx(-0.19042f).epsilon(0.0001));
        }
      }
    }
  }
}

SCENARIO("Logistic Regressor Test", "[QualityEstimator]") {
  GIVEN("A quality, features and target") {
    // AnnotatedText Target
    std::string input = "This is an example.";
    marian::string_view prefix(input.data(), 0);

    std::string target = "- Este es un ejemplo.";

    std::vector<marian::string_view> sentencesView = {
        marian::string_view(target.data(), 1),       // "-"
        marian::string_view(target.data() + 1, 5),   // " Este"
        marian::string_view(target.data() + 6, 3),   // "  es"
        marian::string_view(target.data() + 9, 3),   // " un"
        marian::string_view(target.data() + 12, 8),  // " ejemplo",
        marian::string_view(target.data() + 20, 1),  // ".",
        marian::string_view(target.data() + 21, 0),  // "",
    };

    Response response;

    response.target.appendSentence(prefix, sentencesView.begin(), sentencesView.end());

    // Histories - LogProbs

    const std::vector<float> logProbs = {-0.3, -0.0001, -0.002, -0.5, -0.2, -0.1, -0.001};

    auto histories = logProbsToHistories(logProbs);

    // Memory / Features

    LogisticRegressorQualityEstimator::Scale scale;
    scale.stds = {0.200000003, 0.300000012, 2.5, 0.100000001};
    scale.means = {-0.100000001, -0.769999981, 5, -0.5};

    LogisticRegressorQualityEstimator::Array coefficients = {0.99000001, 0.899999976, -0.200000003, 0.5};
    const float intercept = {-0.300000012};

    AND_GIVEN("QualityEstimator with LogisticRegressor Aligned Memory") {
      auto qualityEstimator = createQualityEstimator(
          LogisticRegressorQualityEstimator(std::move(scale), std::move(coefficients), intercept).toAlignedMemory());
      WHEN("It's call computeQualityScores") {
        qualityEstimator->computeQualityScores(histories, response);

        THEN("It's add WordsQualityEstimate on response") {
          REQUIRE(response.qualityScores.size() == 1);

          const auto& sentenceQualityEstimate = response.qualityScores[0];

          CHECK(sentenceQualityEstimate.wordByteRanges ==
                std::vector<ByteRange>({{0, 1}, {2, 6}, {7, 9}, {10, 12}, {13, 21}}));

          CHECK(sentenceQualityEstimate.wordScores ==
                std::vector<float>({-2.14596, -4.41793, -4.403, -0.93204, -3.03343}));
          CHECK(sentenceQualityEstimate.sentenceScore == Approx(-2.98647).epsilon(0.0001));
        }
      }
    }
  }
}

bool operator==(const std::vector<float>& value1, const std::vector<float>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(), [](const auto& a, const auto& b) {
    auto value = Approx(b).epsilon(0.001);
    return a == value;
  });
}

bool operator==(const marian::bergamot::ByteRange& value1, const marian::bergamot::ByteRange& value2) {
  return (value1.begin == value2.begin) && (value1.end == value2.end);
}

bool operator==(const std::vector<marian::bergamot::ByteRange>& value1,
                const std::vector<marian::bergamot::ByteRange>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(),
                    [](const auto& a, const auto& b) { return (a.begin == b.begin) && (a.end == b.end); });
}

std::ostream& operator<<(std::ostream& os, const marian::bergamot::ByteRange& value) {
  os << "{begin: " << value.begin << ", end: " << value.end << "} ";
  return os;
}

marian::Histories logProbsToHistories(const std::vector<float>& logProbs) {
  marian::Word word;

  auto topHyp = marian::Hypothesis::New();
  float prevLogProb = 0.0;

  for (const auto& logProb : logProbs) {
    topHyp = std::move(marian::Hypothesis::New(std::move(topHyp), word, 0, logProb + prevLogProb));
    prevLogProb += logProb;
  }

  auto history = std::make_shared<marian::History>(1, 0.0);

  history->add(marian::Beam{topHyp}, word, false);

  return {history};
}
