#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test fork choice rule between parallel justification forks

The following checks are performed:
1. node re-orgs to the longer justified parallel fork
2. node re-orgs to the previous fork that became longer justified one
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    connect_nodes_bi,
    connect_nodes,
    disconnect_nodes,
    assert_equal,
    sync_blocks,
    wait_until,
)
from test_framework.messages import (
    CTransaction,
    FromHex,
)
from test_framework.admin import Admin


class ForkChoiceParallelJustificationsTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1'],
            ['-proposing=0', esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def create_justification(node, finalizer, after_blocks):
            node.generatetoaddress(after_blocks - 1, node.getnewaddress())
            connect_nodes(node, finalizer.index)
            sync_blocks([node, finalizer])

            wait_until(lambda: len(node.getrawmempool()) > 0, timeout=150)
            node.generatetoaddress(1, node.getnewaddress())
            assert_equal(len(node.getrawmempool()), 0)
            disconnect_nodes(node, finalizer.index)

        # Two validators (but actually having the same key) produce parallel justifications.
        # node must always follow the longest justified fork
        # validator1 ----> fork1
        #               /
        #           node
        #               \
        # validator2 ----> fork2
        node = self.nodes[0]
        fork1 = self.nodes[1]
        fork2 = self.nodes[2]
        validator1 = self.nodes[3]
        validator2 = self.nodes[4]

        node.importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        validator1.importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')
        validator2.importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')

        # connect node to every peer for fast propagation of admin transactions
        connect_nodes_bi(self.nodes, node.index, fork1.index)
        connect_nodes_bi(self.nodes, node.index, fork2.index)
        connect_nodes_bi(self.nodes, node.index, validator1.index)
        connect_nodes_bi(self.nodes, node.index, validator2.index)

        # leave IBD
        node.generatetoaddress(1, node.getnewaddress())
        sync_blocks([node, fork1, fork2, validator1, validator2])
        Admin.authorize_and_disable(self, node)

        # check that validators are synced after disabling the admin
        wait_until(lambda: validator1.getblockcount() == 2)
        wait_until(lambda: validator2.getblockcount() == 2)
        disconnect_nodes(node, validator2.index)

        payto = validator1.getnewaddress('', 'legacy')
        txid1 = validator1.deposit(payto, 10000)
        validator2.setaccount(payto, '')
        txid2 = validator2.deposit(payto, 10000)
        if txid1 != txid2:  # improve log message
            tx1 = FromHex(CTransaction(), validator1.getrawtransaction(txid1))
            tx2 = FromHex(CTransaction(), validator2.getrawtransaction(txid2))
            print(tx1)
            print(tx2)
            assert_equal(txid1, txid2)

        self.wait_for_transaction(txid1, timeout=150)
        disconnect_nodes(node, validator1.index)

        node.generatetoaddress(1, node.getnewaddress())
        sync_blocks([node, fork1, fork2])

        disconnect_nodes(node, fork1.index)
        disconnect_nodes(node, fork2.index)

        # create common 4 epochs to leave instant finalization
        #                        fork1
        #                       /
        # e0 - e1 - e2 - e3 - e4 node
        #                       \
        #                        fork2
        node.generatetoaddress(21, node.getnewaddress())
        assert_equal(node.getblockcount(), 24)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 4)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 3)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 3)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 3)
        assert_equal(node.getfinalizationstate()['validators'], 1)

        connect_nodes(node, fork1.index)
        connect_nodes(node, fork2.index)
        sync_blocks([node, fork1, fork2])
        disconnect_nodes(node, fork1.index)
        disconnect_nodes(node, fork2.index)

        # create fist justified epoch on fork1
        # node must follow this fork
        #                          J
        #                        - e5 - fork1, node
        #                       /
        # e0 - e1 - e2 - e3 - e4
        #                       \
        #                        fork2
        create_justification(node=fork1, finalizer=validator1, after_blocks=2)
        assert_equal(fork1.getfinalizationstate()['currentEpoch'], 5)
        assert_equal(fork1.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(fork1.getfinalizationstate()['lastJustifiedEpoch'], 5)

        connect_nodes(node, fork1.index)
        sync_blocks([node, fork1])
        disconnect_nodes(node, fork1.index)

        assert_equal(node.getfinalizationstate()['currentEpoch'], 5)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 5)
        self.log.info('node successfully switched to the justified fork')

        # create longer justified epoch on fork2
        # node must switch ("zig") to this fork
        #                          J
        #                        - e5 - fork1
        #                       /
        # e0 - e1 - e2 - e3 - e4
        #                       \            J
        #                        - e5 - e6 - e7 - fork2, node
        create_justification(node=fork2, finalizer=validator2, after_blocks=12)
        assert_equal(fork2.getfinalizationstate()['currentEpoch'], 7)
        assert_equal(fork2.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(fork2.getfinalizationstate()['lastJustifiedEpoch'], 7)

        connect_nodes(node, fork2.index)
        sync_blocks([node, fork2])
        disconnect_nodes(node, fork2.index)

        assert_equal(node.getfinalizationstate()['currentEpoch'], 7)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 7)

        self.log.info('node successfully switched to the longest justified fork')

        # create longer justified epoch on the previous fork1
        # node must switch ("zag") to this fork
        #                          J              J
        #                        - e5 - e6 - e7 - e8 fork1, node
        #                       /
        # e0 - e1 - e2 - e3 - e4
        #                       \            J
        #                        - e5 - e6 - e7 - fork2
        create_justification(node=fork1, finalizer=validator1, after_blocks=16)
        assert_equal(fork1.getfinalizationstate()['currentEpoch'], 8)
        assert_equal(fork1.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(fork1.getfinalizationstate()['lastJustifiedEpoch'], 8)

        connect_nodes(node, fork1.index)
        sync_blocks([node, fork1])
        disconnect_nodes(node, fork1.index)

        assert_equal(node.getfinalizationstate()['currentEpoch'], 8)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 8)

        self.log.info('node successfully switched back to the longest justified fork')


if __name__ == '__main__':
    ForkChoiceParallelJustificationsTest().main()
