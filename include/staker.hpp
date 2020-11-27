#include <eosiolib/multi_index.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/system.hpp>
#include <eosiolib/singleton.hpp>
using namespace eosio;


#define ONE_PERCENT 10000
#define HUNDR_PERCENT 1000000

class [[eosio::contract]] staker : public eosio::contract {

public:
    staker( eosio::name receiver, eosio::name code, eosio::datastream<const char*> ds ): 
    eosio::contract(receiver, code, ds)
    {}
    [[eosio::action]] void refresh(eosio::name staker, uint64_t stake_id);
    [[eosio::action]] void withdraw(eosio::name staker, uint64_t stake_id, eosio::asset quantity); 
    [[eosio::action]] void setplan(uint64_t id, std::string name, uint64_t pause, uint64_t duration_sec, uint64_t emit_percent, uint64_t bonus_percent);
    [[eosio::action]] void rmplan(uint64_t plan_id);
    [[eosio::action]] void stopplan(uint64_t plan_id);
    [[eosio::action]] void sysstake(eosio::name staker, eosio::asset quantity, uint64_t plan_id);


    static void stake(eosio::name staker, eosio::name code, eosio::asset quantity, uint64_t plan_id);
    static void refresh_action(eosio::name staker, uint64_t stake_id);



    static constexpr eosio::name _self = "staker"_n;
    static constexpr eosio::name _token_contract = "eosio.token"_n;
    static constexpr eosio::symbol _stake_symbol     = eosio::symbol(eosio::symbol_code("CRU"), 0);
    
    static constexpr uint64_t _emit_limit = 3000000000;
    static const uint64_t _segments_in_tick = 100000000; 

    static constexpr uint64_t _emit_step_in_secs = 30 * 86400;//PROD
    // static constexpr uint64_t _emit_step_in_secs = 10; //TEST
    

    struct [[eosio::table]] stakeobjects {
        uint64_t id;
        uint64_t plan_id;

        eosio::time_point_sec created_at;
        eosio::time_point_sec freeze_until;
        eosio::time_point_sec last_update_at;

        eosio::time_point_sec last_pay_should_be_at;

        eosio::asset staked_balance;
        bool closed = false;
        eosio::asset plan_to_pay;

        eosio::asset emitted_balance;
        double emitted_segments;
        eosio::asset bonus_balance;
        double bonus_segments;
        eosio::asset withdrawed;

        uint64_t primary_key() const {return id;}
        
        uint64_t byplan() const {return plan_id;}
        uint64_t bybalance() const {return staked_balance.amount;}
        uint64_t bycreated() const {return created_at.sec_since_epoch();}
        uint64_t byupdated() const {return last_update_at.sec_since_epoch();}


        EOSLIB_SERIALIZE(stakeobjects, (id)(plan_id)(created_at)(freeze_until)(last_update_at)(last_pay_should_be_at)(staked_balance)(closed)(plan_to_pay)(emitted_balance)(emitted_segments)(bonus_balance)(bonus_segments)(withdrawed))
    };

    typedef eosio::multi_index<"stakeobjects"_n, stakeobjects,
      eosio::indexed_by<"byplan"_n, eosio::const_mem_fun<stakeobjects, uint64_t, &stakeobjects::byplan>>,
      eosio::indexed_by<"bybalance"_n, eosio::const_mem_fun<stakeobjects, uint64_t, &stakeobjects::bybalance>>,
      eosio::indexed_by<"bycreated"_n, eosio::const_mem_fun<stakeobjects, uint64_t, &stakeobjects::bycreated>>,
      eosio::indexed_by<"byupdated"_n, eosio::const_mem_fun<stakeobjects, uint64_t, &stakeobjects::byupdated>>
    > stakeobjects_index;     
    

    struct [[eosio::table]] gstate {
        uint64_t id;
        eosio::asset total_staked;
        eosio::asset plan_to_pay;

        uint64_t primary_key() const {return id;}

        EOSLIB_SERIALIZE(gstate, (id)(total_staked)(plan_to_pay))
    };

    typedef eosio::multi_index<"gstate"_n, gstate> gstate_index;
    

    struct [[eosio::table]] plans {
        uint64_t id;
        std::string name;
        bool is_active;
        uint64_t pause;
        uint64_t duration;
        uint64_t emit_percent;
        uint64_t bonus_percent;
        uint64_t count;
        eosio::asset plan_to_pay;
        eosio::asset total_staked;

        uint64_t primary_key() const {return id;}

        EOSLIB_SERIALIZE(plans, (id)(name)(is_active)(pause)(duration)(emit_percent)(bonus_percent)(count)(plan_to_pay)(total_staked))
    };

    typedef eosio::multi_index<"plans"_n, plans> plans_index;
    

};
