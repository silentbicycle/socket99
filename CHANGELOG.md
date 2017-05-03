# socket99 Changes By Release

## v 0.2.1 - TBD

### API Changes

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
