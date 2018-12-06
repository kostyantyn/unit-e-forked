// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/messages.h>

#include <test/test_unite.h>
#include <utilstrencodings.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_messages_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(snapshot_utxo_set_serializer) {
  CDataStream s(SER_NETWORK, INIT_PROTO_VERSION);
  auto subset = snapshot::UTXOSubset();
  s << subset;

  std::string exp;
  exp =
      "00000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "00000000"                          // height
      "00"                                // isCoinBase
      "00";                               // outputs
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();

  subset.tx_id.SetHex("aa");
  subset.height = 0xbb;
  subset.is_coin_base = true;
  s << subset;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "00";                               // outputs
  BOOST_CHECK_MESSAGE(HexStr(s) == exp,
                      "expected: " << HexStr(s) << " got: " << exp);
  s.clear();

  subset.outputs[2] = CTxOut();
  s << subset;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // outputs count
      "02000000"                          // outpoint index
      "ffffffffffffffff"                  // nValue (-1 by default)
      "00";                               // scriptPubKey length
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();

  auto out = CTxOut();
  out.nValue = 0xcc;
  out.scriptPubKey << OP_RETURN;
  subset.outputs[2] = out;
  s << subset;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // outputs count
      "02000000"                          // outpoint index
      "cc00000000000000"                  // nValue (-1 by default)
      "01"                                // scriptPubKey length
      "6a";                               // script data
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();
}

BOOST_AUTO_TEST_CASE(snapshot_get_snapshot_serialization) {
  snapshot::GetSnapshot msg;
  msg.best_block_hash.SetHex("bb");
  msg.utxo_subset_index = 55;
  msg.utxo_subset_count = 17;

  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_MESSAGE(stream.size() == 42,
                      "size expected: " << 42 << " got: " << stream.size());

  std::string got = HexStr(stream);
  std::string exp =
      "bb000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "3700000000000000"                  // index
      "1100";                             // length
  BOOST_CHECK_MESSAGE(got == exp, "expected: " << exp << " got: " << got);

  snapshot::GetSnapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.best_block_hash, msg2.best_block_hash);
  BOOST_CHECK_EQUAL(msg.utxo_subset_index, msg2.utxo_subset_index);
  BOOST_CHECK_EQUAL(msg.utxo_subset_count, msg2.utxo_subset_count);
}

BOOST_AUTO_TEST_CASE(snapshot_snapshot_serialization) {
  // serialize empty message
  snapshot::Snapshot msg;
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 113);

  std::string got = HexStr(stream);
  std::string exp =
      "00000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "00000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "00000000000000000000000000000000"  // stake modifier
      "00000000000000000000000000000000"  //
      "0000000000000000"                  // total utxo sets
      "0000000000000000"                  // utxo set index
      "00";                               // utxo set count
  BOOST_CHECK_EQUAL(got, exp);

  // serialize filled
  msg.snapshot_hash.SetHex("aa");
  msg.best_block_hash.SetHex("bb");
  msg.stake_modifier.SetHex("cc");
  msg.total_utxo_subsets = 25000000;
  msg.utxo_subset_index = 128;

  snapshot::UTXOSubset subset;
  subset.height = 53;
  subset.is_coin_base = true;
  subset.tx_id.SetHex("bb");
  CScript script;
  script << OP_RETURN;
  subset.outputs[5] = CTxOut(5, script);
  msg.utxo_subsets.emplace_back(subset);

  stream.clear();
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 165);

  got = HexStr(stream);
  exp =
      "aa000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "bb000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "cc000000000000000000000000000000"  // stake modifier
      "00000000000000000000000000000000"  //
      "40787d0100000000"                  // total utxo sets
      "8000000000000000"                  // index
      "01"                                // utxo set count
      "bb000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "35000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // output size
      "05000000"                          // output index
      "0500000000000000"                  // amount
      "01"                                // script size
      "6a";                               // script
  BOOST_CHECK_EQUAL(got, exp);

  snapshot::Snapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.best_block_hash, msg2.best_block_hash);
  BOOST_CHECK_EQUAL(msg.stake_modifier, msg2.stake_modifier);
  BOOST_CHECK_EQUAL(msg.total_utxo_subsets, msg2.total_utxo_subsets);
  BOOST_CHECK_EQUAL(msg.utxo_subset_index, msg2.utxo_subset_index);
  BOOST_CHECK_EQUAL(msg.utxo_subsets.size(), msg2.utxo_subsets.size());
  BOOST_CHECK_EQUAL(msg.utxo_subsets[0].tx_id, msg2.utxo_subsets[0].tx_id);
  BOOST_CHECK_EQUAL(msg.utxo_subsets[0].outputs.size(),
                    msg2.utxo_subsets[0].outputs.size());
}

BOOST_AUTO_TEST_CASE(utxo_construct) {
  COutPoint out;
  Coin coin;
  snapshot::UTXO utxo1(out, coin);
  BOOST_CHECK_EQUAL(utxo1.out_point.hash, out.hash);
  BOOST_CHECK_EQUAL(utxo1.out_point.n, out.n);
  BOOST_CHECK_EQUAL(utxo1.height, coin.nHeight);
  BOOST_CHECK_EQUAL(utxo1.is_coin_base, coin.IsCoinBase());
  BOOST_CHECK(utxo1.tx_out == coin.out);

  out.hash.SetHex("aa");
  out.n = 10;
  coin.nHeight = 250;
  coin.fCoinBase = 1;
  coin.out.nValue = 35;
  coin.out.scriptPubKey << OP_RETURN;

  snapshot::UTXO utxo2(out, coin);
  BOOST_CHECK_EQUAL(utxo2.out_point.hash, out.hash);
  BOOST_CHECK_EQUAL(utxo2.out_point.n, out.n);
  BOOST_CHECK_EQUAL(utxo2.height, coin.nHeight);
  BOOST_CHECK_EQUAL(utxo2.is_coin_base, coin.IsCoinBase());
  BOOST_CHECK(utxo2.tx_out == coin.out);
}

BOOST_AUTO_TEST_CASE(utxo_serialization) {
  snapshot::UTXO utxo1;
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo1;
  BOOST_CHECK_EQUAL(stream.size(), 50);

  std::string got = HexStr(stream);
  std::string exp =
      "00000000000000000000000000000000"  // txId
      "00000000000000000000000000000000"  //
      "ffffffff"                          // txOutIndex
      "00000000"                          // height
      "00"                                // isCoinBase
      "ffffffffffffffff"                  // CAmount(-1)
      "00";                               // script length
  BOOST_CHECK_EQUAL(got, exp);
  stream.clear();

  COutPoint out;
  out.hash.SetHex("aa");
  out.n = 10;
  Coin coin;
  coin.nHeight = 250;
  coin.fCoinBase = 1;
  coin.out.nValue = 35;
  coin.out.scriptPubKey << OP_RETURN;

  snapshot::UTXO utxo2(out, coin);
  stream << utxo2;
  BOOST_CHECK_EQUAL(stream.size(), 51);

  got = HexStr(stream);
  exp =
      "aa000000000000000000000000000000"  // txId
      "00000000000000000000000000000000"  //
      "0a000000"                          // txOutIndex
      "fa000000"                          // height
      "01"                                // isCoinBase
      "2300000000000000"                  // CAmount
      "01"                                // script length
      "6a";                               // script
  BOOST_CHECK_EQUAL(got, exp);
  stream.clear();
}

BOOST_AUTO_TEST_CASE(snapshot_hash) {
  // expected results are hardcoded to guarantee that hashes
  // didn't change over time
  uint256 stakeModifier;
  snapshot::SnapshotHash h;
  std::string nullHash = h.GetHash(stakeModifier).GetHex();

  snapshot::UTXO a;
  a.out_point.hash.SetHex("aa");
  snapshot::UTXO b;
  b.out_point.hash.SetHex("bb");
  snapshot::UTXO c;
  c.out_point.hash.SetHex("cc");

  std::string aHash =
      "cf51b4881eef3133d8869fbff8754fa2"
      "40432e82f684acbd044922305d60f22b";
  std::string bHash =
      "556d2f4b047588d55bcbcb95f0527acf"
      "23de3a23cf2266878f1310948151d1fb";
  std::string abSumHash =
      "e831ef80a29eb00be2f3cc90cc21f3fa"
      "cb8d02ae111c1ab1f918c77c56c897d2";

  {
    // hashing twice produces the same result
    snapshot::SnapshotHash hash;
    hash.AddUTXO(a);
    hash.AddUTXO(b);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), abSumHash);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), abSumHash);

    // changing stake modifier produces different hash
    uint256 newSM;
    newSM.SetHex("bb");
    BOOST_CHECK(hash.GetHash(newSM).GetHex() != abSumHash);

    // changing stake modifier doesn't change underline UTXO data
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), abSumHash);
  }

  {
    // test adding and reverting UTXOs
    // null == a + b - b - a
    snapshot::SnapshotHash hash;
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), nullHash);
    hash.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), aHash);
    hash.AddUTXO(b);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), abSumHash);
    hash.SubtractUTXO(b);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), aHash);
    hash.SubtractUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), nullHash);
  }

  {
    // test that order doesn't matter
    // a + b == b + a
    snapshot::SnapshotHash hash1;
    snapshot::SnapshotHash hash2;
    hash1.AddUTXO(a);
    hash1.AddUTXO(b);
    hash2.AddUTXO(b);
    hash2.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash1.GetHash(stakeModifier).GetHex(), abSumHash);
    BOOST_CHECK_EQUAL(hash2.GetHash(stakeModifier).GetHex(), abSumHash);
  }

  {
    // that subtraction
    // b = a + b + c - a - c
    snapshot::SnapshotHash hash1;
    hash1.AddUTXO(a);
    hash1.AddUTXO(b);
    hash1.AddUTXO(c);
    hash1.SubtractUTXO(a);
    hash1.SubtractUTXO(c);

    snapshot::SnapshotHash hash2;
    hash2.AddUTXO(b);

    BOOST_CHECK_EQUAL(hash1.GetHash(stakeModifier).GetHex(), bHash);
    BOOST_CHECK_EQUAL(hash2.GetHash(stakeModifier).GetHex(), bHash);
  }

  {
    // negative case
    // null = -a + a
    // a = -a + a + a
    snapshot::SnapshotHash hash;
    hash.SubtractUTXO(a);
    BOOST_CHECK(hash.GetHash(stakeModifier).GetHex() != nullHash);
    hash.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), nullHash);
    hash.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stakeModifier).GetHex(), aHash);
  }

  {
    // restore snapshotHash from disk

    snapshot::SnapshotHash hash1;
    hash1.AddUTXO(a);
    hash1.AddUTXO(b);

    // simulate reading snapshot data from disk
    snapshot::SnapshotHash hash2(hash1.GetData());

    BOOST_CHECK_EQUAL(hash1.GetHash(stakeModifier).GetHex(), hash2.GetHash(stakeModifier).GetHex());
    hash1.AddUTXO(c);
    hash1.SubtractUTXO(a);
    hash2.AddUTXO(c);
    hash2.SubtractUTXO(a);
    BOOST_CHECK_EQUAL(hash1.GetHash(stakeModifier).GetHex(), hash2.GetHash(stakeModifier).GetHex());
  }
}

BOOST_AUTO_TEST_SUITE_END()