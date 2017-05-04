# socket99 Changes By Release

## v 0.2.2 - 2017-05-04

### API Changes

None.

### Other Improvements

Corrected the `_POSIX_C_SOURCE` definition in the Makefile.

Make the port for the tests configurable.


## v 0.2.1 - 2017-05-03

### API Changes

None.

### Other Improvements

Bugfix: Remove memory leak on error code paths after getaddrinfo.
(Thanks @robert-warmka.)

Ensure directories exist before installing files. (Thanks @dgsb.)

Increased warning level and addressed warnings.


## v 0.2.0 - 2015-01-02

### API Changes

Add `socket99_snprintf`, to construct nicer error messages.

Add `setsockopt(2)` support.

### Other Improvements

Bugfix: Record when connect was the cause of connection failures.

Reduced sleep times during test.

Add install, uninstall targets to makefile.


## v 0.1.0 - 2014-07-25

Initial public release.
