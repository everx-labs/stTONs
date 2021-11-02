#pragma once

#include <tvm/schema/message.hpp>
#include <tvm/smart_switcher.hpp>
#include <tvm/contract_handle.hpp>
#include <tvm/replay_attack_protection/timestamp.hpp>

#include "PriceCommon.hpp"
#include "DePool.hpp"
#include "TONTokenWallet.hpp"

namespace tvm { inline namespace schema {

static constexpr unsigned STTONS_TIMESTAMP_DELAY = 1800;
using sttons_replay_protection_t = replay_attack_protection::timestamp<STTONS_TIMESTAMP_DELAY>;

// lend structures are declared in TONTokenWallet.hpp
/*struct lend_record {
  uint128 lend_balance;
  uint32 lend_finish_time;
};
using lend_ownership_map = small_dict_map<addr_std_fixed, lend_record>;

struct lend_array_record {
  address lend_addr;
  uint128 lend_balance;
  uint32 lend_finish_time;
};
using lend_ownership_array = dict_array<lend_array_record>;*/

__interface IstTONsNotify {
  [[internal, noaccept, answer_id]]
  bool_t onLendOwnership(
    uint128 balance,
    uint32  lend_finish_time,
    uint256 pubkey,
    std::optional<address> internal_owner,
    address depool,
    uint256 depool_pubkey,
    cell    payload,
    address answer_addr
  ) = 201;
};
using IstTONsNotifyPtr = handle<IstTONsNotify>;

struct stTONsDetails {
  uint256 owner_pubkey;
  address owner_address;
  address depool;
  uint256 depool_pubkey;
  uint128 balance;
  lend_ownership_array lend_ownership;
  uint128 lend_balance; // sum lend balance to all targets
  bool_t transferring_stake_back;
  uint8 last_depool_error;
};

__interface IstTONs {

  [[internal, noaccept]]
  void onDeploy() = 10;

  // lend ownership to some contract until 'lend_finish_time'
  [[external, internal, noaccept, answer_id]]
  void lendOwnership(
    address answer_addr,
    uint128 grams,
    address dest,
    uint128 lend_balance,
    uint32  lend_finish_time,
    cell    deploy_init_cl,
    cell    payload
  ) = 11;

  // return ownership back to the original owner (for provided amount of tokens)
  [[internal, noaccept]]
  void returnOwnership(uint128 tokens) = 12;

  // Return stake to the provided `dst`.
  // Only works when contract is not in "lend mode".
  // Will return all stake (calling `IDePool::transferStake(dst, 0_u64)`)
  // `processing_crystals` - value will be attached to the message
  [[external, internal, noaccept]]
  void returnStake(address dst, uint128 processing_crystals) = 13;

  // Eliminate contract and return all the remaining crystals to `dst`
  // `ignore_errors` - ignore error returned by depool for transferStake
  // finalize will not work if contract is not in "lend mode"
  // or last_depool_error_ != 0 && !ignore_errors
  // WARNING: do not use `ignore_errors=true`, until you are really sure that
  //   your stake in depool is empty or insignificant
  // STATUS_DEPOOL_CLOSED / STATUS_NO_PARTICIPANT are not considered as an errors forbidding `finalize`
  [[external, internal, noaccept]]
  void finalize(address dst, bool_t ignore_errors) = 14;

  // Receive stake transfer notify (from solidity IParticipant::onTransfer(address source, uint128 amount))
  [[internal, noaccept]]
  void receiveStakeTransfer(address source, uint128 amount) = 0x23c4771d; // = hash_v<"onTransfer(address,uint128)()v2">

  // If error occured while transferring stake back
  [[internal, noaccept]]
  void receiveAnswer(uint32 errcode, uint64 comment) = 0x3f109e44; // IParticipant::receiveAnswer

  // ========== getters ==========
  [[getter]]
  stTONsDetails getDetails() = 15;

  [[getter]]
  address calcStTONsAddr(
    cell code,
    uint256 owner_pubkey,
    std::optional<address> owner_address,
    address depool,
    uint256 depool_pubkey
  ) = 16;
};
using IstTONsPtr = handle<IstTONs>;

struct DstTONs {
  uint256 owner_pubkey_;
  std::optional<address> owner_address_;
  IDePoolPtr depool_;
  uint256 depool_pubkey_;
  uint128 balance_;
  lend_ownership_map lend_ownership_;
  bool_t transferring_stake_back_;
  uint8 last_depool_error_;
};

__interface EstTONs {
};

// Prepare stTONs StateInit structure and expected contract address (hash from StateInit)
inline
std::pair<StateInit, uint256> prepare_sttons_state_init_and_addr(DstTONs data, cell code) {
  cell data_cl =
    prepare_persistent_data<IstTONs, sttons_replay_protection_t, DstTONs>(
      sttons_replay_protection_t::init(), data
    );
  StateInit sttons_init {
    /*split_depth*/{}, /*special*/{},
    code, data_cl, /*library*/{}
  };
  cell init_cl = build(sttons_init).make_cell();
  return { sttons_init, uint256(tvm_hash(init_cl)) };
}

}} // namespace tvm::schema

