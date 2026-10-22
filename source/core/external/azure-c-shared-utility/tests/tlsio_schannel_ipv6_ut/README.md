# Schannel IPv6 certificate tests

These tests import a generated test CA into the Windows **CurrentUser Root** store, then remove that exact certificate in suite cleanup. Run them only under a dedicated, disposable Windows test user or runner with its own certificate store. Do not enable them on a shared developer account or a persistent CI agent.

Configure with `run_unittests=ON`, `use_schannel=ON`, `use_openssl=ON` (OpenSSL supplies the in-memory TLS test server), and `run_isolated_schannel_ipv6_tests=ON`. Build and execute `tlsio_schannel_ipv6_ut_exe` on the disposable runner. The tests do not disable Schannel peer or hostname verification.
