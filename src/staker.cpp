#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/system.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/action.hpp>

#include "staker.hpp"


using namespace eosio;
  
  [[eosio::action]] void staker::refresh(eosio::name staker, uint64_t stake_id) {
  
    require_auth(staker);
    staker::refresh_action(staker, stake_id);
  
  };


  void staker::refresh_action(eosio::name staker, uint64_t stake_id){
   
    stakeobjects_index stakeobjects(_self, staker.value);

    auto so = stakeobjects.find(stake_id);

    eosio::check(so != stakeobjects.end(), "Stake object is not found");
    eosio::check(so -> closed == false, "Stake object is already closed and withdrawed");

    plans_index plans(_self, _self.value);
    auto plan = plans.find(so->plan_id);
    eosio::check(plan != plans.end(), "Plan is not found");

    print(" stake_id: ", stake_id);

    uint64_t current_step = (now() - so -> created_at.sec_since_epoch()) / _emit_step_in_secs;
    
    print(" current_step: ", current_step); 

    uint64_t fact_emitted_steps = ( so -> last_update_at.sec_since_epoch() - so -> created_at.sec_since_epoch()) / _emit_step_in_secs;
    print(" fact_emitted_steps: ", fact_emitted_steps);

    uint64_t pause_steps = plan -> pause / _emit_step_in_secs;
    print(" pause_steps: ", pause_steps);

    uint64_t limit_steps_on_emit = pause_steps + plan -> duration / _emit_step_in_secs;
    print(" limit_steps_on_emit: ", limit_steps_on_emit);

    uint64_t new_steps_on_emit = 0;
    uint64_t collapsed_pause_steps = 0;    

    if (fact_emitted_steps < limit_steps_on_emit) {
      
      if (current_step <= pause_steps) {

        new_steps_on_emit = 0;
        collapsed_pause_steps = current_step - fact_emitted_steps; 
        print(" current_step <= pause_steps: ");
        print(" new_steps_on_emit: ", new_steps_on_emit);
        print(" collapsed_pause_steps: ", collapsed_pause_steps);
      
      } else {

        if (current_step > limit_steps_on_emit) {

          if (fact_emitted_steps > pause_steps) {

            new_steps_on_emit = limit_steps_on_emit - fact_emitted_steps;
            print(" fact_emitted_steps > pause_steps: ");
            print(" new_steps_on_emit: ", new_steps_on_emit);
            
          } else {

            new_steps_on_emit = limit_steps_on_emit - pause_steps;
            print(" fact_emitted_steps <= pause_steps: ");
            print(" new_steps_on_emit: ", new_steps_on_emit);

          }

        } else {
          
          if (current_step > pause_steps) { 
            
            new_steps_on_emit = fact_emitted_steps > pause_steps ? current_step - fact_emitted_steps : current_step - pause_steps;  

          } else {

            new_steps_on_emit = 0;    

          }

          print(" current_step <= limit_steps_on_emit: ");
          print(" new_steps_on_emit: ", new_steps_on_emit);
          
        }


        if (fact_emitted_steps > pause_steps) { 
          
          collapsed_pause_steps = 0;

          print(" fact_emitted_steps >= pause_steps: ");
          print(" collapsed_pause_steps: ", collapsed_pause_steps);

        } else {
          print(" fact_emitted_steps < pause_steps: ");
          
          if (current_step < pause_steps) {
            print(" current_step < pause_steps: ");
            collapsed_pause_steps = current_step - fact_emitted_steps;
          
          } else {
            print(" current_step >= pause_steps: "); 
            collapsed_pause_steps = pause_steps - fact_emitted_steps; 
          
          }
          
          print(" collapsed_pause_steps: ", collapsed_pause_steps);

        
        }
        
        
      }

      double to_emit_in_segments = so->emitted_segments + (double)so->staked_balance.amount * (double)new_steps_on_emit * (double)plan -> emit_percent / (double)HUNDR_PERCENT * _segments_in_tick;
      double bonus_to_emit_in_segments = so->bonus_segments + (double)so->staked_balance.amount * (double)new_steps_on_emit * (double)plan -> bonus_percent / (double)HUNDR_PERCENT * _segments_in_tick;  

      eosio::asset to_emit_in_asset = asset((uint64_t)to_emit_in_segments / _segments_in_tick, _stake_symbol);
      eosio::asset bonus_to_emit_in_asset = asset((uint64_t)bonus_to_emit_in_segments / _segments_in_tick, _stake_symbol);

      stakeobjects.modify(so, _self, [&](auto &s){
        s.last_update_at = eosio::time_point_sec(so -> last_update_at.sec_since_epoch() + collapsed_pause_steps * _emit_step_in_secs + new_steps_on_emit * _emit_step_in_secs);
        
        eosio::check(s.last_update_at <= so->last_pay_should_be_at, "System error");

        s.emitted_balance = to_emit_in_asset;
        s.bonus_balance = bonus_to_emit_in_asset;
        s.emitted_segments = to_emit_in_segments;
        s.bonus_segments = bonus_to_emit_in_segments;
      });

    }

  };



  [[eosio::action]] void staker::withdraw(eosio::name staker, uint64_t stake_id, eosio::asset quantity) {
    require_auth(staker);
    
    stakeobjects_index stakeobjects(_self, staker.value);

    auto so = stakeobjects.find(stake_id);

    eosio::check(so != stakeobjects.end(), "Stake object is not found");
    eosio::check(so -> closed == false, "Stake object is already closed and withdrawed");

    bool is_closed = now() > so -> last_pay_should_be_at.sec_since_epoch() && so -> last_pay_should_be_at.sec_since_epoch() == so -> last_update_at.sec_since_epoch();
    
    eosio::asset to_pay = asset(0, _stake_symbol);

    to_pay += so -> emitted_balance;
    
    if (is_closed){
      to_pay += so -> bonus_balance;
      to_pay += so -> staked_balance;
    }

    eosio::check(to_pay == quantity, "Wrong quantity to pay");

    stakeobjects.modify(so, staker, [&](auto &s){
      s.closed = is_closed;
      s.emitted_segments -= so->emitted_balance.amount * _segments_in_tick;
      s.emitted_balance -= so->emitted_balance;
      s.withdrawed += to_pay;

      if (is_closed) {
        s.bonus_segments -= so->bonus_balance.amount * _segments_in_tick;
        s.bonus_balance -= so->bonus_balance;
      };

    });

    if (to_pay.amount > 0) {
      action(
          permission_level{ _self, "active"_n },
          _token_contract, "transfer"_n,
          std::make_tuple( _self, staker, to_pay, std::string("")) 
      ).send();  
    }
    
  };

  
  [[eosio::action]] void staker::setplan(uint64_t id, std::string name, uint64_t pause, uint64_t duration, uint64_t emit_percent, uint64_t bonus_percent) {
    require_auth(_self);

    plans_index plans(_self, _self.value);
    auto plan = plans.find(id);

    eosio::check(plan == plans.end(), "Plan is already exist");
    eosio::check(duration / _emit_step_in_secs >= 1, "duration should be more then one emit step (30 days)");
    eosio::check(duration % _emit_step_in_secs == 0, "duration should be multiplied to the one emit step (30 days)");
    eosio::check(pause % _emit_step_in_secs == 0, "pause should be multiplied to the one emit step (30 days)");
    
    plans.emplace(_self, [&](auto &p) {
      p.id = id;
      p.name = name;
      p.pause = pause;
      p.is_active = true;
      p.duration = duration;
      p.emit_percent = emit_percent;
      p.bonus_percent = bonus_percent;
      p.plan_to_pay = asset(0, _stake_symbol);
      p.total_staked = asset(0, _stake_symbol);
    });

  };


  [[eosio::action]] void staker::stopplan(uint64_t plan_id) {
    require_auth(_self);

    plans_index plans(_self, _self.value);
    auto plan = plans.find(plan_id);
    eosio::check(plan != plans.end(), "Plan is not exist");
    eosio::check(plan -> count == 0, "Cannot remove plan, which count is more then zero");

    plans.modify(plan, _self, [&](auto &p){
      p.is_active = false;
    });

  };
  

  [[eosio::action]] void staker::rmplan(uint64_t plan_id) {
    require_auth(_self);

    plans_index plans(_self, _self.value);
    auto plan = plans.find(plan_id);
    eosio::check(plan != plans.end(), "Plan is not exist");
    eosio::check(plan -> count == 0, "Cannot remove plan, which count is more then zero");

    plans.erase(plan);

  };
  
  [[eosio::action]] void staker::sysstake(eosio::name staker, eosio::asset quantity, uint64_t plan_id) {
    require_auth(_self);

    staker::stake(staker, _token_contract, quantity, plan_id);

  }

  void staker::stake(eosio::name staker, eosio::name code, eosio::asset quantity, uint64_t plan_id) {
    eosio::check(code == _token_contract, "Wrong token contract for stake");
    eosio::check(quantity.symbol == _stake_symbol, "Wrong symbol for stake");
    
    eosio::check(quantity.amount >= 1000, "Minimum amount for stake is a 1000 CRU");

    plans_index plans(_self, _self.value);
    
    auto plan = plans.find(plan_id);
    eosio::check(plan != plans.end(), "Plan is not found");
    eosio::check(plan -> is_active == true, "Plan is not active");

    uint64_t emit_steps = plan -> duration / _emit_step_in_secs;
    print("emit_steps: ", emit_steps);

    double plan_to_pay_amount = (double)quantity.amount * (double)emit_steps * ((double)plan -> emit_percent + (double)plan -> bonus_percent) / (double)HUNDR_PERCENT;
    print("plan_to_pay_amount: ", plan_to_pay_amount);
    eosio::asset plan_to_pay = asset((uint64_t)plan_to_pay_amount, _stake_symbol);


    gstate_index gstate(_self, _self.value);
    auto gs = gstate.find(0);


    //check stake limits
    if (gs == gstate.end()) {

      eosio::check(plan_to_pay.amount <= _emit_limit, "Cannot stake in reason of overflow of the stake limit");
      
      gstate.emplace(_self, [&](auto &gs){
        gs.plan_to_pay = plan_to_pay;
        gs.total_staked = quantity;
      });

    } else {

      eosio::check(gs -> plan_to_pay.amount + plan_to_pay.amount <= _emit_limit, "Cannot stake in reason of overflow of the stake limit");
      
      gstate.modify(gs, _self, [&](auto &gs){
        gs.plan_to_pay += plan_to_pay; 
        gs.total_staked += quantity;
      });

    }
    
    //emplace stake object
    stakeobjects_index stakeobjects(_self, staker.value);

    stakeobjects.emplace(_self, [&](auto &s){
      s.id = stakeobjects.available_primary_key();
      s.plan_id = plan_id;
      s.plan_to_pay = plan_to_pay;
      s.created_at = eosio::time_point_sec(now());
      s.last_update_at = eosio::time_point_sec(now());
      s.freeze_until = eosio::time_point_sec(now() + plan -> pause);
      s.last_pay_should_be_at = eosio::time_point_sec(now() + plan -> duration + plan -> pause);
      s.staked_balance = quantity;
      s.emitted_balance = asset(0, _stake_symbol);
      s.bonus_balance = asset(0, _stake_symbol);
      s.withdrawed = asset(0, _stake_symbol);
    });

    plans.modify(plan, _self, [&](auto &p){
      p.count += 1;
      p.plan_to_pay += plan_to_pay;
      p.total_staked += quantity;
    });

  };


extern "C" {
   
   /// The apply method implements the dispatch of events to this contract
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
        if (code == staker::_self.value) {
          if (action == "refresh"_n.value){
            execute_action(name(receiver), name(code), &staker::refresh);
          } else if (action == "withdraw"_n.value){
            execute_action(name(receiver), name(code), &staker::withdraw);
          } else if (action == "setplan"_n.value){
            execute_action(name(receiver), name(code), &staker::setplan);
          } else if (action == "rmplan"_n.value){
            execute_action(name(receiver), name(code), &staker::rmplan);
          } else if (action == "sysstake"_n.value){
            execute_action(name(receiver), name(code), &staker::sysstake);
          } else if (action == "stopplan"_n.value){
            execute_action(name(receiver), name(code), &staker::stopplan);
          }
            
        } else {

          if (action == "transfer"_n.value){
            
            struct transfer{
                eosio::name from;
                eosio::name to;
                eosio::asset quantity;
                std::string memo;
            };

            auto op = eosio::unpack_action_data<transfer>();

            if (op.to == staker::_self) {
              //DISPATCHER ON INCOMING TRANSFERS 
              //PLANS:
              // 1 - накопительный
              // 2 - классический
              // 3 - оптимальный
              // 4 - уверенный
              // 5 - выгодный
              // 6 - эвенти 2020 

              uint64_t plan = atoll(op.memo.c_str());

              if (op.quantity.symbol == staker::_stake_symbol) {

                require_auth(op.from);
                staker::stake(op.from, name(code), op.quantity, plan);

              }
            }
          }
        }
  };
};
