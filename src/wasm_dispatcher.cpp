#include "wasm_dispatcher.hpp"

#include "basic_callbacks.hpp"
#include "state_history_connection.hpp"
#include "state_history_rocksdb.hpp"

namespace history_tools {

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct run_state : history_tools::wasm_state<backend_t>, state_history::rdb::db_view_state {
    std::vector<char>    args;
    abieos::input_buffer bin;

    run_state(const char* wasm, eosio::vm::wasm_allocator& wa, backend_t& backend, state_history::rdb::database& db, std::vector<char> args)
        : wasm_state{wa, backend}
        , db_view_state{db}
        , args{args} {}
};

struct callbacks : history_tools::basic_callbacks<callbacks>, state_history::rdb::db_callbacks<callbacks> {
    run_state& state;

    callbacks(run_state& state)
        : state{state} {}

    void get_bin(uint32_t cb_alloc_data, uint32_t cb_alloc) { set_data(cb_alloc_data, cb_alloc, state.bin); }
    void get_args(uint32_t cb_alloc_data, uint32_t cb_alloc) { set_data(cb_alloc_data, cb_alloc, state.args); }
}; // callbacks

void wasm_dispatcher::register_callbacks() {
    history_tools::basic_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    state_history::rdb::db_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();

    rhf_t::add<callbacks, &callbacks::get_bin, eosio::vm::wasm_allocator>("env", "get_bin");
    rhf_t::add<callbacks, &callbacks::get_args, eosio::vm::wasm_allocator>("env", "get_args");
}

struct ship_connection_state : state_history::connection_callbacks, std::enable_shared_from_this<ship_connection_state> {
    std::shared_ptr<run_state>                 state; // xxxxxx
    std::shared_ptr<state_history::connection> connection;

    ship_connection_state(const std::shared_ptr<run_state>& state)
        : state(state) {}

    void start(boost::asio::io_context& ioc) {
        state_history::connection_config config{"127.0.0.1", "8080"};
        connection = std::make_shared<state_history::connection>(ioc, config, shared_from_this());
        connection->connect();
    }

    void received_abi(std::string_view abi) override {
        ilog("received_abi");
        connection->send(state_history::get_status_request_v0{});
    }

    bool received(state_history::get_status_result_v0& status, abieos::input_buffer bin) override {
        ilog("received status");
        connection->request_blocks(status, 0, {});
        return true;
    }

    bool received(state_history::get_blocks_result_v0& result, abieos::input_buffer bin) override {
        ilog("received block ${n}", ("n", result.this_block ? result.this_block->block_num : -1));
        callbacks cb{*state};
        state->reset();
        state->bin = bin;
        state->backend.initialize(&cb);
        // backend(&cb, "env", "initialize"); // todo: needs change to eosio-cpp
        state->backend(&cb, "env", "start", 0);
        state->write_and_reset();
        return true;
    }

    void closed(bool retry) override { ilog("closed"); }
};

struct wasm_dispatcher_impl {
    std::mutex                                               mutex;
    std::map<abieos::name, state_history::connection_config> ship_connections;

    void add_ship_connection(abieos::name name, std::string host, std::string port);

    void create(                                           //
        abieos::name              name,                    //
        std::string               wasm,                    //
        bool                      privileged,              //
        std::vector<std::string>  args,                    //
        std::vector<abieos::name> database_write_contexts, //
        std::vector<abieos::name> ships,                   //
        std::vector<std::string>  api_handlers);
};

void wasm_dispatcher_impl::add_ship_connection(abieos::name name, std::string host, std::string port) {
    std::lock_guard<std::mutex> lock{mutex};
    ship_connections[name] = {host, port};
}

void wasm_dispatcher_impl::create(                     //
    abieos::name              name,                    //
    std::string               wasm,                    //
    bool                      privileged,              //
    std::vector<std::string>  args,                    //
    std::vector<abieos::name> database_write_contexts, //
    std::vector<abieos::name> ships,                   //
    std::vector<std::string>  api_handlers) {

    std::lock_guard<std::mutex>  lock{mutex};
    eosio::vm::wasm_allocator    wa;
    auto                         code = backend_t::read_wasm(wasm);
    backend_t                    backend(code);
    state_history::rdb::database db{"db.rocksdb", {}, {}, true};
    auto                         state = std::make_shared<run_state>(wasm.c_str(), wa, backend, db, abieos::native_to_bin(args));
    // todo: state: drop shared_ptr?
    backend.set_wasm_allocator(&wa);

    rhf_t::resolve(backend.get_module());

    boost::asio::io_context          ioc;
    state_history::connection_config config{"127.0.0.1", "8080"};
    auto                             ship_state = std::make_shared<ship_connection_state>(state);
    ship_state->start(ioc);
    ioc.run();
}

wasm_dispatcher::wasm_dispatcher()
    : my{std::make_shared<wasm_dispatcher_impl>()} {}

void wasm_dispatcher::add_ship_connection(abieos::name name, std::string host, std::string port) {
    my->add_ship_connection(name, host, port);
}

void wasm_dispatcher::create(                          //
    abieos::name              name,                    //
    std::string               wasm,                    //
    bool                      privileged,              //
    std::vector<std::string>  args,                    //
    std::vector<abieos::name> database_write_contexts, //
    std::vector<abieos::name> ships,                   //
    std::vector<std::string>  api_handlers) {

    my->create(name, wasm, privileged, std::move(args), std::move(database_write_contexts), std::move(ships), std::move(api_handlers));
}

} // namespace history_tools
