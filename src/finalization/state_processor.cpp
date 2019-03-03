#include <finalization/state_processor.h>

#include <esperanza/finalizationstate.h>
#include <finalization/state_repository.h>
#include <snapshot/creator.h>

namespace finalization {
namespace {

class ProcessorImpl final : public StateProcessor {
 public:
  explicit ProcessorImpl(Dependency<finalization::StateRepository> repo)
      : m_repo(repo) {}

  bool ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) override;
  bool ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) override;
  bool ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) override;

 private:
  bool ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block);
  bool FinalizationHappened(const CBlockIndex &block_index, blockchain::Height *out_height);

  Dependency<finalization::StateRepository> m_repo;
};

bool ProcessorImpl::ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block) {
  const auto state = m_repo->FindOrCreate(block_index, FinalizationState::COMPLETED);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "ERROR: Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetInitStatus()) {
    case FinalizationState::NEW: {
      state->ProcessNewTip(block_index, block);
      break;
    }

    case FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s heigh=%d has been processed from commits, confirming...\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      assert(block_index.pprev != nullptr);  // we don't process commits of genesis block
      const auto ancestor_state = m_repo->Find(*block_index.pprev);
      assert(ancestor_state != nullptr);
      FinalizationState new_state(*ancestor_state);
      new_state.ProcessNewTip(block_index, block);
      if (m_repo->Confirm(block_index, std::move(new_state), nullptr)) {
        // UNIT-E TODO: DoS commits sender.
        LogPrint(BCLog::FINALIZATION, "WARN: After processing the block_hash=%s height=%d, its finalization state differs from one given from commits. Overwrite it anyway.\n",
                 block_index.GetBlockHash().GetHex(), block_index.nHeight);
      } else {
        LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d confirmed\n",
                 block_index.GetBlockHash().GetHex(), block_index.nHeight);
      }
      break;
    }

    case FinalizationState::COMPLETED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      break;
    }
  }

  return true;
}

bool ProcessorImpl::FinalizationHappened(const CBlockIndex &block_index, blockchain::Height *out_height) {
  if (block_index.pprev == nullptr) {
    return false;
  }
  const auto *prev_state = m_repo->Find(*block_index.pprev);
  const auto *new_state = m_repo->Find(block_index);
  if (prev_state == nullptr || new_state == nullptr) {
    return false;
  }

  const auto epoch_length = m_repo->GetFinalizationParams().epoch_length;
  // workaround first epoch finalization
  if (static_cast<blockchain::Height>(block_index.nHeight) == epoch_length) {
    if (out_height != nullptr) {
      *out_height = epoch_length - 1;
    }
    return true;
  }

  const auto prev_fin_epoch = prev_state->GetLastFinalizedEpoch();
  const auto new_fin_epoch = new_state->GetLastFinalizedEpoch();
  if (prev_fin_epoch == new_fin_epoch) {
    return false;
  }

  assert(new_fin_epoch > prev_fin_epoch);
  if (out_height != nullptr) {
    *out_height = (new_fin_epoch + 1) * epoch_length - 1;
  }
  return true;
}

bool ProcessorImpl::ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "Process tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  if (!ProcessNewTipWorker(block_index, block)) {
    return false;
  }
  if (block_index.nHeight > 0 && !m_repo->Restoring() &&
      (block_index.nHeight + 2) % m_repo->GetFinalizationParams().epoch_length == 0) {
    // Generate the snapshot for the block which is one block behind the last one.
    // The last epoch block will contain the snapshot hash pointing to this snapshot.
    snapshot::Creator::GenerateOrSkip(m_repo->GetTipState()->GetCurrentEpoch());
  }
  blockchain::Height finalization_height = 0;
  if (FinalizationHappened(block_index, &finalization_height)) {
    m_repo->TrimUntilHeight(finalization_height);
  }
  return true;
}

bool ProcessorImpl::ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "Process candidate tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  return ProcessNewTipWorker(block_index, block);
}

bool ProcessorImpl::ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) {
  LogPrint(BCLog::FINALIZATION, "Process commits block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);

  const auto state = m_repo->FindOrCreate(block_index, FinalizationState::FROM_COMMITS);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "ERROR: Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetInitStatus()) {
    case esperanza::FinalizationState::NEW: {
      state->ProcessNewCommits(block_index, txes);
      break;
    }

    case esperanza::FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed from commits\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      break;
    }

    case esperanza::FinalizationState::COMPLETED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      break;
    }
  }

  return true;
}

}  // namespace

std::unique_ptr<StateProcessor> StateProcessor::New(Dependency<finalization::StateRepository> repo) {
  return MakeUnique<ProcessorImpl>(repo);
}

}  // namespace finalization
