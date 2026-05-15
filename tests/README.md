# Tests

Firmware integration tests (not host unit tests). Each app sets `volatile` pass globals.

## Automated

```bash
make test-all
# or: python3 scripts/run_tests.py
```

| APP | Pass symbol | Expected |
|-----|-------------|----------|
| `test_scheduler` | `g_scheduler_test_pass` | 1 |
| `test_mutex` | `g_mutex_test_pass` | 1 |
| `test_priority_inheritance` | `g_pi_test_pass` | 1 |
| `test_msgqueue` | `g_msgqueue_test_pass` | 2 |
| `test_semaphore` | `g_semaphore_test_pass` | 1 |
| `test_timeout` | `g_timeout_test_pass` | 1 |

## Manual (GDB)

```bash
make APP=test_mutex debug
# connect to :1234, break after delay, print g_mutex_test_pass
```
