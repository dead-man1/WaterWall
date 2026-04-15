# TesterClient

`TesterClient` is a synthetic chain-head tunnel used to exercise Waterwall stream tunnels.

It waits `50 ms`, creates one line on each worker, waits for downstream establishment, sends a fixed sequence of
deterministic request chunks upstream, verifies a deterministic response sequence coming back downstream, and fails the
process on any mismatch or timeout.
