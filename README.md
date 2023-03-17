# Description

This release contains the first release of EOSIO History-Tools `fill-pg` v1.0.0 and is compatible with EOSIO v2.1.0. All other tools included in the prior alpha releases of History-Tools are now deprecated.

## docker image

## 1. clone source code
`git clone https://github.com/armoniax/history-tools.git && cd history_tools && git submodule update --init --recursive`

### 2. build local docker image
`docker build -t armoniax/amcsync2pg .`

### 3. push to docker hub
`docker push armoniax/amcsync2pg`

## fill-pg

`fill-pg` fills postgresql with data from nodeos's State History Plugin. It provides nearly all
data that applications which monitor the chain need. It provides the following:

* Header information from each block
* Transaction and action traces, including inline actions and deferred transactions
* Contract table history, at the block level
* Tables which track the history of chain state, including
  * Accounts, including permissions and linkauths
  * Account resource limits and usage
  * Contract code
  * Contract ABIs
  * Consensus parameters
  * Activated consensus upgrades

### PostgreSQL table schema changes

This release completely rewrites the SHiP protocol to SQL conversion code so that the database tables would directly align with the data structures defined in the SHiP protocol. This also changes the table schema used by previous releases.

Here are the basic rules for the conversion:

  - Nested SHiP `struct` types with more than one field are mapped to SQL custom types.
  - Nested SHiP `vector` types are mapped to SQL arrays.
  - SHiP `variant` types are mapped to a SQL type or table containing the union fields of their constituent types.  

Consequently, instead of having their own tables in previous releases, `action_trace`, `action_trace_ram_delta`, `action_trace_auth_sequence` and `action_trace_authorization` are arrays nested inside `transaction_trace` table or `action_trace` type. The SQL `UNNEST` operator can be used to flatten arrays into tables for query. 

The current list of tables created by  `fill-pg` are:
  - account
  - account_metadata
  - block_info  
  - code                      
  - contract_index_double
  - contract_index_long_double
  - contract_index128
  - contract_index256
  - contract_index64 
  - contract_row
  - contract_table
  - fill_status
  - generated_transaction
  - global_property
  - key_value
  - permission
  - permission_link
  - protocol_state
  - received_block  
  - resource_limits
  - resource_limits_config
  - resource_limits_state
  - resource_usage
  - transaction_trace 

## Data migration from prior releases

This release upgrades `fill-pg` to support `nodeos` v2.1.0. The remaining tools are still disabled and have not been upgraded. The schema used by
`fill-pg` has changed. At present no data migration tool is available, so data may be manually migrated, regenerated by replaying
from the desired block number, or exist side by side in two schemas, with older blocks in the v0.3.0 schema and newer blocks in the new schema.  Use of the `--pg-schema` option will facilitate the transition. 

Full details of the differences can be found via diff of plain text backups of each schema using `pg_dump`, not included here
for brevity.


## Additional details

`fill-pg` keeps action data and contract table data in its original binary form. Future versions
may optionally support converting this to JSON.

To conserve space, `fill-pg` doesn't store blocks in postgresql. The majority of apps
don't need the blocks since:

* Blocks don't include inline actions and only include some deferred transactions. Most
  applications need to handle these, so they should examine the traces instead. e.g. many transfers
  live in the inline actions and deferred transactions that blocks exclude.
* Most apps don't verify block signatures. If they do, then they should connect directly to
  nodeos's State History Plugin to get the necessary data. Note that contrary to
  popular belief, the data returned by the `/v1/get_block` RPC API is insufficient for
  signature verification since it uses a lossy JSON conversion.
* Most apps which currently use the `/v1/get_block` RPC API (e.g. `eosjs`) only need a tiny
  subset of the data within each block; `fill-pg` stores this data. There are apps which use
  `/v1/get_block` incorrectly since their authors didn't realize the blocks miss
  critical data that their applications need.

`fill-pg` supports both full history and partial history (`trim` option). This allows users
to make their own tradeoffs. They can choose between supporting queries covering the entire
history of the chain, or save space by only covering recent history.


### SHiP protocol changes ([#10268](https://github.com/EOSIO/eos/pull/10268))

SHiP protocol has been changed to allow a client to request the block_header only instead of the entire block. `fill-pg` has been updated to utilize this feature when the `nodeos` it connects to supports it. 

## Deprecation and Removal Notices
`fill-rocksdb`, `wasm-ql-rocksdb`, `combo-rocksdb`,`wasm-ql-pg`, and `history-tools` have been deprecated and disabled as of this v1.0.0 release.


## Getting Started

You can use the docker-compose file to see how fill-pg interacts with nodeos and postgresql.
For example, if you want to run fill-pg with some snapshot and setup one peer address, create a
.env file such as:

```
SNAPSHOT_FILE=/root/history-tools/snapshot-2021-03-05-10-eos-v4@0171570460.bin
PEER_ADDR=peer.main.alohaeos.com:9876
```

Then execute the command:

```
docker-compose up
```

And you will start seeing logs from the 3 containers, showing how they interact between each other.

Further customization can be achieved with other environment variables for example to set an specific
branch/commit for nodeos or history-tools, such as:

```
DOCKER_EOSIO_TAG=develop
DOCKER_HISTORY_TOOLS_TAG=935650a6fb9ca596affe0a3c42e6a1966675061d
```

You can also modify the provided docker-compose.yaml so that, for example, it takes more p2p peer addresses,
for example:

```
       ...
       - --p2p-peer-address=peer.main.alohaeos.com:9876
       - --p2p-peer-address=p2p.eosflare.io:9876
       - --p2p-peer-address=p2p.eosargentina.io:5222
       - --p2p-peer-address=eos-bp.index.pro:9876
       - --p2p-peer-address=eosbp-0.atticlab.net:9876
       - --p2p-peer-address=mainnet.eosarabia.net:3571
       ...
```


## Contributing

[Contributing Guide](./CONTRIBUTING.md)

[Code of Conduct](./CONTRIBUTING.md#conduct)

## License

[MIT](./LICENSE)

## Important

See [LICENSE](LICENSE) for copyright and license terms.

All repositories and other materials are provided subject to the terms of this [IMPORTANT](important.md) notice and you must familiarize yourself with its terms.  The notice contains important information, limitations and restrictions relating to our software, publications, trademarks, third-party resources, and forward-looking statements.  By accessing any of our repositories and other materials, you accept and agree to the terms of the notice.
