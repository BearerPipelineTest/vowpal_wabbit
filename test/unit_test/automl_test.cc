// Copyright (c) by respective owners including Yahoo!, Microsoft, and
// individual contributors. All rights reserved. Released under a BSD (revised)
// license as described in the file LICENSE.

#include "vw/core/reductions/automl.h"

#include "reductions/details/automl_impl.h"
#include "simulator.h"
#include "test_common.h"
#include "vw/core/estimator_config.h"
#include "vw/core/metric_sink.h"
#include "vw/core/vw_fwd.h"

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <functional>
#include <map>
#include <utility>

using simulator::callback_map;
using simulator::cb_sim;
using namespace VW::reductions::automl;

namespace aml_test
{
template <typename T>
void check_interactions_match_exclusions(VW::reductions::automl::automl<
    interaction_config_manager<VW::reductions::automl::config_oracle<T>, VW::estimator_config>>* aml)
{
  for (const auto& estimator : aml->cm->estimators)
  {
    BOOST_CHECK(aml->cm->_config_oracle.configs[estimator.first.config_index].conf_type == config_type::Exclusion);
    auto& exclusions = aml->cm->_config_oracle.configs[estimator.first.config_index].elements;
    auto& interactions = estimator.first.live_interactions;
    auto& interaction_type = aml->cm->_config_oracle._interaction_type;
    // Check that no interaction can be found in exclusions
    for (const auto& interaction : interactions)
    {
      if (interaction_type == "quadratic")
      {
        VW::namespace_index ns1 = interaction[0];
        VW::namespace_index ns2 = interaction[1];
        std::vector<VW::namespace_index> ns{ns1, ns2};
        BOOST_CHECK(exclusions.find(ns) == exclusions.end());
      }
      else
      {
        VW::namespace_index ns1 = interaction[0];
        VW::namespace_index ns2 = interaction[1];
        VW::namespace_index ns3 = interaction[2];
        std::vector<VW::namespace_index> ns{ns1, ns2, ns3};
        BOOST_CHECK(exclusions.find(ns) == exclusions.end());
      }
    }
    // Check that interaction count is equal to quadratic interaction size minus exclusion count
    size_t exclusion_count = exclusions.size();
    size_t quad_inter_count = (aml->cm->ns_counter.size()) * (aml->cm->ns_counter.size() + 1) / 2;
    BOOST_CHECK_EQUAL(interactions.size(), quad_inter_count - exclusion_count);
  }
}

template <typename T>
void check_config_states(VW::reductions::automl::automl<
    interaction_config_manager<VW::reductions::automl::config_oracle<T>, VW::estimator_config>>* aml)
{
  // No configs in the index queue should be live
  auto index_queue = aml->cm->_config_oracle.index_queue;
  while (!index_queue.empty())
  {
    auto config_index = index_queue.top().second;
    index_queue.pop();
    BOOST_CHECK(aml->cm->_config_oracle.configs[config_index].state != VW::reductions::automl::config_state::Live);
  }

  // All configs in the estimators should be live
  for (const auto& score : aml->cm->estimators)
  {
    BOOST_CHECK(
        aml->cm->_config_oracle.configs[score.first.config_index].state == VW::reductions::automl::config_state::Live);
  }
}

template <typename T>
VW::reductions::automl::automl<
    interaction_config_manager<VW::reductions::automl::config_oracle<T>, VW::estimator_config>>*
get_automl_data(VW::workspace& all)
{
  std::vector<std::string> e_r;
  all.l->get_enabled_reductions(e_r);
  if (std::find(e_r.begin(), e_r.end(), "automl") == e_r.end())
  { BOOST_FAIL("automl not found in enabled reductions"); }

  VW::LEARNER::multi_learner* automl_learner = as_multiline(all.l->get_learner_by_name_prefix("automl"));

  return (VW::reductions::automl::automl<
      interaction_config_manager<VW::reductions::automl::config_oracle<T>, VW::estimator_config>>*)
      automl_learner->get_internal_type_erased_data_pointer_test_use_only();
}
template VW::reductions::automl::automl<interaction_config_manager<
    VW::reductions::automl::config_oracle<VW::reductions::automl::oracle_rand_impl>, VW::estimator_config>>*
get_automl_data(VW::workspace& all);
template VW::reductions::automl::automl<interaction_config_manager<
    VW::reductions::automl::config_oracle<VW::reductions::automl::one_diff_impl>, VW::estimator_config>>*
get_automl_data(VW::workspace& all);
template VW::reductions::automl::automl<interaction_config_manager<
    VW::reductions::automl::config_oracle<VW::reductions::automl::champdupe_impl>, VW::estimator_config>>*
get_automl_data(VW::workspace& all);

using aml_rand = VW::reductions::automl::automl<interaction_config_manager<
    VW::reductions::automl::config_oracle<VW::reductions::automl::oracle_rand_impl>, VW::estimator_config>>;
using aml_onediff = VW::reductions::automl::automl<interaction_config_manager<
    VW::reductions::automl::config_oracle<VW::reductions::automl::one_diff_impl>, VW::estimator_config>>;
}  // namespace aml_test

// Need to add save_load functionality to multiple structs in automl reduction including
// config_manager and estimator_config.
BOOST_AUTO_TEST_CASE(automl_save_load)
{
  const size_t num_iterations = 1000;
  const size_t split = 690;
  const size_t seed = 88;
  const std::vector<uint64_t> swap_after = {500};
  callback_map empty_hooks;
  auto ctr_no_save = simulator::_test_helper_hook(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--fixed_significance_level "
      "--random_seed 5 --global_lease 10",
      empty_hooks, num_iterations, seed, swap_after);
  BOOST_CHECK_GT(ctr_no_save.back(), 0.6f);

  auto ctr_with_save = simulator::_test_helper_save_load(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--fixed_significance_level "
      "--random_seed 5 --global_lease 10",
      num_iterations, seed, swap_after, split);
  BOOST_CHECK_GT(ctr_with_save.back(), 0.6f);

  BOOST_CHECK_EQUAL_COLLECTIONS(ctr_no_save.begin(), ctr_no_save.end(), ctr_with_save.begin(), ctr_with_save.end());
}

BOOST_AUTO_TEST_CASE(automl_assert_0th_event_automl)
{
  const size_t zero = 0;
  const size_t num_iterations = 10;
  callback_map test_hooks;

  // technically runs after the 0th example is learned
  test_hooks.emplace(zero, [&zero](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_rand* aml = aml_test::get_automl_data<VW::reductions::automl::oracle_rand_impl>(all);
    BOOST_CHECK_EQUAL(aml->cm->total_learn_count, zero);
    BOOST_CHECK(aml->current_state == VW::reductions::automl::automl_state::Experimenting);
    return true;
  });

  // test executes right after learn call of the 10th example
  test_hooks.emplace(num_iterations, [&num_iterations](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_rand* aml = aml_test::get_automl_data<VW::reductions::automl::oracle_rand_impl>(all);
    BOOST_CHECK_EQUAL(aml->cm->total_learn_count, num_iterations);
    BOOST_CHECK(aml->current_state == VW::reductions::automl::automl_state::Experimenting);
    return true;
  });

  auto ctr = simulator::_test_helper_hook(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--random_seed 5 "
      "--oracle_type rand --global_lease 10",
      test_hooks, num_iterations);

  BOOST_CHECK_GT(ctr.back(), 0.1f);
}

BOOST_AUTO_TEST_CASE(automl_assert_0th_event_metrics)
{
  const auto metric_name = std::string("total_learn_calls");
  const size_t zero = 0;
  const size_t num_iterations = 10;
  callback_map test_hooks;

  // technically runs after the 0th example is learned
  test_hooks.emplace(zero, [&metric_name, &zero](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    VW::metric_sink metrics;
    all.l->persist_metrics(metrics);

    BOOST_REQUIRE_EQUAL(metrics.get_uint(metric_name), zero);
    return true;
  });

  // test executes right after learn call of the 10th example
  test_hooks.emplace(num_iterations, [&metric_name, &num_iterations](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    VW::metric_sink metrics;
    all.l->persist_metrics(metrics);

    BOOST_REQUIRE_EQUAL(metrics.get_uint(metric_name), num_iterations);
    return true;
  });

  auto ctr = simulator::_test_helper_hook(
      "--extra_metrics ut_metrics.json --cb_explore_adf --quiet --epsilon 0.2 --random_seed 5 --global_lease 10",
      test_hooks, num_iterations);

  BOOST_CHECK_GT(ctr.back(), 0.1f);
}

BOOST_AUTO_TEST_CASE(automl_assert_live_configs_and_lease)
{
  const size_t fifteen = 15;
  const size_t thirty_three = 33;
  const size_t num_iterations = 100;
  callback_map test_hooks;

  // Note this is after learning 14 examples (first iteration is Collecting)
  test_hooks.emplace(fifteen, [&fifteen](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_rand* aml = aml_test::get_automl_data<VW::reductions::automl::oracle_rand_impl>(all);
    aml_test::check_interactions_match_exclusions(aml);
    aml_test::check_config_states(aml);
    BOOST_CHECK(aml->current_state == VW::reductions::automl::automl_state::Experimenting);
    BOOST_CHECK_EQUAL(aml->cm->total_learn_count, 15);
    BOOST_CHECK_EQUAL(aml->cm->current_champ, 0);
    BOOST_CHECK_CLOSE(aml->cm->automl_significance_level, 0.05, FLOAT_TOL);
    BOOST_CHECK_CLOSE(aml->cm->automl_estimator_decay, 1.0, FLOAT_TOL);
    BOOST_CHECK_EQUAL(aml->cm->estimators[0].first.config_index, 0);
    BOOST_CHECK_EQUAL(aml->cm->estimators[1].first.config_index, 3);
    BOOST_CHECK_EQUAL(aml->cm->estimators[2].first.config_index, 1);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.configs.size(), 4);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.configs[0].lease, 10);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.configs[1].lease, 10);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.configs[2].lease, 20);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.configs[3].lease, 20);
    BOOST_CHECK_EQUAL(aml->cm->estimators[0].first._estimator.update_count, 0);
    BOOST_CHECK_EQUAL(aml->cm->estimators[1].first._estimator.update_count, 15);
    BOOST_CHECK_EQUAL(aml->cm->estimators[2].first._estimator.update_count, 5);
    BOOST_CHECK_EQUAL(aml->cm->estimators[0].second.update_count, 0);
    BOOST_CHECK_EQUAL(aml->cm->estimators[1].second.update_count, 15);
    BOOST_CHECK_EQUAL(aml->cm->estimators[2].second.update_count, 5);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.index_queue.size(), 0);
    return true;
  });

  auto ctr = simulator::_test_helper_hook(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--fixed_significance_level "
      "--random_seed 5 "
      "--oracle_type rand --global_lease 10",
      test_hooks, num_iterations);

  BOOST_CHECK_GT(ctr.back(), 0.1f);
}

// Note higher ctr compared to cpp_simulator_without_interaction in tutorial_test.cc
BOOST_AUTO_TEST_CASE(automl_cpp_simulator_automl)
{
  auto ctr = simulator::_test_helper(
      "--cb_explore_adf --quiet --epsilon 0.2 --random_seed 5 --automl 3 --priority_type "
      "favor_popular_namespaces --oracle_type rand --global_lease 10");
  BOOST_CHECK_GT(ctr.back(), 0.6f);
}

BOOST_AUTO_TEST_CASE(automl_namespace_switch)
{
  const size_t num_iterations = 1000;
  callback_map test_hooks;
  const std::vector<uint64_t> swap_after = {500};
  const size_t seed = 88;

  test_hooks.emplace(100, [&](cb_sim& sim, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_onediff* aml = aml_test::get_automl_data<VW::reductions::automl::one_diff_impl>(all);
    auto count_ns_T = aml->cm->ns_counter.count('T');
    BOOST_CHECK_EQUAL(count_ns_T, 0);

    // change user namespace to start with letter T
    sim.user_ns = "Tser";
    return true;
  });

  test_hooks.emplace(101, [&](cb_sim& sim, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_onediff* aml = aml_test::get_automl_data<VW::reductions::automl::one_diff_impl>(all);
    size_t tser_count = aml->cm->ns_counter.at('T');
    BOOST_CHECK_GT(tser_count, 1);

    // reset user namespace to appropriate value
    sim.user_ns = "User";
    return true;
  });

  test_hooks.emplace(num_iterations, [&](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_onediff* aml = aml_test::get_automl_data<VW::reductions::automl::one_diff_impl>(all);

    auto champ_exclusions =
        aml->cm->_config_oracle.configs[aml->cm->estimators[aml->cm->current_champ].first.config_index].elements;
    BOOST_CHECK_EQUAL(champ_exclusions.size(), 1);
    std::vector<VW::namespace_index> ans{'U', 'U'};
    BOOST_CHECK(champ_exclusions.find(ans) != champ_exclusions.end());
    auto champ_interactions = aml->cm->estimators[aml->cm->current_champ].first.live_interactions;
    BOOST_CHECK_EQUAL(champ_interactions.size(), 5);
    return true;
  });

  auto ctr = simulator::_test_helper_hook(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--random_seed 5 "
      "--global_lease 500 --oracle_type one_diff --noconstant ",
      test_hooks, num_iterations, seed, swap_after);
  BOOST_CHECK_GT(ctr.back(), 0.65f);
}

BOOST_AUTO_TEST_CASE(automl_clear_configs)
{
  const size_t seed = 85;
  const size_t num_iterations = 1000;
  const std::vector<uint64_t> swap_after = {200, 500};
  const size_t clear_champ_switch = 931;
  callback_map test_hooks;

  test_hooks.emplace(clear_champ_switch - 1, [&](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_rand* aml = aml_test::get_automl_data<VW::reductions::automl::oracle_rand_impl>(all);
    aml_test::check_interactions_match_exclusions<VW::reductions::automl::oracle_rand_impl>(aml);
    aml_test::check_config_states(aml);
    BOOST_CHECK_EQUAL(aml->cm->current_champ, 0);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.valid_config_size, 4);
    BOOST_CHECK_EQUAL(clear_champ_switch - 1, aml->cm->total_learn_count);
    BOOST_CHECK_EQUAL(aml->cm->estimators[0].first.live_interactions.size(), 3);
    BOOST_CHECK_EQUAL(aml->cm->estimators[1].first.live_interactions.size(), 2);
    BOOST_CHECK_EQUAL(aml->cm->estimators[2].first.live_interactions.size(), 2);
    BOOST_CHECK(aml->current_state == VW::reductions::automl::automl_state::Experimenting);
    return true;
  });

  test_hooks.emplace(clear_champ_switch, [&clear_champ_switch](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_rand* aml = aml_test::get_automl_data<VW::reductions::automl::oracle_rand_impl>(all);
    aml_test::check_interactions_match_exclusions(aml);
    aml_test::check_config_states(aml);
    BOOST_CHECK_EQUAL(aml->cm->current_champ, 0);
    BOOST_CHECK_EQUAL(clear_champ_switch, aml->cm->total_learn_count);
    BOOST_CHECK_EQUAL(aml->cm->estimators.size(), 2);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.valid_config_size, 4);
    BOOST_CHECK_EQUAL(aml->cm->estimators[0].first.live_interactions.size(), 2);
    BOOST_CHECK(aml->current_state == VW::reductions::automl::automl_state::Experimenting);
    return true;
  });

  // we initialize the reduction pointing to position 0 as champ, that config is hard-coded to empty
  auto ctr = simulator::_test_helper_hook(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--fixed_significance_level "
      "--random_seed 5 --oracle_type rand --global_lease 500 --noconstant ",
      test_hooks, num_iterations, seed, swap_after);

  BOOST_CHECK_GT(ctr.back(), 0.4f);
}

BOOST_AUTO_TEST_CASE(automl_clear_configs_one_diff)
{
  const size_t num_iterations = 1000;
  const std::vector<uint64_t> swap_after = {500};
  const size_t seed = 88;
  const size_t clear_champ_switch = 645;
  callback_map test_hooks;

  test_hooks.emplace(clear_champ_switch - 1, [&](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_onediff* aml = aml_test::get_automl_data<VW::reductions::automl::one_diff_impl>(all);
    aml_test::check_interactions_match_exclusions(aml);
    aml_test::check_config_states(aml);
    BOOST_CHECK_EQUAL(aml->cm->current_champ, 0);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.valid_config_size, 4);
    BOOST_CHECK_EQUAL(clear_champ_switch - 1, aml->cm->total_learn_count);
    BOOST_CHECK(aml->current_state == VW::reductions::automl::automl_state::Experimenting);
    return true;
  });

  test_hooks.emplace(clear_champ_switch, [&clear_champ_switch](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_onediff* aml = aml_test::get_automl_data<VW::reductions::automl::one_diff_impl>(all);
    aml_test::check_interactions_match_exclusions(aml);
    aml_test::check_config_states(aml);
    BOOST_CHECK_EQUAL(aml->cm->current_champ, 0);
    BOOST_CHECK_EQUAL(clear_champ_switch, aml->cm->total_learn_count);
    BOOST_CHECK_EQUAL(aml->cm->estimators.size(), 3);
    BOOST_CHECK_EQUAL(aml->cm->_config_oracle.valid_config_size, 4);
    BOOST_CHECK(aml->current_state == VW::reductions::automl::automl_state::Experimenting);
    return true;
  });

  test_hooks.emplace(clear_champ_switch + 1, [&clear_champ_switch](cb_sim&, VW::workspace& all, VW::multi_ex&) {
    aml_test::aml_onediff* aml = aml_test::get_automl_data<VW::reductions::automl::one_diff_impl>(all);
    BOOST_CHECK_EQUAL(aml->cm->estimators.size(), 3);
    BOOST_CHECK_EQUAL(aml->cm->estimators[0].first.live_interactions.size(), 3);
    BOOST_CHECK_EQUAL(aml->cm->estimators[1].first.live_interactions.size(), 2);
    BOOST_CHECK_EQUAL(aml->cm->estimators[2].first.live_interactions.size(), 2);
    return true;
  });

  // we initialize the reduction pointing to position 0 as champ, that config is hard-coded to empty
  auto ctr = simulator::_test_helper_hook(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--fixed_significance_level "
      "--random_seed 5 --noconstant --global_lease 10",
      test_hooks, num_iterations, seed, swap_after);

  BOOST_CHECK_GT(ctr.back(), 0.65f);
}

BOOST_AUTO_TEST_CASE(automl_q_col_consistency)
{
  const size_t seed = 88;
  const size_t num_iterations = 1000;

  auto ctr_q_col = simulator::_test_helper(
      "--cb_explore_adf --quiet --epsilon 0.2 --random_seed 5 -q :: --global_lease 10", num_iterations, seed);
  auto ctr_aml = simulator::_test_helper(
      "--cb_explore_adf --quiet --epsilon 0.2 --random_seed 5 --automl 1 --global_lease 10", num_iterations, seed);

  BOOST_CHECK_CLOSE(ctr_q_col.back(), ctr_aml.back(), FLOAT_TOL);
}

BOOST_AUTO_TEST_CASE(one_diff_impl_unittest)
{
  using namespace VW::reductions::automl;

  const size_t num_iterations = 2;
  callback_map test_hooks;
  // const std::vector<uint64_t> swap_after = {500};
  const size_t seed = 88;

  test_hooks.emplace(1, [&](cb_sim& sim, VW::workspace& all, VW::multi_ex&) {
    const size_t CHAMP = 0;
    auto* aml = aml_test::get_automl_data<one_diff_impl>(all);

    config_oracle<one_diff_impl>& co = aml->cm->_config_oracle;
    auto rand_state = all.get_random_state();

    std::map<VW::namespace_index, uint64_t> ns_counter;
    std::vector<std::pair<aml_estimator<VW::estimator_config>, VW::estimator_config>> estimators;

    config_oracle<one_diff_impl> oracle(
        aml->cm->global_lease, co.calc_priority, co._interaction_type, co._oracle_type, rand_state);

    auto& configs = oracle.configs;
    auto& prio_queue = oracle.index_queue;

    ns_counter['A'] = 1;
    ns_counter['B'] = 1;

    BOOST_CHECK_EQUAL(configs.size(), 0);
    BOOST_CHECK_EQUAL(estimators.size(), 0);
    BOOST_CHECK_EQUAL(prio_queue.size(), 0);
    interaction_config_manager<config_oracle<one_diff_impl>, VW::estimator_config>::insert_starting_configuration(
        estimators, oracle, aml->cm->automl_significance_level, aml->cm->automl_estimator_decay);
    BOOST_CHECK_EQUAL(configs.size(), 1);
    BOOST_CHECK_EQUAL(estimators.size(), 1);
    BOOST_CHECK_EQUAL(prio_queue.size(), 0);
    auto& champ_interactions = estimators[CHAMP].first.live_interactions;

    BOOST_CHECK_EQUAL(champ_interactions.size(), 0);
    auto& exclusions = oracle.configs[estimators[0].first.config_index];
    auto& interactions = estimators[0].first.live_interactions;
    ns_based_config::apply_config_to_interactions(
        false, ns_counter, oracle._interaction_type, exclusions, interactions);
    BOOST_CHECK_EQUAL(champ_interactions.size(), 3);

    const interaction_vec_t expected = {
        {'A', 'A'},
        {'A', 'B'},
        {'B', 'B'},
    };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        champ_interactions.begin(), champ_interactions.end(), expected.begin(), expected.end());

    BOOST_CHECK_EQUAL(configs.size(), 1);
    oracle.gen_configs(estimators[CHAMP].first.live_interactions, ns_counter);
    BOOST_CHECK_EQUAL(configs.size(), 4);
    BOOST_CHECK_EQUAL(prio_queue.size(), 3);

    const set_ns_list_t excl_0{};
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[0].elements.begin(), configs[0].elements.end(), excl_0.begin(), excl_0.end());
    const set_ns_list_t excl_1{{'A', 'A'}};
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[1].elements.begin(), configs[1].elements.end(), excl_1.begin(), excl_1.end());
    const set_ns_list_t excl_2{{'A', 'B'}};
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[2].elements.begin(), configs[2].elements.end(), excl_2.begin(), excl_2.end());
    const set_ns_list_t excl_3{{'B', 'B'}};
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[3].elements.begin(), configs[3].elements.end(), excl_3.begin(), excl_3.end());

    BOOST_CHECK_EQUAL(estimators.size(), 1);
    // add dummy evaluators to simulate that all configs are in play
    for (size_t i = 1; i < configs.size(); ++i)
    {
      interaction_config_manager<config_oracle<one_diff_impl>, VW::estimator_config>::apply_config_at_slot(estimators,
          oracle.configs, i, config_oracle<one_diff_impl>::choose(oracle.index_queue),
          aml->cm->automl_significance_level, aml->cm->automl_estimator_decay, 1);
      auto& temp_exclusions = oracle.configs[estimators[i].first.config_index];
      auto& temp_interactions = estimators[i].first.live_interactions;
      ns_based_config::apply_config_to_interactions(
          false, ns_counter, oracle._interaction_type, temp_exclusions, temp_interactions);
    }
    BOOST_CHECK_EQUAL(prio_queue.size(), 0);
    BOOST_CHECK_EQUAL(estimators.size(), 4);

    // excl_2 is now champ
    interaction_config_manager<config_oracle<one_diff_impl>, VW::estimator_config>::apply_new_champ(
        oracle, 2, estimators, 0, false, ns_counter);

    BOOST_CHECK_EQUAL_COLLECTIONS(configs[0].elements.begin(), configs[0].elements.end(), excl_2.begin(), excl_2.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[1].elements.begin(), configs[1].elements.end(), excl_0.begin(), excl_0.end());

    BOOST_CHECK_EQUAL(oracle.valid_config_size, 4);
    BOOST_CHECK_EQUAL(configs.size(), 4);

    const set_ns_list_t excl_4{{'A', 'A'}, {'A', 'B'}};
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[2].elements.begin(), configs[2].elements.end(), excl_4.begin(), excl_4.end());
    const set_ns_list_t excl_5{{'A', 'B'}, {'B', 'B'}};
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[3].elements.begin(), configs[3].elements.end(), excl_5.begin(), excl_5.end());

    // the previous two exclusion configs are now inside the priority queue
    BOOST_CHECK_EQUAL(prio_queue.size(), 2);

    // add dummy evaluators to simulate that all configs are in play
    for (size_t i = 2; i < 4; ++i)
    {
      interaction_config_manager<config_oracle<one_diff_impl>, VW::estimator_config>::apply_config_at_slot(estimators,
          oracle.configs, i, config_oracle<one_diff_impl>::choose(oracle.index_queue),
          aml->cm->automl_significance_level, aml->cm->automl_estimator_decay, 1);
      auto& temp_config = oracle.configs[estimators[i].first.config_index];
      auto& temp_interactions = estimators[i].first.live_interactions;
      ns_based_config::apply_config_to_interactions(
          false, ns_counter, oracle._interaction_type, temp_config, temp_interactions);
    }
    BOOST_CHECK_EQUAL(prio_queue.size(), 0);

    // excl_4 is now champ
    interaction_config_manager<config_oracle<one_diff_impl>, VW::estimator_config>::apply_new_champ(
        oracle, 3, estimators, 0, false, ns_counter);

    BOOST_CHECK_EQUAL_COLLECTIONS(configs[0].elements.begin(), configs[0].elements.end(), excl_4.begin(), excl_4.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[1].elements.begin(), configs[1].elements.end(), excl_2.begin(), excl_2.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[3].elements.begin(), configs[3].elements.end(), excl_1.begin(), excl_1.end());
    const set_ns_list_t excl_6{{'A', 'A'}, {'A', 'B'}, {'B', 'B'}};
    BOOST_CHECK_EQUAL_COLLECTIONS(configs[2].elements.begin(), configs[2].elements.end(), excl_6.begin(), excl_6.end());

    BOOST_CHECK_EQUAL(prio_queue.size(), 2);

    return true;
  });

  auto ctr = simulator::_test_helper_hook(
      "--automl 3 --priority_type favor_popular_namespaces --cb_explore_adf --quiet --epsilon 0.2 "
      "--random_seed 5 "
      "--global_lease 500 --oracle_type one_diff --noconstant ",
      test_hooks, num_iterations, seed);
}

BOOST_AUTO_TEST_CASE(exc_incl_unit_test)
{
  using namespace VW::reductions::automl;

  std::map<VW::namespace_index, uint64_t> ns_counter{{'A', 5}, {'B', 4}, {'C', 3}};

  interaction_vec_t interactions;

  ns_based_config test_config_interaction(set_ns_list_t{{'A', 'A'}, {'A', 'B'}}, 4000, config_type::Interaction);
  ns_based_config::apply_config_to_interactions(false, ns_counter, "", test_config_interaction, interactions);

  const set_ns_list_t expected{{'A', 'A'}, {'A', 'B'}};
  BOOST_CHECK_EQUAL(interactions.size(), 2);
  BOOST_CHECK_EQUAL_COLLECTIONS(interactions.begin(), interactions.end(), expected.begin(), expected.end());

  ns_based_config test_config_exclusion(set_ns_list_t{{'A', 'A'}, {'A', 'B'}}, 4000, config_type::Exclusion);
  ns_based_config::apply_config_to_interactions(false, ns_counter, "quadratic", test_config_exclusion, interactions);

  const set_ns_list_t expected2{{'A', 'C'}, {'B', 'B'}, {'B', 'C'}, {'C', 'C'}};
  BOOST_CHECK_EQUAL(interactions.size(), 4);
  BOOST_CHECK_EQUAL_COLLECTIONS(interactions.begin(), interactions.end(), expected2.begin(), expected2.end());
}
