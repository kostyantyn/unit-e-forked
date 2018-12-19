#include <boost/test/unit_test.hpp>
#include <finalization/cache.h>
#include <finalization/p2p.h>
#include <chain.h>
#include <test/test_unite.h>
#include <esperanza/finalizationstate.h>
#include <validation.h>

BOOST_FIXTURE_TEST_SUITE(commits_tests, TestingSetup)

namespace {

CBlockIndex *AddBlock(CBlockIndex *prev) {
  const auto height = prev->nHeight + 1;
  const auto res = mapBlockIndex.emplace(uint256S(std::to_string(height)), new CBlockIndex);
  CBlockIndex &index = *res.first->second;
  index.nHeight = height;
  index.phashBlock = &res.first->first;
  index.pprev = prev;
  chainActive.SetTip(&index);
  assert(finalization::cache::ProcessNewTip(index, CBlock()));
  return &index;
}


void AddBlocks(int number) {
  for (int i = 0; i < number; ++i) {
    AddBlock(chainActive.Tip());
  }
}

uint256 Checkpoint(uint32_t epoch) {
  const auto h = esperanza::GetEpochHeight(epoch + 1) - 1;
  return chainActive[h]->GetBlockHash();
}

}

BOOST_AUTO_TEST_CASE(get_commits_locator) {
  CChain &chain = chainActive;
  const auto epochLength = esperanza::GetEpochLength();
  // test uses small steps between blocks, epoch length must be greater
  BOOST_CHECK(epochLength > 3);

  // Fill chain right before first checkpoint and check `start` has only Genesis
  AddBlocks(epochLength - 2); // -1 for genesis, -1 to be one block before checkpoint
  BOOST_CHECK_EQUAL(esperanza::GetCurrentEpoch(), 0);
  BOOST_CHECK_EQUAL(esperanza::GetLastFinalizedEpoch(), 0);
  BOOST_CHECK_EQUAL(chain.Height(), epochLength - 2);
  {
    const auto locator = finalization::p2p::GetCommitsLocator(nullptr, chain.Tip());
    std::vector<uint256> expected_start = { chain.Genesis()->GetBlockHash() };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.Tip()->GetBlockHash());
  }

  // Check start has Genesis and chain[2]
  {
    const auto locator = finalization::p2p::GetCommitsLocator(chain[2], chain.Tip());
    std::vector<uint256> expected_start = { chain.Genesis()->GetBlockHash(), chain[2]->GetBlockHash() };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.Tip()->GetBlockHash());
  }

  // Add one more block, we're still in 0th epoch, but we have a checkpoint
  AddBlocks(1);
  BOOST_CHECK_EQUAL(esperanza::GetCurrentEpoch(), 0);
  BOOST_CHECK_EQUAL(esperanza::GetLastFinalizedEpoch(), 0);
  BOOST_CHECK_EQUAL(chain.Height(), epochLength - 1);
  {
    const auto locator = finalization::p2p::GetCommitsLocator(nullptr, chain.Tip());
    std::vector<uint256> expected_start = { chain.Genesis()->GetBlockHash(), Checkpoint(0) };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.Tip()->GetBlockHash());
  }

  // Now we're in the second epoch
  AddBlocks(1);
  BOOST_CHECK_EQUAL(esperanza::GetCurrentEpoch(), 1);
  BOOST_CHECK_EQUAL(esperanza::GetLastFinalizedEpoch(), 0);
  BOOST_CHECK_EQUAL(chain.Height(), epochLength);
  {
    const auto locator = finalization::p2p::GetCommitsLocator(nullptr, chain.Tip());
    std::vector<uint256> expected_start = { chain.Genesis()->GetBlockHash(), Checkpoint(0) };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.Tip()->GetBlockHash());
  }

  // Generate one more epoch, finalization moved, new checkpoint included
  AddBlocks(epochLength);
  BOOST_CHECK_EQUAL(esperanza::GetCurrentEpoch(), 2);
  BOOST_CHECK_EQUAL(esperanza::GetLastFinalizedEpoch(), 1);
  BOOST_CHECK_EQUAL(chain.Height(), epochLength * 2);
  {
    const auto locator = finalization::p2p::GetCommitsLocator(nullptr, chain.Tip());
    std::vector<uint256> expected_start = { Checkpoint(1) };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.Tip()->GetBlockHash());
  }
  // chain[2] is behind last finalized checkpoint, skip it
  {
    const auto locator = finalization::p2p::GetCommitsLocator(chain[2], chain.Tip());
    std::vector<uint256> expected_start = { Checkpoint(1) };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.Tip()->GetBlockHash());
  }
  // Checkpoint from epoch 1 isn't included twice
  {
    const auto locator = finalization::p2p::GetCommitsLocator(chain[99], chain.Tip());
    std::vector<uint256> expected_start = { Checkpoint(1) };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.Tip()->GetBlockHash());
  }
}

BOOST_AUTO_TEST_SUITE_END()
