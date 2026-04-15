# TesterServer

`TesterServer` is a synthetic chain-end tunnel paired with `TesterClient`.

It validates the deterministic request sequence coming from upstream, then sends a deterministic response sequence back
downstream on the same line. Any size/order/data mismatch aborts the process.
