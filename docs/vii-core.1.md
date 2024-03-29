% vii-core(1)
% vii Development Foundation
%

# NAME

vii-core - Core daemon for vii payment network

# SYNOPSYS

vii-core [OPTIONS]

# DESCRIPTION

VII is a decentralized, federated peer-to-peer network that allows
people to send payments in any asset anywhere in the world
instantaneously, and with minimal fee. `vii-core` is the core
component of this network. `vii-core` is a C++ implementation of
the vii Consensus Protocol configured to construct a chain of
ledgers that are guaranteed to be in agreement across all the
participating nodes at all times.

## Configuration file

In most modes of operation, vii-core requires a configuration
file.  By default, it looks for a file called `vii-core.cfg` in
the current working directory, but this default can be changed by the
`--conf` command-line option.  The configuration file is in TOML
syntax.  The full set of supported directives can be found in
`%prefix%/share/doc/vii-core.cfg`.

%commands%

# EXAMPLES

See `%prefix%/share/doc/*.cfg` for some example vii-core
configuration files

# FILES

vii-core.cfg
:   Configuration file (in current working directory by default)

# SEE ALSO

<https://www.vii.vip/developers/vii-core/software/admin.html>
:   vii-core administration guide

<https://www.vii.vip>
:   Home page of vii development foundation

# BUGS

Please report bugs using the github issue tracker:\
<https://github.com/viichain/vii-core/issues>
