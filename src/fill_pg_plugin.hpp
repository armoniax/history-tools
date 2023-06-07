// copyright defined in LICENSE.txt

#pragma once
#include "fill_plugin.hpp"
#include "pg_plugin.hpp"
#include <appbase/application.hpp>

// TODO: move backup_block_extension to ship_protocol.hpp

using block_id_type = eosio::checksum256;

enum header_extension_type {
    feature_activation_type                 = 0,
    schedule_change_extension_type,
    schedule_change_extension_v2_type,
    backup_block_extension_type
};

struct previous_backup_info {
    eosio::checksum256  id;                    // previous backup block id
    eosio::name         producer;              // previous backup block producer
    uint32_t            contribution  = 0;     // need to boost 10000
};

struct backup_block_extension {

    static constexpr uint16_t extension_id() { return header_extension_type::backup_block_extension_type; }
    static constexpr bool     enforce_unique() { return true; }

    bool                is_backup                  = false; // is backup block
    std::optional<previous_backup_info>             previous_backup = {};
};

EOSIO_REFLECT(previous_backup_info, id, producer, contribution);
EOSIO_REFLECT(backup_block_extension, is_backup, previous_backup);

class fill_pg_plugin : public appbase::plugin<fill_pg_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES((pg_plugin)(fill_plugin))

    fill_pg_plugin();
    virtual ~fill_pg_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

  private:
    std::shared_ptr<struct fill_postgresql_plugin_impl> my;
};

template<std::size_t I = 0, typename FuncT, typename... Tp>
inline typename std::enable_if<I == sizeof...(Tp), void>::type
variant_for_each(std::variant<Tp...>, FuncT)
{ }

template<std::size_t I = 0, typename FuncT, typename... Tp>
inline typename std::enable_if<I < sizeof...(Tp), void>::type
variant_for_each(std::variant<Tp...> t, FuncT f)
{
    if (I == t.index())
        f(I, std::get<I>(t));
    else
        f(I, std::variant_alternative_t<I, decltype(t)>());
    variant_for_each<I + 1, FuncT, Tp...>(t, f);
}
