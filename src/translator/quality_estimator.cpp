#include <iostream>
#include "quality_estimator.h"



namespace marian {
namespace bergamot {

QualityEstimator::QualityEstimator(std::string file_parameters) {
  char delimiter = '\n';
  int line_position = 0;
  std::string line;
  std::vector<std::string> file_input;
  file_input.reserve(4);
  std::istringstream istr(file_parameters);
  while (std::getline(istr,line, delimiter)) {
     file_input.emplace_back(line); 
  }
  if (file_input.size()!=4) {
      throw std::length_error("Model file should contains 4 lines, one per model parameter");
  }

  QualityEstimator::initVector(this->stds, file_input[0]);
  QualityEstimator::initVector(this->means, file_input[1]);
  QualityEstimator::initVector(this->coefficients, file_input[2]);
  QualityEstimator::initVector(this->intercept, file_input[3]);
}

void QualityEstimator::initVector(std::vector<float> &emptyVector, std::string line){
    char delimiter=' ';
    size_t start=0;
    size_t end= line.find_first_of(delimiter);
    
    while (end <= std::string::npos) {
        emptyVector.push_back(std::stof(line.substr(start, end-start)));
        if (end == std::string::npos)
	        break;
        start=end+1;
    	end = line.find_first_of(delimiter, start);
    }
}

void QualityEstimator::mapBPEToWords(Response& sentence, Words words){
    //The result is a vector of maps
    // each element of vector is a map containing the bpes of a word
    // each map key is a tuple consisting of the begining and end of the specific subword
    std::vector<std::map<std::tuple<size_t, size_t>, float>> target_words;
    for (int sentenceIdx = 0; sentenceIdx < sentence.size(); sentenceIdx++) {
        auto &quality = sentence.qualityScores[sentenceIdx];
        size_t wordIdx = 0;
        bool first = true;
        std::map<std::tuple<size_t, size_t>, float> map;
        float previous_p;
        for (auto &p : quality.word) {
            if (words[wordIdx].toString()==words[wordIdx].DEFAULT_EOS_ID.toString()){
                break;
            }
            ByteRange subword = sentence.target.wordAsByteRange(sentenceIdx, wordIdx);
            size_t subword_begin = subword.begin;
            size_t subword_end = subword.end;
            char first_word = sentence.target.text[subword_begin];
            wordIdx++;
            if (first){
                first = false;
                previous_p = p;
                map[std::make_tuple(subword_begin, subword_end)] = previous_p;
                continue;
            }
            if (first_word == ' '){
                target_words.push_back(map);
                map.clear();
                map[std::make_tuple(subword_begin+1, subword_end)] = p;
            }
            else{
                map[std::make_tuple(subword_begin, subword_end)] = p;
            }
        }
        //last word
        if (!map.empty()){
            target_words.push_back(map);
        }
    }
    this->mapping = target_words;
}
 intgemm::AlignedVector<QualityEstimator::FeatureVector> QualityEstimator::extractFeatures(Response& sentence){
     auto quality_scores = sentence.qualityScores[0]; //this is expected to be only one
     auto map = this->mapping;
     const intgemm::Index n_rows = map.size();
     const intgemm::Index width = 64;
     intgemm::AlignedVector<QualityEstimator::FeatureVector> feature_matrix(n_rows * width);
     intgemm::Index current_idx = 0;
     for(auto &word: map) {
        bool first=true;
        size_t first_subword_idx;
        size_t last_subword_idx;
        float bpe_scores_sum = 0;
        float bpe_scores_min;
        for (auto &subword:word){
            auto tmp = subword.first;
            float bpe_score = subword.second;
            if (first){
                bpe_scores_min = bpe_score;
                first_subword_idx = std::get<0>(tmp);
                first=false;
            }
            if(bpe_score<bpe_scores_min){
                bpe_scores_min = bpe_score;
            }
            last_subword_idx = std::get<1>(tmp);
            bpe_scores_sum += bpe_score;
        }
        float bpe_mean = bpe_scores_sum/word.size();
        float model_scores = quality_scores.sequence/quality_scores.word.size();
        auto features = QualityEstimator::FeatureVector(bpe_mean,bpe_scores_min,word.size(),model_scores);
        feature_matrix[current_idx] = features;
        current_idx +=1;
        int subword_len = last_subword_idx - first_subword_idx;
        auto final_word = sentence.target.text.substr(first_subword_idx, subword_len);
     }
    // we still need to subtract mean and divide by std
     return feature_matrix;
 }


void QualityEstimator::compute_quality_scores(Response& sentence, Words words) {
    this->mapBPEToWords(sentence, words);
    auto feature_vector = this->extractFeatures(sentence);
}

}
}