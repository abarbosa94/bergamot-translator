// This file describes the base class ServiceBase, and a non-threaded subclass
// implementing translation functionality called NonThreadedService.

#ifndef SRC_BERGAMOT_SERVICE_BASE_H_
#define SRC_BERGAMOT_SERVICE_BASE_H_
#include "batch_translator.h"
#include "batcher.h"
#include "data/types.h"
#include "response.h"
#include "text_processor.h"

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {
/// ServiceBase abstracts the common elements in an implementation of a
/// Translation Service. There are two variants of implementation present - An
/// implementation running multithreaded and one running single-threaded.

class ServiceBase {
public:
  explicit ServiceBase(Ptr<Options> options);

  /// Transfers ownership of input string to this class, returns a future
  /// containing an object which provides access to translations, other features
  /// like sentencemappings and (tentatively) alignments.
  std::future<Response> translate(std::string &&input);

  /// Convenience accessor methods to extract these vocabulary outside service.
  /// e.g: For use in decoding histories for marian-decoder replacement.
  Ptr<Vocab const> sourceVocab() const { return vocabs_.front(); }
  Ptr<Vocab const> targetVocab() const { return vocabs_.back(); }

protected:
  // Enqueue queues a request for translation, this can be synchronous, blocking
  // or asynchronous and queued in the background.
  virtual void enqueue() = 0;

  /// Assigned in the order of incoming Request-s as they arrive by Service.
  /// Further used to tie-break priority based on arrival-order. Although not
  /// used at the moment, can also be used to identify a unique Request.
  size_t requestId_;

  /// Vocabs to encode and decode in the source and target languages.
  std::vector<Ptr<Vocab const>> vocabs_; // ORDER DEPENDENCY

  /// Text processor is a combination of a sentence-splitter and tokenizer to
  /// operate on a blob of input-text.
  TextProcessor text_processor_; // ORDER DEPENDENCY
  Batcher batcher_;
};

class NonThreadedService : public ServiceBase {
public:
  explicit NonThreadedService(Ptr<Options> options);

private:
  // NonThreaded service overrides unimplemented functions in base-class using
  // blocking mechanisms.
  void enqueue() override;
  // There's a single translator, launched as part of the main process.
  BatchTranslator translator_;
};

// Used across Services
inline std::vector<Ptr<const Vocab>> loadVocabularies(Ptr<Options> options) {
  // @TODO: parallelize vocab loading for faster startup
  auto vfiles = options->get<std::vector<std::string>>("vocabs");
  // with the current setup, we need at least two vocabs: src and trg
  ABORT_IF(vfiles.size() < 2, "Insufficient number of vocabularies.");
  std::vector<Ptr<Vocab const>> vocabs(vfiles.size());
  std::unordered_map<std::string, Ptr<Vocab>> vmap;
  for (size_t i = 0; i < vocabs.size(); ++i) {
    auto m = vmap.emplace(std::make_pair(vfiles[i], Ptr<Vocab>()));
    if (m.second) { // new: load the vocab
      m.first->second = New<Vocab>(options, i);
      m.first->second->load(vfiles[i]);
    }
    vocabs[i] = m.first->second;
  }
  return vocabs;
}

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SERVICE_BASE_H_
