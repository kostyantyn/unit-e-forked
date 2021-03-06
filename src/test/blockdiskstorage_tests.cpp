// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockdb.h>
#include <validation.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>
#include <wallet/test/wallet_test_fixture.h>

BOOST_FIXTURE_TEST_SUITE(blockdiskstorage_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(readblock) {

  auto block_disk_storage = BlockDB::New();

  CBlock block;
  auto current_tip = *chainActive.Tip();
  auto result = block_disk_storage->ReadBlock(current_tip);

  BOOST_CHECK(result);
  BOOST_CHECK(result->GetHash() == current_tip.GetBlockHash());
}

BOOST_AUTO_TEST_SUITE_END()
